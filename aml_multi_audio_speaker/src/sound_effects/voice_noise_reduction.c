#include "sound_effects_priv.h"
#include "logger.h"
#include "product_type.h"

#include <aml_voice.h>       // 晶晨语音处理SDK

#ifdef CONFIG_ENABLE_VOICE_NOISE_REDUCTION

static bool g_voice_nr_init = false;
static bool g_voice_nr_enabled = false;

/**
 * @brief 语音降噪初始化
 */
int voice_noise_reduction_init(void)
{
    if (g_voice_nr_init) {
        LOG_INFO("Voice noise reduction already initialized");
        return 0;
    }
    
    // 初始化Amlogic语音处理SDK
    if (aml_voice_init() != 0) {
        LOG_ERROR("Voice noise reduction init failed");
        return -1;
    }
    
    // 配置语音降噪参数
    aml_voice_set_noise_reduction_level(3); // 设置降噪等级为3（中等）
    aml_voice_set_echo_cancellation(true);  // 开启回声消除
    aml_voice_set_voice_enhancement(true);  // 开启语音增强
    
    g_voice_nr_enabled = true;
    g_voice_nr_init = true;
    
    LOG_INFO("Voice noise reduction module init success");
    LOG_INFO("  Noise reduction: enabled");
    LOG_INFO("  Echo cancellation: enabled");
    LOG_INFO("  Voice enhancement: enabled");
    
    return 0;
}

/**
 * @brief 语音降噪反初始化
 */
void voice_noise_reduction_deinit(void)
{
    if (g_voice_nr_init) {
        // 反初始化Amlogic语音处理SDK
        aml_voice_deinit();
        
        g_voice_nr_enabled = false;
        g_voice_nr_init = false;
        
        LOG_INFO("Voice noise reduction module deinit success");
    }
}

/**
 * @brief 设置语音降噪状态
 */
int voice_noise_reduction_set_enabled(bool enabled)
{
    if (!g_voice_nr_init) {
        LOG_ERROR("Voice noise reduction not initialized");
        return -1;
    }
    
    g_voice_nr_enabled = enabled;
    
    if (enabled) {
        aml_voice_enable_noise_reduction(true);
        aml_voice_enable_echo_cancellation(true);
        aml_voice_enable_voice_enhancement(true);
        LOG_INFO("Voice noise reduction enabled");
    } else {
        aml_voice_enable_noise_reduction(false);
        aml_voice_enable_echo_cancellation(false);
        aml_voice_enable_voice_enhancement(false);
        LOG_INFO("Voice noise reduction disabled");
    }
    
    return 0;
}

/**
 * @brief 处理语音数据
 */
int voice_noise_reduction_process(uint8_t *input_data, int input_len, uint8_t *output_data, int *output_len)
{
    if (!g_voice_nr_init || !g_voice_nr_enabled || !input_data || input_len <= 0 || !output_data || !output_len) {
        return -1;
    }
    
    int processed_len = aml_voice_process(input_data, input_len, output_data, *output_len);
    
    if (processed_len > 0) {
        *output_len = processed_len;
        return 0;
    }
    
    // 如果处理失败，直接复制数据
    if (*output_len >= input_len) {
        memcpy(output_data, input_data, input_len);
        *output_len = input_len;
        return 0;
    }
    
    return -1;
}

/**
 * @brief 设置语音降噪等级
 */
int voice_noise_reduction_set_level(int level)
{
    if (!g_voice_nr_init) {
        LOG_ERROR("Voice noise reduction not initialized");
        return -1;
    }
    
    if (level < 0 || level > 5) {
        LOG_ERROR("Invalid noise reduction level: %d, range [0-5]", level);
        return -1;
    }
    
    aml_voice_set_noise_reduction_level(level);
    LOG_INFO("Voice noise reduction level set to: %d", level);
    
    return 0;
}

#else

// 空实现
int voice_noise_reduction_init(void) { return 0; }
void voice_noise_reduction_deinit(void) {}
int voice_noise_reduction_set_enabled(bool enabled) { return 0; }
int voice_noise_reduction_process(uint8_t *input_data, int input_len, uint8_t *output_data, int *output_len) { return -1; }
int voice_noise_reduction_set_level(int level) { return 0; }

#endif
