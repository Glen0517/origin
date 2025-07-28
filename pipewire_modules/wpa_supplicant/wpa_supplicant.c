#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <jansson.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>

#include "wpa_supplicant.h"
#include "../../include/dbus_utils.h"

// 前向声明
static void* wpa_supplicant_worker_thread(void* data);
static int wpa_supplicant_connect_ctrl_interface(WpaSupplicantService* service);
static void wpa_supplicant_close_ctrl_interface(WpaSupplicantService* service);
static int wpa_supplicant_send_command(WpaSupplicantService* service, const char* cmd, char* response, size_t resp_len);
static void wpa_supplicant_set_state(WpaSupplicantService* service, WpaState state);
static void wpa_supplicant_parse_scan_results(WpaSupplicantService* service, const char* results);

// 创建WPA Supplicant服务
WpaSupplicantService* wpa_supplicant_create(struct pw_context* context, const WpaConfig* config) {
    if (!context || !config) {
        fprintf(stderr, "Invalid parameters for wpa_supplicant_create\n");
        return NULL;
    }

    WpaSupplicantService* service = calloc(1, sizeof(WpaSupplicantService));
    if (!service) {
        fprintf(stderr, "Failed to allocate memory for WpaSupplicantService\n");
        return NULL;
    }

    // 初始化D-Bus
    if (!dbus_initialize()) {
        fprintf(stderr, "Failed to initialize D-Bus connection for wpa_supplicant\n");
        // 继续初始化但记录错误
    }

    // 初始化互斥锁
    pthread_mutex_init(&service->mutex, NULL);

    // 设置上下文和配置
    service->context = context;
    service->config = *config;
    service->state = WPA_STATE_DISCONNECTED;
    service->running = false;
    service->ctrl_fd = -1;
    service->wpa_handle = NULL;

    // 设置默认配置
    if (!service->config.interface[0]) {
        strncpy(service->config.interface, "wlan0", sizeof(service->config.interface)-1);
    }
    if (service->config.scan_interval == 0) {
        service->config.scan_interval = 30;
    }
    if (service->config.max_retries == 0) {
        service->config.max_retries = 3;
    }

    return service;
}

// 销毁WPA Supplicant服务
void wpa_supplicant_destroy(WpaSupplicantService* service) {
    if (!service) return;

    wpa_supplicant_stop(service);

    // 关闭控制接口
    wpa_supplicant_close_ctrl_interface(service);

    // 清理互斥锁
    pthread_mutex_destroy(&service->mutex);

    // 清理D-Bus
    dbus_cleanup();

    free(service);
}

