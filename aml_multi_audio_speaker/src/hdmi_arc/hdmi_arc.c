#include "hdmi_arc.h"
#include "logger.h"
#include "product_type.h"
#include "common_def.h"

#if CONFIG_ENABLE_HDMI_ARC

// 静态全局变量
static HdmiArcConfig_t g_hdmi_cfg = {0};
static bool g_hdmi_connected = false;
static int g_current_format = 0;

// Amlogic SDK相关头文件
#include <aml_hdmi.h>        // 晶晨HDMI SDK
#include <aml_cec.h>         // 晶晨CEC SDK
#include <aml_audio.h>       // 晶晨音频SDK

/**
 * @brief HDMI信号检测回调函数
 */
static void hdmi_signal_callback(int event, int format, int sample_rate)
{
    switch (event) {
        case HDMI_EVENT_CONNECTED:
            LOG_INFO("HDMI ARC connected");
            g_hdmi_connected = true;
            g_current_format = format;
            g_hdmi_cfg.sample_rate = sample_rate;
            break;
        case HDMI_EVENT_DISCONNECTED:
            LOG_INFO("HDMI ARC disconnected");
            g_hdmi_connected = false;
            g_current_format = 0;
            break;
        case HDMI_EVENT_FORMAT_CHANGED:
            LOG_INFO("HDMI ARC format changed: %d, sample rate: %d", format, sample_rate);
            g_current_format = format;
            g_hdmi_cfg.sample_rate = sample_rate;
            break;
        default:
            break;
    }
}

/**
 * @brief CEC事件回调函数
 */
static void cec_event_callback(int event, int device_id, int command)
{
    switch (event) {
        case CEC_EVENT_DEVICE_DETECTED:
            LOG_INFO("CEC device detected: %d", device_id);
            break;
        case CEC_EVENT_POWER_ON:
            LOG_INFO("CEC power on command received");
            // 处理电源开启命令
            system_api_set_state(SYS_STATE_WORKING);
            break;
        case CEC_EVENT_POWER_OFF:
            LOG_INFO("CEC power off command received");
            // 处理电源关闭命令
            system_api_set_state(SYS_STATE_STANDBY);
            break;
        case CEC_EVENT_VOLUME_UP:
            LOG_INFO("CEC volume up command received");
            // 处理音量增加命令
            volume_ctrl_increase();
            break;
        case CEC_EVENT_VOLUME_DOWN:
            LOG_INFO("CEC volume down command received");
            // 处理音量减少命令
            volume_ctrl_decrease();
            break;
        default:
            break;
    }
}

/**
 * @brief HDMI ARC初始化
 */
int hdmi_arc_init(HdmiArcConfig_t *cfg)
{
    // 初始化配置
    memset(&g_hdmi_cfg, 0, sizeof(HdmiArcConfig_t));
    if (cfg != NULL) {
        memcpy(&g_hdmi_cfg, cfg, sizeof(HdmiArcConfig_t));
    } else {
        // 默认配置
        g_hdmi_cfg.cec_en = true;
        g_hdmi_cfg.auto_switch_en = true;
        g_hdmi_cfg.sample_rate = 48000;
    }
    
    // 1. 初始化Amlogic HDMI
    if (aml_hdmi_init() != 0) {
        LOG_ERROR("HDMI init failed");
        return FAILURE;
    }
    
    // 2. 配置HDMI ARC
    aml_hdmi_enable_arc(true);
    aml_hdmi_set_callback(hdmi_signal_callback);
    
    // 3. 初始化CEC
    if (g_hdmi_cfg.cec_en) {
        if (aml_cec_init() != 0) {
            LOG_ERROR("CEC init failed");
        } else {
            aml_cec_set_callback(cec_event_callback);
            aml_cec_enable(true);
        }
    }
    
    // 4. 设置音频格式
    aml_hdmi_set_audio_format(0); // 默认格式
    
    // 5. 设置初始状态
    g_hdmi_connected = false;
    g_current_format = 0;
    
    LOG_INFO("HDMI ARC module init success");
    LOG_INFO("  CEC Enabled: %s", g_hdmi_cfg.cec_en ? "ON" : "OFF");
    LOG_INFO("  Auto Switch: %s", g_hdmi_cfg.auto_switch_en ? "ON" : "OFF");
    
    return SUCCESS;
}

/**
 * @brief HDMI ARC反初始化
 */
int hdmi_arc_deinit(void)
{
    // 关闭CEC
    if (g_hdmi_cfg.cec_en) {
        aml_cec_enable(false);
        aml_cec_deinit();
    }
    
    // 关闭HDMI ARC
    aml_hdmi_enable_arc(false);
    aml_hdmi_deinit();
    
    g_hdmi_connected = false;
    g_current_format = 0;
    
    LOG_INFO("HDMI ARC module deinit success");
    
    return SUCCESS;
}

/**
 * @brief 检测HDMI ARC信号
 */
bool hdmi_arc_detect_signal(void)
{
    // 使用Amlogic HDMI SDK检测信号
    g_hdmi_connected = aml_hdmi_detect_signal();
    
    LOG_INFO("HDMI ARC signal detected: %s", g_hdmi_connected ? "YES" : "NO");
    
    return g_hdmi_connected;
}

/**
 * @brief 设置HDMI ARC音频格式
 */
int hdmi_arc_set_audio_format(int fmt)
{
    // 验证参数
    if (fmt < 0 || fmt > 15) {
        LOG_ERROR("Invalid HDMI audio format: %d", fmt);
        return FAILURE;
    }
    
    // 使用Amlogic HDMI SDK设置音频格式
    if (aml_hdmi_set_audio_format(fmt) != 0) {
        LOG_ERROR("Set HDMI audio format failed");
        return FAILURE;
    }
    
    g_current_format = fmt;
    LOG_INFO("Set HDMI audio format to %d", fmt);
    
    return SUCCESS;
}

/**
 * @brief 获取当前HDMI ARC状态
 */
bool hdmi_arc_get_status(void)
{
    return g_hdmi_connected;
}

/**
 * @brief 轮询处理HDMI ARC事件
 */
void hdmi_arc_event_poll(void)
{
    // 轮询HDMI事件
    aml_hdmi_event_poll();
    
    // 轮询CEC事件
    if (g_hdmi_cfg.cec_en) {
        aml_cec_event_poll();
    }
    
    // 自动切换检测
    if (g_hdmi_cfg.auto_switch_en && g_hdmi_connected) {
        // 检测HDMI音频信号
        bool has_audio = aml_hdmi_check_audio_present();
        
        if (has_audio) {
            // 获取当前音源
            int current_source = audio_source_get_current();
            
            // 如果当前不是HDMI ARC音源，则自动切换
            if (current_source != SOURCE_HDMI_ARC) {
                LOG_INFO("HDMI ARC auto switch triggered");
                audio_source_switch(SOURCE_HDMI_ARC);
            }
        }
    }
}

#endif
