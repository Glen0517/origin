#include "audio_source_priv.h"
#include "logger.h"
#include "audio_core.h"
#include "hdmi_arc.h"
#include "product_type.h"

#include <aml_hdmi.h>        // 晶晨HDMI SDK

static bool g_hdmi_src_init = false;
static bool g_hdmi_signal_present = false;
static int g_hdmi_current_format = 0;

/**
 * @brief HDMI音频数据接收回调函数
 */
static void hdmi_audio_data_callback(uint8_t *pcm_data, int data_len) {
    if (g_hdmi_src_init && g_hdmi_signal_present && pcm_data && data_len > 0) {
        // 将接收到的音频数据发送到音频核心
        audio_core_play_pcm(pcm_data, data_len);
    }
}

/**
 * @brief HDMI音频格式变更回调函数
 */
static void hdmi_audio_format_callback(int format) {
    if (g_hdmi_src_init) {
        g_hdmi_current_format = format;
        LOG_INFO("HDMI audio format changed to: %d", format);
    }
}

#ifdef CONFIG_ENABLE_HDMI_ARC
int src_hdmi_init(void) {
    if (g_hdmi_src_init) {
        LOG_INFO("HDMI ARC source already initialized");
        return 0;
    }
    
    // 初始化HDMI ARC模块
    HdmiArcConfig_t hdmi_cfg = {
        .cec_en = true,
        .auto_switch_en = true,
        .sample_rate = 48000
    };
    
    if (hdmi_arc_init(&hdmi_cfg) != 0) {
        LOG_ERROR("HDMI ARC source init failed: HDMI ARC module init error");
        return -1;
    }
    
    // 设置HDMI音频回调函数
    aml_hdmi_set_audio_callback(hdmi_audio_data_callback);
    aml_hdmi_set_format_callback(hdmi_audio_format_callback);
    
    // 配置HDMI音频参数
    aml_hdmi_set_audio_sample_rate(48000);    // 默认采样率48kHz
    aml_hdmi_set_audio_channels(2);           // 默认立体声
    
    g_hdmi_src_init = true;
    g_hdmi_signal_present = hdmi_arc_detect_signal();
    g_hdmi_current_format = 0;
    
    LOG_INFO("HDMI ARC source init success");
    LOG_INFO("  Signal detected: %s", g_hdmi_signal_present ? "YES" : "NO");
    LOG_INFO("  Sample rate: 48kHz");
    LOG_INFO("  Channels: Stereo");
    
    return 0;
}

void src_hdmi_deinit(void) {
    if (g_hdmi_src_init) {
        // 反初始化HDMI ARC模块
        hdmi_arc_deinit();
        
        g_hdmi_src_init = false;
        g_hdmi_signal_present = false;
        g_hdmi_current_format = 0;
        
        LOG_INFO("HDMI ARC source deinit success");
    }
}
#else
int src_hdmi_init(void) { return 0; }
void src_hdmi_deinit(void) {}
#endif