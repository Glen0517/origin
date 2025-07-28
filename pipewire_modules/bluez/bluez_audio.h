#ifndef BLUEZ_AUDIO_H
#define BLUEZ_AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <dbus/dbus.h>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format.h>
#include <spa/param/bluetooth.h>

#define BLUEZ_MAX_DEVICES 10

typedef enum {
    CONNECTION_STATE_DISCONNECTED,
    CONNECTION_STATE_CONNECTING,
    CONNECTION_STATE_CONNECTED,
    CONNECTION_STATE_DISCONNECTING
} ConnectionState;

typedef enum {
    DEVICE_TYPE_HEADPHONES,
    DEVICE_TYPE_SPEAKER,
    DEVICE_TYPE_MICROPHONE,
    DEVICE_TYPE_LE_AUDIO
} DeviceType;

typedef struct {
    char address[18];          // 蓝牙地址 (AA:BB:CC:DD:EE:FF)
    char name[256];            // 设备名称
    DeviceType type;           // 设备类型
    ConnectionState state;     // 连接状态
    uint16_t handle;           // 连接句柄
    int hci_socket;            // HCI套接字
    DBusConnection* dbus_conn; // D-Bus连接
    struct pw_stream* stream;  // PipeWire流
    struct spa_audio_info format; // 音频格式
    bool streaming;            // 是否正在流传输
    pthread_mutex_t mutex;     // 互斥锁
    bool avrcp_supported;      // 支持AVRCP
    bool le_audio_supported;   // 支持LE Audio
    bool cis_connected;        // CIS连接状态
    char codec_name[32];       // 当前编解码器名称
    uint32_t codec_sample_rate;// 编解码器采样率
    uint8_t codec_channels;    // 编解码器通道数
    uint8_t codec_bits_per_sample; // 编解码器位深
    struct spa_param_bluetooth_a2dp a2dp_params; // A2DP参数
    struct spa_param_bluetooth_le_audio le_audio_params; // LE Audio参数
} BluezAudioDevice;

// 初始化和销毁函数
int bluez_audio_init(void);
void bluez_audio_destroy(void);

// 连接管理函数
int bluez_audio_connect(const char* address);
int bluez_audio_disconnect(const char* address);

// 设备发现和信息函数
int bluez_audio_get_devices(BluezAudioDevice** devices, size_t max_devices);
void bluez_parse_advertising_data(BluezAudioDevice* dev, uint8_t* data, size_t length);

// 音频流函数
int bluez_audio_write(BluezAudioDevice* dev, const void* data, size_t size);

// 新增功能函数
int bluez_negotiate_codec(BluezAudioDevice* dev);
int bluez_avrcp_send_command(BluezAudioDevice* dev, uint8_t command, uint8_t data);
int bluez_le_audio_setup_cis(BluezAudioDevice* dev);

// BLE控制函数
int bluez_ble_send_command(const char* address, uint8_t* data, size_t length);
int bluez_ble_start_scan(void);
int bluez_ble_stop_scan(void);

#endif // BLUEZ_AUDIO_H