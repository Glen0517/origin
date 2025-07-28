#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dbus/dbus.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <pipewire/pipewire.h>
#include <spa/utils/result.h>
#include <spa/param/audio/format-utils.h>

#include "bluez_audio.h"

// 蓝牙音频设备结构体
struct bluez_audio_device {
    char address[18];          // 蓝牙地址 (AA:BB:CC:DD:EE:FF)
    char name[256];            // 设备名称
    enum device_type type;     // 设备类型
    enum connection_state state; // 连接状态
    uint16_t handle;           // 连接句柄
    int hci_socket;            // HCI套接字
    DBusConnection* dbus_conn; // D-Bus连接
    struct pw_stream* stream;  // PipeWire流
    struct spa_audio_info format; // 音频格式
    bool streaming;            // 是否正在流传输
    pthread_mutex_t mutex;     // 互斥锁
};

// 全局设备列表
static struct bluez_audio_device* devices[BLUEZ_MAX_DEVICES] = {NULL};
static int device_count = 0;
static pthread_mutex_t devices_mutex = PTHREAD_MUTEX_INITIALIZER;

// D-Bus回调函数原型
static DBusHandlerResult dbus_message_handler(DBusConnection* conn, DBusMessage* msg, void* user_data);

// 初始化蓝牙音频模块
int bluez_audio_init() {
    int ret = 0;
    DBusError err;

    // 初始化PipeWire
    pw_init(NULL, NULL);

    // 初始化D-Bus
    dbus_error_init(&err);
    DBusConnection* conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "D-Bus connection failed: %s\n", err.message);
        dbus_error_free(&err);
        return -1;
    }

    // 注册D-Bus消息处理函数
    dbus_connection_add_filter(conn, dbus_message_handler, NULL, NULL);

    // 请求名称
    const char* dbus_name = "org.bluez.AudioFramework";
    ret = dbus_bus_request_name(conn, dbus_name, DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "D-Bus name request failed: %s\n", err.message);
        dbus_error_free(&err);
        dbus_connection_unref(conn);
        return -1;
    }

    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        fprintf(stderr, "Could not get primary ownership of D-Bus name\n");
        dbus_connection_unref(conn);
        return -1;
    }

    // 初始化HCI套接字
    int hci_sock = hci_open_dev(hci_get_route(NULL));
    if (hci_sock < 0) {
        perror("Failed to open HCI socket");
        dbus_connection_unref(conn);
        return -1;
    }

    // 开始设备发现
    ret = bluez_start_discovery(hci_sock);
    if (ret < 0) {
        perror("Failed to start discovery");
        close(hci_sock);
        dbus_connection_unref(conn);
        return -1;
    }

    printf("BlueZ audio module initialized successfully\n");
    return 0;
}

// 开始蓝牙设备发现
int bluez_start_discovery(int hci_sock) {
    int ret;
    uint8_t scan_type = 0x01; // 主动扫描
    uint16_t interval = htobs(0x0010);
    uint16_t window = htobs(0x0010);
    uint8_t own_bdaddr_type = 0;
    uint8_t filter_policy = 0;

    // 设置扫描参数
    ret = hci_le_set_scan_parameters(hci_sock, scan_type, interval, window, own_bdaddr_type, filter_policy, 10000);
    if (ret < 0) {
        return ret;
    }

    // 启用扫描
    ret = hci_le_set_scan_enable(hci_sock, 0x01, 0, 10000);
    if (ret < 0) {
        return ret;
    }

    // 启动发现线程
    pthread_t discovery_thread;
    ret = pthread_create(&discovery_thread, NULL, discovery_thread_func, (void*)hci_sock);
    if (ret != 0) {
        perror("Failed to create discovery thread");
        return -1;
    }

    pthread_detach(discovery_thread);
    return 0;
}

// 设备发现线程函数
static void* discovery_thread_func(void* arg) {
    int hci_sock = (int)arg;
    uint8_t buf[HCI_MAX_EVENT_SIZE];
    struct hci_response_hdr* hdr;
    struct hci_ev_le_meta_event* meta;
    struct le_advertising_info* info;
    int len;

    printf("Starting Bluetooth device discovery...\n");

    while (1) {
        len = read(hci_sock, buf, sizeof(buf));
        if (len < 0) {
            perror("Read error");
            break;
        }

        hdr = (struct hci_response_hdr*)buf;
        if (hdr->evt != HCI_EV_LE_META_EVENT) {
            continue;
        }

        meta = (struct hci_ev_le_meta_event*)(buf + 1);
        if (meta->subevent != LE_ADVERTISING_REPORT) {
            continue;
        }

        info = (struct le_advertising_info*)(meta->data + 1);
        bluez_process_advertising_report(info);

        // 短暂休眠以降低CPU占用
        usleep(100000);
    }

    return NULL;
}

