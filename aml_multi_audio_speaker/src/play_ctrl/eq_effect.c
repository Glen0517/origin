#include "play_ctrl_priv.h"
#include "logger.h"
#include "audio_core.h"
#include "product_type.h"

#include <aml_eq.h>          // 晶晨EQ SDK

static bool g_eq_effect_init = false;
static int g_eq_gain[9] = {0};       // 9个EQ频段的增益
static int g_eq_freq_band[9] = {0};  // 9个EQ频段的频率

/**
 * @brief EQ初始化
 */
int eq_effect_init(void) {
    if (g_eq_effect_init) {
        LOG_INFO("EQ effect already initialized");
        return 0;
    }
    
    // 初始化Amlogic EQ SDK
    if (aml_eq_init() != 0) {
        LOG_ERROR("EQ effect init failed: EQ SDK init error");
        return -1;
    }
    
    // 设置默认EQ参数
    int default_gain[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};  // 默认增益为0
    int default_freq[9] = {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000};  // 默认频率带
    
    // 设置EQ增益
    if (aml_eq_set_gain(default_gain, 9) != 0) {
        LOG_ERROR("EQ effect init failed: set gain error");
    }
    
    // 设置EQ频率带
    if (aml_eq_set_freq_band(default_freq, 9) != 0) {
        LOG_ERROR("EQ effect init failed: set freq band error");
    }
    
    // 启用EQ
    aml_eq_enable(true);
    
    // 更新全局变量
    memcpy(g_eq_gain, default_gain, sizeof(g_eq_gain));
    memcpy(g_eq_freq_band, default_freq, sizeof(g_eq_freq_band));
    g_eq_effect_init = true;
    
    LOG_INFO("EQ effect module init success");
    LOG_INFO("  Default EQ enabled: YES");
    
    return 0;
}

/**
 * @brief EQ反初始化
 */
void eq_effect_deinit(void) {
    if (g_eq_effect_init) {
        // 禁用EQ
        aml_eq_enable(false);
        
        // 反初始化EQ SDK
        aml_eq_deinit();
        
        g_eq_effect_init = false;
        
        LOG_INFO("EQ effect module deinit success");
    }
}

/**
 * @brief 设置EQ增益
 */
int eq_effect_set_gain(int *gain, int len) {
    if (!g_eq_effect_init || !gain || len != 9) {
        LOG_ERROR("EQ effect: set gain failed - invalid parameters");
        return -1;
    }
    
    // 设置EQ增益
    if (aml_eq_set_gain(gain, len) != 0) {
        LOG_ERROR("EQ effect: set gain failed");
        return -1;
    }
    
    // 更新全局变量
    memcpy(g_eq_gain, gain, sizeof(g_eq_gain));
    
    LOG_INFO("EQ effect: set gain success");
    LOG_INFO("  EQ gain: %d %d %d %d %d %d %d %d %d", 
             g_eq_gain[0], g_eq_gain[1], g_eq_gain[2], g_eq_gain[3], g_eq_gain[4],
             g_eq_gain[5], g_eq_gain[6], g_eq_gain[7], g_eq_gain[8]);
    
    return 0;
}

/**
 * @brief 设置EQ频率带
 */
int eq_effect_set_freq_band(int *freq, int len) {
    if (!g_eq_effect_init || !freq || len != 9) {
        LOG_ERROR("EQ effect: set freq band failed - invalid parameters");
        return -1;
    }
    
    // 设置EQ频率带
    if (aml_eq_set_freq_band(freq, len) != 0) {
        LOG_ERROR("EQ effect: set freq band failed");
        return -1;
    }
    
    // 更新全局变量
    memcpy(g_eq_freq_band, freq, sizeof(g_eq_freq_band));
    
    LOG_INFO("EQ effect: set freq band success");
    LOG_INFO("  EQ freq band: %d %d %d %d %d %d %d %d %d Hz", 
             g_eq_freq_band[0], g_eq_freq_band[1], g_eq_freq_band[2], g_eq_freq_band[3], g_eq_freq_band[4],
             g_eq_freq_band[5], g_eq_freq_band[6], g_eq_freq_band[7], g_eq_freq_band[8]);
    
    return 0;
}

/**
 * @brief 获取当前EQ增益
 */
int eq_effect_get_gain(int *gain, int len) {
    if (!g_eq_effect_init || !gain || len != 9) {
        return -1;
    }
    
    memcpy(gain, g_eq_gain, sizeof(g_eq_gain));
    
    return 0;
}

/**
 * @brief 获取当前EQ频率带
 */
int eq_effect_get_freq_band(int *freq, int len) {
    if (!g_eq_effect_init || !freq || len != 9) {
        return -1;
    }
    
    memcpy(freq, g_eq_freq_band, sizeof(g_eq_freq_band));
    
    return 0;
}