// 启动WPA Supplicant服务
int wpa_supplicant_start(WpaSupplicantService* service) {
    if (!service || service->running) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 连接到wpa_supplicant控制接口
    if (wpa_supplicant_connect_ctrl_interface(service) < 0) {
        fprintf(stderr, "Failed to connect to wpa_supplicant control interface\n");
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 设置运行标志
    service->running = true;

    // 创建工作线程
    if (pthread_create(&service->thread, NULL, wpa_supplicant_worker_thread, service) != 0) {
        fprintf(stderr, "Failed to create wpa_supplicant worker thread\n");
        service->running = false;
        wpa_supplicant_close_ctrl_interface(service);
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 停止WPA Supplicant服务
void wpa_supplicant_stop(WpaSupplicantService* service) {
    if (!service || !service->running) {
        return;
    }

    pthread_mutex_lock(&service->mutex);
    service->running = false;
    pthread_mutex_unlock(&service->mutex);

    // 等待线程结束
    pthread_join(service->thread, NULL);

    // 断开当前连接
    wpa_supplicant_disconnect(service);

    // 设置状态为未连接
    wpa_supplicant_set_state(service, WPA_STATE_DISCONNECTED);
}

// 连接到WiFi网络
int wpa_supplicant_connect(WpaSupplicantService* service, const char* ssid, const char* password, WpaSecurity security) {
    if (!service || !ssid || service->state != WPA_STATE_DISCONNECTED) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 设置状态为认证中
    wpa_supplicant_set_state(service, WPA_STATE_AUTHENTICATING);

    // 在实际实现中，这里应该通过wpa_supplicant控制接口发送连接命令
    // 简化处理，直接模拟连接过程
    char cmd[512];
    char response[1024];

    // 构建网络配置命令
    snprintf(cmd, sizeof(cmd), "ADD_NETWORK\n");
    if (wpa_supplicant_send_command(service, cmd, response, sizeof(response)) <= 0) {
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    int network_id = atoi(response);
    if (network_id < 0) {
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 设置SSID
    snprintf(cmd, sizeof(cmd), "SET_NETWORK %d ssid \"%s\"\n", network_id, ssid);
    wpa_supplicant_send_command(service, cmd, response, sizeof(response));

    // 设置安全类型和密码
    if (security == WPA_SECURITY_WPA2 || security == WPA_SECURITY_WPA3) {
        snprintf(cmd, sizeof(cmd), "SET_NETWORK %d key_mgmt WPA-PSK\n", network_id);
        wpa_supplicant_send_command(service, cmd, response, sizeof(response));
        snprintf(cmd, sizeof(cmd), "SET_NETWORK %d psk \"%s\"\n", network_id, password);
        wpa_supplicant_send_command(service, cmd, response, sizeof(response));
    } else if (security == WPA_SECURITY_WEP) {
        snprintf(cmd, sizeof(cmd), "SET_NETWORK %d key_mgmt NONE\n", network_id);
        wpa_supplicant_send_command(service, cmd, response, sizeof(response));
        snprintf(cmd, sizeof(cmd), "SET_NETWORK %d wep_key0 \"%s\"\n", network_id, password);
        wpa_supplicant_send_command(service, cmd, response, sizeof(response));
    }

    // 选择并启用网络
    snprintf(cmd, sizeof(cmd), "SELECT_NETWORK %d\n", network_id);
    wpa_supplicant_send_command(service, cmd, response, sizeof(response));

    // 模拟连接过程
    wpa_supplicant_set_state(service, WPA_STATE_ASSOCIATING);
    usleep(1000000); // 模拟延迟
    wpa_supplicant_set_state(service, WPA_STATE_ASSOCIATED);
    usleep(1000000); // 模拟延迟

    // 更新会话信息
    strncpy(service->session.current_network.ssid, ssid, sizeof(service->session.current_network.ssid)-1);
    service->session.current_network.security = security;
    service->session.current_network.signal_strength = 80; // 模拟信号强度
    service->session.current_network.frequency = 2437; // 模拟频率(2.4GHz频道6)
    strncpy(service->session.current_network.bssid, "aa:bb:cc:dd:ee:ff", sizeof(service->session.current_network.bssid)-1);
    service->session.connection_time = time(NULL);
    strncpy(service->session.ip_address, "192.168.1.101", sizeof(service->session.ip_address)-1);
    strncpy(service->session.gateway, "192.168.1.1", sizeof(service->session.gateway)-1);
    strncpy(service->session.dns_servers, "8.8.8.8,8.8.4.4", sizeof(service->session.dns_servers)-1);

    // 设置为已连接状态
    wpa_supplicant_set_state(service, WPA_STATE_CONNECTED);

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 断开WiFi连接
int wpa_supplicant_disconnect(WpaSupplicantService* service) {
    if (!service || service->state == WPA_STATE_DISCONNECTED) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 发送断开连接命令
    if (service->ctrl_fd >= 0) {
        char response[1024];
        wpa_supplicant_send_command(service, "DISCONNECT\n", response, sizeof(response));
    }

    // 重置会话信息
    memset(&service->session.current_network, 0, sizeof(WpaNetwork));
    memset(&service->session.ip_address, 0, sizeof(service->session.ip_address));
    memset(&service->session.gateway, 0, sizeof(service->session.gateway));
    memset(&service->session.dns_servers, 0, sizeof(service->session.dns_servers));

    // 设置状态为未连接
    wpa_supplicant_set_state(service, WPA_STATE_DISCONNECTED);

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 扫描WiFi网络
int wpa_supplicant_scan_networks(WpaSupplicantService* service) {
    if (!service || service->state != WPA_STATE_DISCONNECTED) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 设置状态为扫描中
    wpa_supplicant_set_state(service, WPA_STATE_SCANNING);

    // 发送扫描命令
    char response[4096];
    if (wpa_supplicant_send_command(service, "SCAN\n", response, sizeof(response)) <= 0) {
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 等待扫描完成
    usleep(3000000); // 3秒延迟

    // 获取扫描结果
    if (wpa_supplicant_send_command(service, "SCAN_RESULTS\n", response, sizeof(response)) <= 0) {
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 解析扫描结果
    wpa_supplicant_parse_scan_results(service, response);

    // 恢复未连接状态
    wpa_supplicant_set_state(service, WPA_STATE_DISCONNECTED);

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 获取当前状态
WpaState wpa_supplicant_get_state(WpaSupplicantService* service) {
    if (!service) {
        return WPA_STATE_ERROR;
    }

    WpaState state;
    pthread_mutex_lock(&service->mutex);
    state = service->state;
    pthread_mutex_unlock(&service->mutex);
    return state;
}

// 获取会话信息
const WpaSession* wpa_supplicant_get_session(WpaSupplicantService* service) {
    if (!service || service->state != WPA_STATE_CONNECTED) {
        return NULL;
    }

    return &service->session;
}

// 获取错误信息
const char* wpa_supplicant_get_error(WpaSupplicantService* service) {
    if (!service) {
        return "Invalid service pointer";
    }

    return service->error_msg;
}

// 设置状态并发送D-Bus信号
static void wpa_supplicant_set_state(WpaSupplicantService* service, WpaState state) {
    if (!service) return;

    pthread_mutex_lock(&service->mutex);
    WpaState old_state = service->state;
    service->state = state;
    pthread_mutex_unlock(&service->mutex);

    // 只有状态实际改变时才发送信号
    if (old_state != state) {
        // 创建JSON payload
        json_t* details = json_object();
        json_object_set_new(details, "old_state", json_integer(old_state));
        json_object_set_new(details, "new_state", json_integer(state));
        json_object_set_new(details, "ssid", json_string(service->session.current_network.ssid));
        json_object_set_new(details, "timestamp", json_integer(time(NULL)));

        const char* json_str = json_dumps(details, JSON_COMPACT);
        if (json_str) {
            // 发送D-Bus信号
            dbus_emit_signal("com.realtimeaudio.WpaSupplicant", DBUS_SIGNAL_TYPE_STATE_CHANGED, json_str);
            free((void*)json_str);
        }
        json_decref(details);

        // 日志记录
        printf("wpa_supplicant state changed from %d to %d\n", old_state, state);
    }
}

// 工作线程函数
static void* wpa_supplicant_worker_thread(void* data) {
    WpaSupplicantService* service = (WpaSupplicantService*)data;
    time_t last_scan = 0;

    while (1) {
        pthread_mutex_lock(&service->mutex);
        bool running = service->running;
        pthread_mutex_unlock(&service->mutex);

        if (!running) {
            break;
        }

        // 自动扫描网络（如果启用）
        if (service->config.auto_connect && service->state == WPA_STATE_DISCONNECTED) {
            time_t now = time(NULL);
            if (now - last_scan >= service->config.scan_interval) {
                wpa_supplicant_scan_networks(service);
                last_scan = now;
            }
        }

        // 检查连接状态
        if (service->state == WPA_STATE_CONNECTED) {
            // 在实际实现中，这里应该定期检查连接状态
        }

        sleep(1);
    }

    return NULL;
}

// 连接到wpa_supplicant控制接口
static int wpa_supplicant_connect_ctrl_interface(WpaSupplicantService* service) {
    struct sockaddr_un addr;
    char ctrl_path[256];

    // 创建Unix域套接字
    service->ctrl_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (service->ctrl_fd < 0) {
        snprintf(service->error_msg, sizeof(service->error_msg), "socket error: %s", strerror(errno));
        return -1;
    }

    // 设置控制接口路径
    snprintf(ctrl_path, sizeof(ctrl_path), "/var/run/wpa_supplicant/%s", service->config.interface);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ctrl_path, sizeof(addr.sun_path)-1);

    // 连接到控制接口
    if (connect(service->ctrl_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        snprintf(service->error_msg, sizeof(service->error_msg), "connect error: %s", strerror(errno));
        close(service->ctrl_fd);
        service->ctrl_fd = -1;
        return -1;
    }

    return 0;
}

// 关闭控制接口
static void wpa_supplicant_close_ctrl_interface(WpaSupplicantService* service) {
    if (service->ctrl_fd >= 0) {
        close(service->ctrl_fd);
        service->ctrl_fd = -1;
    }
}

// 发送命令到wpa_supplicant
static int wpa_supplicant_send_command(WpaSupplicantService* service, const char* cmd, char* response, size_t resp_len) {
    if (!service || service->ctrl_fd < 0 || !cmd || !response || resp_len == 0) {
        return -1;
    }

    // 发送命令
    ssize_t bytes_sent = send(service->ctrl_fd, cmd, strlen(cmd), 0);
    if (bytes_sent < 0) {
        snprintf(service->error_msg, sizeof(service->error_msg), "send error: %s", strerror(errno));
        return -1;
    }

    // 读取响应
    ssize_t bytes_read = recv(service->ctrl_fd, response, resp_len-1, 0);
    if (bytes_read < 0) {
        snprintf(service->error_msg, sizeof(service->error_msg), "recv error: %s", strerror(errno));
        return -1;
    }

    response[bytes_read] = '\0';
    return bytes_read;
}

// 解析扫描结果
static void wpa_supplicant_parse_scan_results(WpaSupplicantService* service, const char* results) {
    if (!service || !results) return;

    // 在实际实现中，这里应该解析扫描结果并存储网络列表
    // 简化处理，仅记录日志
    printf("Scan results:\n%s\n", results);

    // 创建扫描结果D-Bus信号
    json_t* details = json_object();
    json_object_set_new(details, "interface", json_string(service->config.interface));
    json_object_set_new(details, "scan_results", json_string(results));
    json_object_set_new(details, "timestamp", json_integer(time(NULL)));

    const char* json_str = json_dumps(details, JSON_COMPACT);
    if (json_str) {
        dbus_emit_signal("com.realtimeaudio.WpaSupplicant", DBUS_SIGNAL_TYPE_SCAN_RESULTS, json_str);
        free((void*)json_str);
    }
    json_decref(details);
}