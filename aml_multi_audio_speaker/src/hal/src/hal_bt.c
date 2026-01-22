/**
 * @file hal_bt.c
 * @brief 蓝牙硬件抽象实现
 * @details 实现蓝牙硬件抽象层的接口函数，封装Amlogic蓝牙SDK的功能
 * @author AML Audio Team
 * @date 2026-01-16
 */

#include "hal_bt.h"
#include "log/aml_log.h"
#include <aml_bt.h>
#include <aml_bt_a2dp.h>
#include <aml_bt_hfp.h>
#include "bluetooth/bluetooth.h"
#include "bluetooth/hci.h"
#include "bluetooth/hci_lib.h"
#include "bluetooth/l2cap.h"
#include "bluetooth/sdp.h"
#include "bluetooth/sdp_lib.h"

// 定义蓝牙模块日志分类
AML_LOG_DEFINE(bt_log);
// 设置默认日志分类
#define AML_LOG_DEFAULT AML_LOG_GET_CAT(bt_log)

static bool g_bt_init = false;

/**
 * @brief 初始化蓝牙硬件
 * @details 初始化Amlogic蓝牙SDK，准备蓝牙硬件
 * @return 初始化结果：0表示成功，非0表示失败
 */
int hal_bt_init(void) {
    if (g_bt_init) {
        LOG_INFO("HAL bluetooth already initialized");
        return SUCCESS;
    }
    
    if (aml_bt_init() != 0) {
        LOG_ERROR("Amlogic bluetooth SDK init failed");
        return FAILURE;
    }
    
    g_bt_init = true;
    LOG_INFO("HAL bluetooth init success");
    return SUCCESS;
}

/**
 * @brief 反初始化蓝牙硬件
 * @details 反初始化Amlogic蓝牙SDK，清理蓝牙硬件资源
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int hal_bt_deinit(void) {
    if (!g_bt_init) {
        LOG_INFO("HAL bluetooth not initialized");
        return SUCCESS;
    }
    
    if (aml_bt_deinit() != 0) {
        LOG_ERROR("Amlogic bluetooth SDK deinit failed");
        return FAILURE;
    }
    
    g_bt_init = false;
    LOG_INFO("HAL bluetooth deinit success");
    return SUCCESS;
}

/**
 * @brief 获取蓝牙连接状态
 * @details 获取当前蓝牙设备的连接状态
 * @return 连接状态：1表示已连接，0表示未连接
 */
int hal_bt_get_connection_status(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return 0;
    }
    
    return aml_bt_get_connection_status();
}

/**
 * @brief 获取蓝牙A2DP媒体状态
 * @details 获取蓝牙A2DP音频流的播放状态
 * @return 媒体状态：1表示正在播放，0表示停止
 */
int hal_bt_a2dp_get_media_status(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return 0;
    }
    
    return aml_bt_a2dp_get_media_status();
}

/**
 * @brief 轮询蓝牙事件
 * @details 处理蓝牙相关的事件，包括连接、断开、媒体流等
 * @return 轮询结果：0表示成功，非0表示失败
 */
int hal_bt_event_poll(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    // 轮询蓝牙A2DP事件
    if (hal_bt_a2dp_event_poll() != 0) {
        LOG_ERROR("Poll A2DP event failed");
    }
    
    // 轮询蓝牙HFP事件
    if (hal_bt_hfp_event_poll() != 0) {
        LOG_ERROR("Poll HFP event failed");
    }
    
    // 轮询蓝牙MESH事件（如果启用）
    #ifdef CONFIG_ENABLE_BT_MESH
    if (hal_bt_mesh_event_poll() != 0) {
        LOG_ERROR("Poll MESH event failed");
    }
    #endif
    
    return SUCCESS;
}

/**
 * @brief 轮询蓝牙A2DP事件
 * @details 处理蓝牙A2DP相关的事件
 * @return 轮询结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_event_poll(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_a2dp_event_poll();
    if (ret != 0) {
        LOG_ERROR("Amlogic A2DP event poll failed: %d", ret);
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 轮询蓝牙HFP事件
 * @details 处理蓝牙HFP（免提电话）相关的事件
 * @return 轮询结果：0表示成功，非0表示失败
 */
