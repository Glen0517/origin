#include "audio_source_priv.h"
#include "logger.h"
#include "audio_core.h"
#include "product_type.h"

#include <aml_line_in.h>     // 晶晨Line-In SDK

static bool g_aux_src_init = false;
static bool g_aux_detect_enabled = true;
static bool g_aux_signal_present = false;

/**
 * @brief AUX信号检测回调函数
 */
static void aux_signal_detect_callback(bool signal_present) {
    if (g_aux_src_init && g_aux_detect_enabled) {
        g_aux_signal_present = signal_present;
        LOG_INFO("AUX signal detected: %s", signal_present ? "YES" : "NO");
        
        if (!signal_present) {
            // 当没有信号时，可以选择暂停播放
            audio_core_pause();
        }
    }
}

/**
 * @brief AUX音频数据接收回调函数
 */
static void aux_audio_data_callback(uint8_t *pcm_data, int data_len) {
    if (g_aux_src_init && g_aux_signal_present && pcm_data && data_len > 0) {
        // 将接收到的音频数据发送到音频核心
        audio_core_play_pcm(pcm_data, data_len);
    }
}

#ifdef CONFIG_ENABLE_AUX
int src_aux_init(void) {
    if (g_aux_src_init) {
        LOG_INFO("AUX source already initialized");
        return 0;
    }
    
    // 初始化Amlogic Line-In SDK
    if (aml_line_in_init() != 0) {
        LOG_ERROR("AUX source init failed: Line-In SDK init error");
        return -1;
    }
    
    // 设置AUX音频回调函数
    aml_line_in_set_signal_callback(aux_signal_detect_callback);
    aml_line_in_set_data_callback(aux_audio_data_callback);
    
    // 配置AUX参数
    aml_line_in_set_sample_rate(48000);      // 默认采样率48kHz
    aml_line_in_set_channels(2);             // 默认立体声
    aml_line_in_set_gain(10);                // 默认增益10dB
    
    // 启用AUX功能
    aml_line_in_enable(true);
    aml_line_in_enable_detection(g_aux_detect_enabled);
    
    g_aux_src_init = true;
    g_aux_signal_present = false;
    
    LOG_INFO("AUX line-in source init success");
    LOG_INFO("  Sample rate: 48kHz");
    LOG_INFO("  Channels: Stereo");
    LOG_INFO("  Signal detection: %s", g_aux_detect_enabled ? "ENABLED" : "DISABLED");
    
    return 0;
}

void src_aux_deinit(void) {
    if (g_aux_src_init) {
        // 禁用AUX功能
        aml_line_in_enable(false);
        aml_line_in_enable_detection(false);
        
        // 反初始化Amlogic Line-In SDK
        aml_line_in_deinit();
        
        g_aux_src_init = false;
        g_aux_signal_present = false;
        
        LOG_INFO("AUX line-in source deinit success");
    }
}
#else
int src_aux_init(void) { return 0; }
void src_aux_deinit(void) {
    LOG_INFO("AUX source deinit called");
    // 清理AUX相关资源
    // 虽然是空实现，但保持函数接口一致
}
#endif