// 处理广告报告
static void bluez_process_advertising_report(struct le_advertising_info* info) {
    char addr[18];
    ba2str(&info->bdaddr, addr);

    // 检查是否是新设备
    bool is_new = true;
    pthread_mutex_lock(&devices_mutex);

    for (int i = 0; i < device_count; i++) {
        if (strcmp(devices[i]->address, addr) == 0) {
            is_new = false;
            break;
        }
    }

    if (is_new && device_count < BLUEZ_MAX_DEVICES) {
        // 解析设备名称
        char name[256] = {0};
        bluez_parse_device_name(info->data, info->length, name, sizeof(name));

        // 创建新设备
        struct bluez_audio_device* dev = malloc(sizeof(struct bluez_audio_device));
        if (dev) {
            memset(dev, 0, sizeof(struct bluez_audio_device));
            strcpy(dev->address, addr);
            strcpy(dev->name, name);
            dev->state = CONNECTION_STATE_DISCONNECTED;
            dev->hci_socket = hci_get_route(NULL);
            pthread_mutex_init(&dev->mutex, NULL);

            // 尝试确定设备类型
            dev->type = bluez_determine_device_type(info->data, info->length);

            devices[device_count++] = dev;
            printf("Discovered new Bluetooth device: %s (%s)\n", name, addr);

            // 发送设备发现信号
            bluez_emit_device_discovered(dev);
        }
    }

    pthread_mutex_unlock(&devices_mutex);
}

// 解析设备名称
static void bluez_parse_device_name(uint8_t* data, size_t length, char* name, size_t name_len) {
    size_t offset = 0;
    while (offset < length) {
        uint8_t field_length = data[offset];
        if (field_length == 0 || offset + field_length + 1 > length) {
            break;
        }

        uint8_t field_type = data[offset + 1];
        if (field_type == 0x09) { // 完整本地名称
            strncpy(name, (char*)(data + offset + 2), min(field_length, name_len - 1));
            name[min(field_length, name_len - 1)] = '\0';
            break;
        } else if (field_type == 0x08 && *name == '\0') { // 短本地名称
            strncpy(name, (char*)(data + offset + 2), min(field_length, name_len - 1));
            name[min(field_length, name_len - 1)] = '\0';
        }

        offset += field_length + 1;
    }

    // 如果没有找到名称，使用地址作为名称
    if (*name == '\0') {
        strncpy(name, (char*)data, name_len - 1);
        name[name_len - 1] = '\0';
    }
}

// 确定设备类型
static enum device_type bluez_determine_device_type(uint8_t* data, size_t length) {
    // 简化实现，实际应解析UUID和服务类
    return DEVICE_TYPE_HEADPHONES;
}

// 连接到蓝牙音频设备
int bluez_audio_connect(const char* address) {
    pthread_mutex_lock(&devices_mutex);

    struct bluez_audio_device* dev = NULL;
    for (int i = 0; i < device_count; i++) {
        if (strcmp(devices[i]->address, address) == 0) {
            dev = devices[i];
            break;
        }
    }

    if (!dev) {
        pthread_mutex_unlock(&devices_mutex);
        fprintf(stderr, "Device not found: %s\n", address);
        return -1;
    }

    pthread_mutex_lock(&dev->mutex);
    pthread_mutex_unlock(&devices_mutex);

    if (dev->state == CONNECTION_STATE_CONNECTED) {
        pthread_mutex_unlock(&dev->mutex);
        return 0;
    }

    // 连接到设备
    int ret = bluez_connect_to_device(dev);
    pthread_mutex_unlock(&dev->mutex);
    return ret;
}

// 实际连接设备的函数
static int bluez_connect_to_device(struct bluez_audio_device* dev) {
    // 实现D-Bus调用连接到BlueZ设备
    // 简化实现，实际应使用bluez的D-Bus API

    dev->state = CONNECTION_STATE_CONNECTING;
    bluez_emit_connection_state_changed(dev);

    // 模拟连接延迟
    sleep(2);

    // 创建PipeWire流
    dev->stream = bluez_create_pipewire_stream(dev);
    if (!dev->stream) {
        dev->state = CONNECTION_STATE_DISCONNECTED;
        bluez_emit_connection_state_changed(dev);
        return -1;
    }

    dev->state = CONNECTION_STATE_CONNECTED;
    dev->streaming = true;
    bluez_emit_connection_state_changed(dev);

    printf("Connected to Bluetooth audio device: %s\n", dev->name);
    return 0;
}