int hal_bt_hfp_event_poll(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_hfp_event_poll();
    if (ret != 0) {
        LOG_ERROR("Amlogic HFP event poll failed: %d", ret);
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 轮询蓝牙MESH事件
 * @details 处理蓝牙MESH网络相关的事件
 * @return 轮询结果：0表示成功，非0表示失败
 */
int hal_bt_mesh_event_poll(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    #ifdef CONFIG_ENABLE_BT_MESH
    int ret = aml_bt_mesh_event_poll();
    if (ret != 0) {
        LOG_ERROR("Amlogic MESH event poll failed: %d", ret);
        return FAILURE;
    }
    return SUCCESS;
    #else
    LOG_WARN("BT MESH not enabled");
    return SUCCESS;
    #endif
}

/**
 * @brief 设置蓝牙设备名称
 * @details 设置蓝牙设备的显示名称
 * @param name 设备名称字符串
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_bt_set_device_name(const char *name) {
    if (!g_bt_init) {
        AML_LOGE("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    if (!name || strlen(name) == 0 || strlen(name) > 31) {
        AML_LOGE("Invalid device name: %s (length: %zu)", name, name ? strlen(name) : 0);
        return INVALID_PARAM;
    }
    
    int ret = aml_bt_set_device_name(name);
    if (ret != 0) {
        AML_LOGE("Amlogic bluetooth set device name failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("HAL bluetooth device name set to: %s", name);
    return SUCCESS;
}

/**
 * @brief 设置蓝牙配对码
 * @details 设置蓝牙设备的配对码
 * @param pin 配对码字符串
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_bt_set_pin_code(const char *pin) {
    if (!g_bt_init) {
        AML_LOGE("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    if (!pin || strlen(pin) == 0 || strlen(pin) > 16) {
        AML_LOGE("Invalid pin code: %s (length: %zu)", pin, pin ? strlen(pin) : 0);
        return INVALID_PARAM;
    }
    
    int ret = aml_bt_set_pin_code(pin);
    if (ret != 0) {
        AML_LOGE("Amlogic bluetooth set pin code failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("HAL bluetooth pin code set successfully");
    return SUCCESS;
}

/**
 * @brief 设置蓝牙可发现模式
 * @details 设置蓝牙设备是否可被其他设备发现
 * @param discoverable 是否可发现
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_bt_set_discoverable(bool discoverable) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_set_discoverable(discoverable);
    if (ret != 0) {
        LOG_ERROR("Amlogic bluetooth set discoverable failed: %d", ret);
        return FAILURE;
    }
    
    LOG_INFO("HAL bluetooth discoverable set to: %d", discoverable);
    return SUCCESS;
}

/**
 * @brief 设置蓝牙可配对模式
 * @details 设置蓝牙设备是否可被其他设备配对
 * @param pairable 是否可配对
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_bt_set_pairable(bool pairable) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_set_pairable(pairable);
    if (ret != 0) {
        LOG_ERROR("Amlogic bluetooth set pairable failed: %d", ret);
        return FAILURE;
    }
    
    LOG_INFO("HAL bluetooth pairable set to: %d", pairable);
    return SUCCESS;
}

/**
 * @brief 设置蓝牙可配对超时时间
 * @details 设置蓝牙设备可配对模式的超时时间
 * @param timeout 超时时间（秒）
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_bt_set_pairable_timeout(int timeout) {
    if (!g_bt_init) {
        AML_LOGE("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    if (timeout < 0 || timeout > 3600) {
        AML_LOGE("Invalid timeout value: %d (range: 0-3600)", timeout);
        return INVALID_PARAM;
    }
    
    int ret = aml_bt_set_pairable_timeout(timeout);
    if (ret != 0) {
        AML_LOGE("Amlogic bluetooth set pairable timeout failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("HAL bluetooth pairable timeout set to: %d seconds", timeout);
    return SUCCESS;
}

/**
 * @brief 获取已连接蓝牙设备名称
 * @details 获取当前已连接的蓝牙设备名称
 * @param name 设备名称缓冲区
 * @param len 缓冲区长度
 * @return 获取结果：0表示成功，非0表示失败
 */
int hal_bt_get_connected_dev_name(char *name, int len) {
    if (!g_bt_init) {
        AML_LOGE("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    if (!name || len <= 0 || len > 256) {
        AML_LOGE("Invalid parameters: name=%p, len=%d", name, len);
        return INVALID_PARAM;
    }
    
    int ret = aml_bt_get_connected_dev_name(name, len);
    if (ret != 0) {
        AML_LOGE("Amlogic bluetooth get connected device name failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGD("HAL bluetooth connected device name: %s", name);
    return SUCCESS;
}

/**
 * @brief 获取已连接蓝牙设备地址
 * @details 获取当前已连接的蓝牙设备地址
 * @param addr 设备地址缓冲区
 * @param len 缓冲区长度
 * @return 获取结果：0表示成功，非0表示失败
 */
int hal_bt_get_connected_dev_addr(char *addr, int len) {
    if (!g_bt_init) {
        AML_LOGE("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    if (!addr || len <= 0 || len < 18) {
        AML_LOGE("Invalid parameters: addr=%p, len=%d (minimum 18 bytes required)", addr, len);
        return INVALID_PARAM;
    }
    
    int ret = aml_bt_get_connected_dev_addr(addr, len);
    if (ret != 0) {
        AML_LOGE("Amlogic bluetooth get connected device address failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGD("HAL bluetooth connected device address: %s", addr);
    return SUCCESS;
}

/**
 * @brief 获取已连接蓝牙设备类型
 * @details 获取当前已连接的蓝牙设备类型
 * @param type 设备类型指针
 * @return 获取结果：0表示成功，非0表示失败
 */
int hal_bt_get_connected_dev_type(int *type) {
    if (!g_bt_init) {
        AML_LOGE("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    if (!type) {
        AML_LOGE("Invalid parameters: type=%p", type);
        return INVALID_PARAM;
    }
    
    int ret = aml_bt_get_connected_dev_type(type);
    if (ret != 0) {
        AML_LOGE("Amlogic bluetooth get connected device type failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGD("HAL bluetooth connected device type: %d", *type);
    return SUCCESS;
}

/**
 * @brief 获取蓝牙断连原因
 * @details 获取蓝牙断开连接的原因
 * @return 断连原因：0表示未知，1表示正常断开，2表示信号丢失，3表示设备电量低，4表示其他
 */
int hal_bt_get_disconnect_reason(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return 0;
    }
    
    int reason = aml_bt_get_disconnect_reason();
    LOG_DEBUG("HAL bluetooth disconnect reason: %d", reason);
    return reason;
}

/**
 * @brief 扫描蓝牙设备
 * @details 扫描周围的蓝牙设备
 * @param timeout 扫描超时时间（秒）
 * @param device_info 设备信息回调函数
 * @param user_data 用户自定义数据
 * @return 扫描结果：0表示成功，非0表示失败
 */
int hal_bt_scan_devices(int timeout, BluetoothDeviceCallback_t device_info, void *user_data) {
    if (!g_bt_init) {
        AML_LOGE("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    if (timeout <= 0 || timeout > 30) {
        AML_LOGE("Invalid scan timeout: %d (range: 1-30 seconds)", timeout);
        return INVALID_PARAM;
    }
    
    AML_LOGI("Scanning Bluetooth devices for %d seconds", timeout);
    
    // 使用bluez5库扫描蓝牙设备
    int dev_id = hci_get_route(NULL);
    if (dev_id < 0) {
        AML_LOGE("Failed to get HCI device: %s", strerror(errno));
        return FAILURE;
    }
    
    int sock = hci_open_dev(dev_id);
    if (sock < 0) {
        AML_LOGE("Failed to open HCI device: %s", strerror(errno));
        return FAILURE;
    }
    
    // 设置扫描参数
    uint8_t scan_type = 0x01; // 主动扫描
    uint16_t interval = htobs(0x0010); // 扫描间隔
    uint16_t window = htobs(0x0010); // 扫描窗口
    uint8_t own_type = 0x00; // 公共地址
    uint8_t filter_policy = 0x00; // 不使用过滤
    
    if (hci_le_set_scan_parameters(sock, scan_type, interval, window, own_type, filter_policy, 1000) < 0) {
        AML_LOGE("Failed to set scan parameters: %s", strerror(errno));
        close(sock);
        return FAILURE;
    }
    
    // 启用扫描
    if (hci_le_set_scan_enable(sock, 1, 0, 1000) < 0) {
        AML_LOGE("Failed to enable scan: %s", strerror(errno));
        close(sock);
        return FAILURE;
    }
    
    // 开始扫描
    AML_LOGI("Bluetooth scan started");
    
    // 模拟扫描结果，实际实现中应该使用hci_le_set_scan_callback
    if (device_info) {
        BluetoothDeviceInfo_t device;
        memset(&device, 0, sizeof(BluetoothDeviceInfo_t));
        strcpy(device.address, "00:11:22:33:44:55");
        strcpy(device.name, "Test Device");
        device.rssi = -75;
        device.flags = 0x05; // 可发现，可连接
        device_info(&device, user_data);
    }
    
    // 等待扫描完成
    sleep(timeout);
    
    // 禁用扫描
    if (hci_le_set_scan_enable(sock, 0, 0, 1000) < 0) {
        AML_LOGE("Failed to disable scan: %s", strerror(errno));
        close(sock);
        return FAILURE;
    }
    
    close(sock);
    AML_LOGI("Bluetooth scan completed");
    return SUCCESS;
}

/**
 * @brief 连接蓝牙设备
 * @details 连接指定地址的蓝牙设备
 * @param addr 蓝牙设备地址
 * @return 连接结果：0表示成功，非0表示失败
 */
int hal_bt_connect(const char *addr) {
    if (!g_bt_init) {
        AML_LOGE("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    if (!addr || strlen(addr) != 17) {
        AML_LOGE("Invalid Bluetooth device address: %s", addr);
        return INVALID_PARAM;
    }
    
    int ret = aml_bt_connect(addr);
    if (ret != 0) {
        AML_LOGE("Amlogic bluetooth connect failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("HAL bluetooth connect initiated to: %s", addr);
    return SUCCESS;
}

/**
 * @brief 断开蓝牙连接
 * @details 断开当前蓝牙设备的连接
 * @return 断开结果：0表示成功，非0表示失败
 */
int hal_bt_disconnect(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_disconnect();
    if (ret != 0) {
        LOG_ERROR("Amlogic bluetooth disconnect failed: %d", ret);
        return FAILURE;
    }
    
    LOG_INFO("HAL bluetooth disconnect initiated");
    return SUCCESS;
}

/**
 * @brief 初始化蓝牙A2DP功能
 * @details 初始化蓝牙A2DP协议相关的功能
 * @return 初始化结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_init(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_a2dp_init();
    if (ret != 0) {
        LOG_ERROR("Amlogic bluetooth A2DP init failed: %d", ret);
        return FAILURE;
    }
    
    LOG_INFO("HAL bluetooth A2DP init success");
    return SUCCESS;
}

/**
 * @brief 反初始化蓝牙A2DP功能
 * @details 反初始化蓝牙A2DP协议相关的功能
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_deinit(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_a2dp_deinit();
    if (ret != 0) {
        LOG_ERROR("Amlogic bluetooth A2DP deinit failed: %d", ret);
        return FAILURE;
    }
    
    LOG_INFO("HAL bluetooth A2DP deinit success");
    return SUCCESS;
}

/**
 * @brief 开始蓝牙A2DP音频流
 * @details 开始蓝牙A2DP音频流的传输
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_start_stream(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_a2dp_start_stream();
    if (ret != 0) {
        LOG_ERROR("Amlogic bluetooth A2DP start stream failed: %d", ret);
        return FAILURE;
    }
    
    LOG_INFO("HAL bluetooth A2DP stream started");
    return SUCCESS;
}

/**
 * @brief 停止蓝牙A2DP音频流
 * @details 停止蓝牙A2DP音频流的传输
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_stop_stream(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_a2dp_stop_stream();
    if (ret != 0) {
        LOG_ERROR("Amlogic bluetooth A2DP stop stream failed: %d", ret);
        return FAILURE;
    }
    
    LOG_INFO("HAL bluetooth A2DP stream stopped");
    return SUCCESS;
}

/**
 * @brief 设置蓝牙A2DP音量
 * @details 设置蓝牙A2DP音频流的音量
 * @param volume 音量值（0-100）
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_set_volume(int volume) {
    if (!g_bt_init) {
        AML_LOGE("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    if (volume < 0 || volume > 100) {
        AML_LOGE("Invalid A2DP volume: %d", volume);
        return INVALID_PARAM;
    }
    
    int ret = aml_bt_a2dp_set_volume(volume);
    if (ret != 0) {
        AML_LOGE("Amlogic bluetooth A2DP set volume failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("HAL bluetooth A2DP volume set to: %d", volume);
    return SUCCESS;
}

/**
 * @brief 获取蓝牙A2DP音量
 * @details 获取蓝牙A2DP音频流的当前音量
 * @return 音量值（0-100），失败返回-1
 */
int hal_bt_a2dp_get_volume(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return -1;
    }
    
    int volume = aml_bt_a2dp_get_volume();
    if (volume < 0) {
        LOG_ERROR("Amlogic bluetooth A2DP get volume failed");
        return -1;
    }
    
    LOG_DEBUG("HAL bluetooth A2DP volume: %d", volume);
    return volume;
}

/**
 * @brief 设置蓝牙A2DP音频参数
 * @details 设置蓝牙A2DP音频流的参数
 * @param sample_rate 采样率
 * @param channels 声道数
 * @param bit_depth 比特深度
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_set_audio_params(int sample_rate, int channels, int bit_depth) {
    if (!g_bt_init) {
        AML_LOGE("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    if (sample_rate <= 0 || channels <= 0 || bit_depth <= 0) {
        AML_LOGE("Invalid A2DP audio parameters: sample_rate=%d, channels=%d, bit_depth=%d", 
                 sample_rate, channels, bit_depth);
        return INVALID_PARAM;
    }
    
    int ret = aml_bt_a2dp_set_audio_params(sample_rate, channels, bit_depth);
    if (ret != 0) {
        AML_LOGE("Amlogic bluetooth A2DP set audio parameters failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("HAL bluetooth A2DP audio parameters set: sample_rate=%d, channels=%d, bit_depth=%d", 
             sample_rate, channels, bit_depth);
    return SUCCESS;
}

/**
 * @brief 蓝牙A2DP播放控制：播放
 * @details 控制蓝牙A2DP音频流开始播放
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_play(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_a2dp_play();
    if (ret != 0) {
        LOG_ERROR("Amlogic bluetooth A2DP play failed: %d", ret);
        return FAILURE;
    }
    
    LOG_INFO("HAL bluetooth A2DP play command sent");
    return SUCCESS;
}

/**
 * @brief 蓝牙A2DP播放控制：暂停
 * @details 控制蓝牙A2DP音频流暂停播放
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_pause(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_a2dp_pause();
    if (ret != 0) {
        LOG_ERROR("Amlogic bluetooth A2DP pause failed: %d", ret);
        return FAILURE;
    }
    
    LOG_INFO("HAL bluetooth A2DP pause command sent");
    return SUCCESS;
}

/**
 * @brief 蓝牙A2DP播放控制：停止
 * @details 控制蓝牙A2DP音频流停止播放
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_stop(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_a2dp_stop();
    if (ret != 0) {
        LOG_ERROR("Amlogic bluetooth A2DP stop failed: %d", ret);
        return FAILURE;
    }
    
    LOG_INFO("HAL bluetooth A2DP stop command sent");
    return SUCCESS;
}

/**
 * @brief 蓝牙A2DP播放控制：下一曲
 * @details 控制蓝牙A2DP音频流播放下一曲
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_next(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_a2dp_next();
    if (ret != 0) {
        LOG_ERROR("Amlogic bluetooth A2DP next failed: %d", ret);
        return FAILURE;
    }
    
    LOG_INFO("HAL bluetooth A2DP next command sent");
    return SUCCESS;
}

/**
 * @brief 蓝牙A2DP播放控制：上一曲
 * @details 控制蓝牙A2DP音频流播放上一曲
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_prev(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_a2dp_prev();
    if (ret != 0) {
        LOG_ERROR("Amlogic bluetooth A2DP prev failed: %d", ret);
        return FAILURE;
    }
    
    LOG_INFO("HAL bluetooth A2DP prev command sent");
    return SUCCESS;
}

/**
 * @brief 蓝牙A2DP音量控制：增加音量
 * @details 控制蓝牙A2DP音频流增加音量
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_volume_up(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_a2dp_volume_up();
    if (ret != 0) {
        LOG_ERROR("Amlogic bluetooth A2DP volume up failed: %d", ret);
        return FAILURE;
    }
    
    LOG_INFO("HAL bluetooth A2DP volume up command sent");
    return SUCCESS;
}

/**
 * @brief 蓝牙A2DP音量控制：减少音量
 * @details 控制蓝牙A2DP音频流减少音量
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_volume_down(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_a2dp_volume_down();
    if (ret != 0) {
        LOG_ERROR("Amlogic bluetooth A2DP volume down failed: %d", ret);
        return FAILURE;
    }
    
    LOG_INFO("HAL bluetooth A2DP volume down command sent");
    return SUCCESS;
}

/**
 * @brief 蓝牙A2DP音量控制：静音
 * @details 控制蓝牙A2DP音频流静音
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_mute(void) {
    if (!g_bt_init) {
        AML_LOGE("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_a2dp_mute();
    if (ret != 0) {
        AML_LOGE("Amlogic bluetooth A2DP mute failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("HAL bluetooth A2DP mute command sent");
    return SUCCESS;
}

/**
 * @brief 蓝牙A2DP音量控制：取消静音
 * @details 控制蓝牙A2DP音频流取消静音
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_unmute(void) {
    if (!g_bt_init) {
        AML_LOGE("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_a2dp_unmute();
    if (ret != 0) {
        AML_LOGE("Amlogic bluetooth A2DP unmute failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("HAL bluetooth A2DP unmute command sent");
    return SUCCESS;
}

/**
 * @brief 创建蓝牙MESH网络
 * @details 创建蓝牙MESH网络
 * @return 创建结果：0表示成功，非0表示失败
 */
int hal_bt_mesh_create_network(void) {
    if (!g_bt_init) {
        AML_LOGE("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    #ifdef CONFIG_ENABLE_BT_MESH
    int ret = aml_bt_mesh_create_network();
    if (ret != 0) {
        AML_LOGE("Amlogic bluetooth MESH create network failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("HAL bluetooth MESH network created");
    return SUCCESS;
    #else
    AML_LOGW("BT MESH not enabled");
    return FAILURE;
    #endif
}

/**
 * @brief 蓝牙MESH配对
 * @details 进行蓝牙MESH设备配对
 * @return 配对结果：0表示成功，非0表示失败
 */
int hal_bt_mesh_pair(void) {
    if (!g_bt_init) {
        AML_LOGE("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    #ifdef CONFIG_ENABLE_BT_MESH
    int ret = aml_bt_mesh_pair();
    if (ret != 0) {
        AML_LOGE("Amlogic bluetooth MESH pair failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("HAL bluetooth MESH pairing started");
    return SUCCESS;
    #else
    AML_LOGW("BT MESH not enabled");
    return FAILURE;
    #endif
}

/**
 * @brief 蓝牙MESH音量同步
 * @details 同步蓝牙MESH网络中所有设备的音量
 * @param vol 音量值
 * @return 同步结果：0表示成功，非0表示失败
 */
int hal_bt_mesh_sync_volume(int vol) {
    if (!g_bt_init) {
        AML_LOGE("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    if (vol < 0 || vol > 100) {
        AML_LOGE("Invalid MESH volume: %d (range: 0-100)", vol);
        return INVALID_PARAM;
    }
    
    #ifdef CONFIG_ENABLE_BT_MESH
    int ret = aml_bt_mesh_sync_volume(vol);
    if (ret != 0) {
        AML_LOGE("Amlogic bluetooth MESH sync volume failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("HAL bluetooth MESH volume synced to: %d", vol);
    return SUCCESS;
    #else
    AML_LOGW("BT MESH not enabled");
    return NOT_SUPPORT;
    #endif
}