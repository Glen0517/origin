#include "spdif_optical.h"
#include "logger.h"
#include "product_type.h"
#include "common_def.h"

#if CONFIG_ENABLE_SPDIF

// 静态全局变量
static SpdifConfig_t g_spdif_cfg = {0};
static bool g_spdif_connected = false;
static SpdifFormat_e g_current_format = SPDIF_FORMAT_PCM;

// Amlogic SDK相关头文件
#include <aml_spdif.h>       // 晶晨SPDIF SDK
#include <aml_audio.h>       // 晶晨音频SDK

/**
 * @brief SPDIF信号检测回调函数
 */
static void spdif_signal_callback(int event, int format, int sample_rate, int bits)
{
    switch (event) {
        case SPDIF_EVENT_CONNECTED:
            LOG_INFO("SPDIF connected");
            g_spdif_connected = true;
            g_current_format = format;
            g_spdif_cfg.sample_rate = sample_rate;
            g_spdif_cfg.bits_per_sample = bits;
            break;
        case SPDIF_EVENT_DISCONNECTED:
            LOG_INFO("SPDIF disconnected");
            g_spdif_connected = false;
            g_current_format = SPDIF_FORMAT_PCM;
            break;
        case SPDIF_EVENT_FORMAT_CHANGED:
            LOG_INFO("SPDIF format changed: %d, sample rate: %d, bits: %d", format, sample_rate, bits);
            g_current_format = format;
            g_spdif_cfg.sample_rate = sample_rate;
            g_spdif_cfg.bits_per_sample = bits;
            break;
        default:
            break;
    }
}

/**
 * @brief SPDIF初始化
 */
int spdif_optical_init(SpdifConfig_t *cfg)
{
    // 初始化配置
    memset(&g_spdif_cfg, 0, sizeof(SpdifConfig_t));
    if (cfg != NULL) {
        memcpy(&g_spdif_cfg, cfg, sizeof(SpdifConfig_t));
    } else {
        // 默认配置
        g_spdif_cfg.auto_switch_en = true;
        g_spdif_cfg.sample_rate = 48000;
        g_spdif_cfg.bits_per_sample = 16;
    }
    
    // 1. 初始化Amlogic SPDIF
    if (aml_spdif_init() != 0) {
        LOG_ERROR("SPDIF init failed");
        return FAILURE;
    }
    
    // 2. 配置SPDIF
    aml_spdif_set_callback(spdif_signal_callback);
    
    // 3. 设置初始音频格式
    aml_spdif_set_format(SPDIF_FORMAT_PCM);
    
    // 4. 设置初始状态
    g_spdif_connected = false;
    g_current_format = SPDIF_FORMAT_PCM;
    
    LOG_INFO("SPDIF optical module init success");
    LOG_INFO("  Auto Switch: %s", g_spdif_cfg.auto_switch_en ? "ON" : "OFF");
    LOG_INFO("  Default Sample Rate: %dHz", g_spdif_cfg.sample_rate);
    LOG_INFO("  Default Bits: %dbits", g_spdif_cfg.bits_per_sample);
    
    return SUCCESS;
}

/**
 * @brief SPDIF反初始化
 */
int spdif_optical_deinit(void)
{
    // 关闭SPDIF
    aml_spdif_deinit();
    
    g_spdif_connected = false;
    g_current_format = SPDIF_FORMAT_PCM;
    
    LOG_INFO("SPDIF optical module deinit success");
    
    return SUCCESS;
}

/**
 * @brief 检测SPDIF信号
 */
bool spdif_optical_detect_signal(void)
{
    // 使用Amlogic SPDIF SDK检测信号
    g_spdif_connected = aml_spdif_detect_signal();
    
    LOG_INFO("SPDIF signal detected: %s", g_spdif_connected ? "YES" : "NO");
    
    return g_spdif_connected;
}

/**
 * @brief 设置SPDIF音频格式
 */
int spdif_optical_set_format(SpdifFormat_e format)
{
    // 验证参数
    if (format < SPDIF_FORMAT_PCM || format > SPDIF_FORMAT_AC3) {
        LOG_ERROR("Invalid SPDIF format: %d", format);
        return FAILURE;
    }
    
    // 使用Amlogic SPDIF SDK设置音频格式
    if (aml_spdif_set_format(format) != 0) {
        LOG_ERROR("Set SPDIF format failed");
        return FAILURE;
    }
    
    g_current_format = format;
    LOG_INFO("Set SPDIF format to %d", format);
    
    return SUCCESS;
}

/**
 * @brief 获取当前SPDIF状态
 */
bool spdif_optical_get_status(void)
{
    return g_spdif_connected;
}

/**
 * @brief 轮询处理SPDIF事件
 */
void spdif_optical_event_poll(void)
{
    // 轮询SPDIF事件
    aml_spdif_event_poll();
    
    // 自动切换检测
    if (g_spdif_cfg.auto_switch_en && g_spdif_connected) {
        // 检测SPDIF音频信号
        bool has_audio = aml_spdif_check_audio_present();
        
        if (has_audio) {
            // 获取当前音源
            int current_source = audio_source_get_current();
            
            // 如果当前不是SPDIF音源，则自动切换
            if (current_source != SOURCE_SPDIF) {
                LOG_INFO("SPDIF auto switch triggered");
                audio_source_switch(SOURCE_SPDIF);
            }
        }
    }
}

#endif