// 创建PipeWire流
static struct pw_stream* bluez_create_pipewire_stream(struct bluez_audio_device* dev) {
    struct pw_properties* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_DEVICE, dev->address,
        PW_KEY_STREAM_NAME, "BlueZ Audio Stream",
        NULL
    );

    struct pw_stream* stream = pw_stream_new_simple(
        pw_context_new(NULL, NULL),
        "bluez-audio-stream",
        props,
        NULL
    );

    if (!stream) {
        pw_properties_free(props);
        return NULL;
    }

    // 设置流参数
    struct spa_audio_info format = {
        .format = SPA_AUDIO_FORMAT_S16_LE,
        .rate = 48000,
        .channels = 2,
        .position = {SPA_AUDIO_CHANNEL_FL, SPA_AUDIO_CHANNEL_FR}
    };

    dev->format = format;

    // 连接流
    pw_stream_connect(stream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS,
        NULL,
        0);

    return stream;
}

// 断开蓝牙音频设备连接
int bluez_audio_disconnect(const char* address) {
    pthread_mutex_lock(&devices_mutex);

    struct bluez_audio_device* dev = NULL;
    for (int i = 0; i < device_count; i++) {
        if (strcmp(devices[i]->address, address) == 0) {
            dev = devices[i];
            break;
        }
    }

    if (!dev) {
        pthread_mutex_unlock(&devices_mutex);
        return -1;
    }

    pthread_mutex_lock(&dev->mutex);
    pthread_mutex_unlock(&devices_mutex);

    if (dev->state == CONNECTION_STATE_DISCONNECTED) {
        pthread_mutex_unlock(&dev->mutex);
        return 0;
    }

    // 停止流传输
    dev->streaming = false;

    // 断开PipeWire流
    if (dev->stream) {
        pw_stream_disconnect(dev->stream);
        pw_stream_destroy(dev->stream);
        dev->stream = NULL;
    }

    // 发送D-Bus断开连接命令
    // ...

    dev->state = CONNECTION_STATE_DISCONNECTED;
    pthread_mutex_unlock(&dev->mutex);

    bluez_emit_connection_state_changed(dev);
    printf("Disconnected from Bluetooth audio device: %s\n", dev->name);
    return 0;
}

// D-Bus消息处理函数
static DBusHandlerResult dbus_message_handler(DBusConnection* conn, DBusMessage* msg, void* user_data) {
    // 实现D-Bus消息处理
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

// 发送设备发现信号
static void bluez_emit_device_discovered(struct bluez_audio_device* dev) {
    // 实现信号发射
}

// 发送连接状态变化信号
static void bluez_emit_connection_state_changed(struct bluez_audio_device* dev) {
    // 实现信号发射
}

// 销毁蓝牙音频模块
void bluez_audio_destroy() {
    pthread_mutex_lock(&devices_mutex);

    for (int i = 0; i < device_count; i++) {
        if (devices[i]->state == CONNECTION_STATE_CONNECTED) {
            bluez_audio_disconnect(devices[i]->address);
        }
        pthread_mutex_destroy(&devices[i]->mutex);
        free(devices[i]);
    }

    device_count = 0;
    pthread_mutex_unlock(&devices_mutex);
    pthread_mutex_destroy(&devices_mutex);

    printf("BlueZ audio module destroyed\n");
}

// 获取设备列表
int bluez_audio_get_devices(struct bluez_audio_device** devs, size_t max_devices) {
    pthread_mutex_lock(&devices_mutex);
    int count = min(device_count, max_devices);

    for (int i = 0; i < count; i++) {
        devs[i] = devices[i];
    }

    pthread_mutex_unlock(&devices_mutex);
    return count;
}

// 写入音频数据到蓝牙设备
int bluez_audio_write(struct bluez_audio_device* dev, const void* data, size_t size) {
    if (!dev || !data || size == 0 || dev->state != CONNECTION_STATE_CONNECTED || !dev->streaming) {
        return -1;
    }

    // 将音频数据写入PipeWire流
    // 简化实现
    return size;
}