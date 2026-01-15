#include "audio_source_priv.h"
#include "logger.h"
#include "audio_core.h"
#include "product_type.h"

#include <aml_uac.h>         // 晶晨UAC SDK

static bool g_uac_src_init = false;
static bool g_uac_device_connected = false;
static char g_uac_device_path[64] = {0};

/**
 * @brief UAC设备连接回调函数
 */
static void uac_device_callback(const char *dev_path, bool connected) {
    if (g_uac_src_init) {
        g_uac_device_connected = connected;
        
        if (connected) {
            strncpy(g_uac_device_path, dev_path, sizeof(g_uac_device_path) - 1);
            LOG_INFO("UAC device connected: %s", dev_path);
            
            // 打开UAC设备
            aml_uac_open(dev_path);
        } else {
            LOG_INFO("UAC device disconnected: %s", g_uac_device_path);
            g_uac_device_path[0] = '\0';
            
            // 关闭UAC设备
            aml_uac_close();
        }
    }
}

/**
 * @brief UAC音频数据接收回调函数
 */
static void uac_audio_data_callback(uint8_t *pcm_data, int data_len) {
    if (g_uac_src_init && g_uac_device_connected && pcm_data && data_len > 0) {
        // 将接收到的音频数据发送到音频核心
        audio_core_play_pcm(pcm_data, data_len);
    }
}

// UAC仅高端产品支持
#ifdef CONFIG_ENABLE_UAC
int src_uac_init(void) {
    if (g_uac_src_init) {
        LOG_INFO("UAC source already initialized");
        return 0;
    }
    
    // 初始化Amlogic UAC SDK
    if (aml_uac_init() != 0) {
        LOG_ERROR("UAC source init failed: UAC SDK init error");
        return -1;
    }
    
    // 设置UAC回调函数
    aml_uac_set_device_callback(uac_device_callback);
    aml_uac_set_data_callback(uac_audio_data_callback);
    
    // 配置UAC参数
    aml_uac_set_sample_rate(48000);      // 默认采样率48kHz
    aml_uac_set_channels(2);             // 默认立体声
    aml_uac_set_buffer_size(4096);       // 默认缓冲区大小4KB
    
    // 启用UAC设备检测
    aml_uac_enable_detection(true);
    
    g_uac_src_init = true;
    g_uac_device_connected = false;
    g_uac_device_path[0] = '\0';
    
    LOG_INFO("UAC (USB Audio Class) source init success");
    LOG_INFO("  Device detection: ENABLED");
    LOG_INFO("  Sample rate: 48kHz");
    LOG_INFO("  Channels: Stereo");
    
    return 0;
}

void src_uac_deinit(void) {
    if (g_uac_src_init) {
        // 禁用UAC设备检测
        aml_uac_enable_detection(false);
        
        // 关闭当前打开的UAC设备
        if (g_uac_device_connected) {
            aml_uac_close();
        }
        
        // 反初始化Amlogic UAC SDK
        aml_uac_deinit();
        
        g_uac_src_init = false;
        g_uac_device_connected = false;
        g_uac_device_path[0] = '\0';
        
        LOG_INFO("UAC (USB Audio Class) source deinit success");
    }
}
#else
int src_uac_init(void) { return 0; }
void src_uac_deinit(void) {}
#endif