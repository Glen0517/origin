#include "audio_source_priv.h"
#include "logger.h"
#include "audio_core.h"
#include "storage.h"
#include "hal.h"  // 硬件抽象层

// 存储进程管理函数声明
int storage_process_mount_device(const char *dev_path);
int storage_process_umount_device(void);

static bool g_usb_src_init = false;         // USB音频源初始化标志
static bool g_usb_connected = false;        // USB设备连接状态
static char g_usb_device_path[64] = {0};    // USB设备路径

/**
 * @brief 获取USB音频源状态
 * @details 检查USB设备是否连接
 * @return 连接状态：1-已连接，0-未连接
 */
int src_usb_get_status(void) {
    return g_usb_src_init && g_usb_connected ? 1 : 0;
}

/**
 * @brief USB音频设备连接状态回调函数
 */
static void usb_audio_device_callback(const char *dev_path, bool connected) {
    if (g_usb_src_init) {
        g_usb_connected = connected;
        
        if (connected) {
            strncpy(g_usb_device_path, dev_path, sizeof(g_usb_device_path) - 1);
            LOG_INFO("USB audio device connected: %s", dev_path);
            
            // 打开USB音频设备（通过HAL层）
            // 参数：USB音频设备路径
            hal_usb_audio_open(dev_path);
            
            // 挂载USB存储设备
            // 参数：USB设备路径
            storage_process_mount_device(dev_path);
        } else {
            LOG_INFO("USB audio device disconnected: %s", g_usb_device_path);
            g_usb_device_path[0] = '\0';
            
            // 关闭USB音频设备（通过HAL层）
            // 无参数
            hal_usb_audio_close();
            
            // 卸载USB存储设备
            storage_process_umount_device();
        }
    }
}

/**
 * @brief USB音频数据接收回调函数
 */
static void usb_audio_data_callback(uint8_t *pcm_data, int data_len) {
    if (g_usb_src_init && g_usb_connected && pcm_data && data_len > 0) {
        // 将接收到的音频数据发送到音频核心
        audio_core_play_pcm(pcm_data, data_len);
    }
}

int src_usb_init(void) {
    if (g_usb_src_init) {
        LOG_INFO("USB source already initialized");
        return 0;
    }
    
    // 初始化USB硬件（通过HAL层）
    // 返回值：0表示成功，非0表示失败
    if (hal_usb_init() != 0) {
        LOG_ERROR("USB source init failed: HAL USB init error");
        return FAILURE;
    }
    
    // 设置USB音频设备连接状态回调（通过HAL层）
    // 参数：设备连接状态回调函数指针
    hal_usb_audio_set_device_callback(usb_audio_device_callback);
    
    // 设置USB音频数据接收回调（通过HAL层）
    // 参数：音频数据回调函数指针
    hal_usb_audio_set_data_callback(usb_audio_data_callback);
    
    // 启用USB音频设备检测（通过HAL层）
    // 参数：true表示启用，false表示禁用
    hal_usb_audio_enable_detection(true);
    
    g_usb_src_init = true;
    g_usb_connected = false;
    g_usb_device_path[0] = '\0';
    
    LOG_INFO("USB local source init success");
    LOG_INFO("  Device detection: ENABLED");
    
    return 0;
}

void src_usb_deinit(void) {
    if (g_usb_src_init) {
        // 禁用USB音频设备检测（通过HAL层）
        // 参数：false表示禁用
        hal_usb_audio_enable_detection(false);
        
        // 关闭当前打开的USB音频设备（通过HAL层）
        // 无参数
        if (g_usb_connected) {
            hal_usb_audio_close();
        }
        
        // 反初始化USB硬件（通过HAL层）
        // 无参数
        hal_usb_deinit();
        
        g_usb_src_init = false;
        g_usb_connected = false;
        g_usb_device_path[0] = '\0';
        
        LOG_INFO("USB local source deinit success");
    }
}