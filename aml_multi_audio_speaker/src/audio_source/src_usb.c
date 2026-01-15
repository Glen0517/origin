#include "audio_source_priv.h"
#include "logger.h"
#include "audio_core.h"
#include "storage.h"

#include <aml_usb_audio.h>  // 晶晨USB音频SDK

static bool g_usb_src_init = false;
static bool g_usb_connected = false;
static char g_usb_device_path[64] = {0};

/**
 * @brief USB音频设备连接状态回调函数
 */
static void usb_audio_device_callback(const char *dev_path, bool connected) {
    if (g_usb_src_init) {
        g_usb_connected = connected;
        
        if (connected) {
            strncpy(g_usb_device_path, dev_path, sizeof(g_usb_device_path) - 1);
            LOG_INFO("USB audio device connected: %s", dev_path);
            
            // 打开USB音频设备
            aml_usb_audio_open(dev_path);
        } else {
            LOG_INFO("USB audio device disconnected: %s", g_usb_device_path);
            g_usb_device_path[0] = '\0';
            
            // 关闭USB音频设备
            aml_usb_audio_close();
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
    
    // 初始化Amlogic USB音频SDK
    if (aml_usb_audio_init() != 0) {
        LOG_ERROR("USB source init failed: USB audio SDK init error");
        return -1;
    }
    
    // 设置USB音频回调函数
    aml_usb_audio_set_device_callback(usb_audio_device_callback);
    aml_usb_audio_set_data_callback(usb_audio_data_callback);
    
    // 启用USB音频设备检测
    aml_usb_audio_enable_detection(true);
    
    g_usb_src_init = true;
    g_usb_connected = false;
    g_usb_device_path[0] = '\0';
    
    LOG_INFO("USB local source init success");
    LOG_INFO("  Device detection: ENABLED");
    
    return 0;
}

void src_usb_deinit(void) {
    if (g_usb_src_init) {
        // 禁用USB音频设备检测
        aml_usb_audio_enable_detection(false);
        
        // 关闭当前打开的USB音频设备
        if (g_usb_connected) {
            aml_usb_audio_close();
        }
        
        // 反初始化Amlogic USB音频SDK
        aml_usb_audio_deinit();
        
        g_usb_src_init = false;
        g_usb_connected = false;
        g_usb_device_path[0] = '\0';
        
        LOG_INFO("USB local source deinit success");
    }
}