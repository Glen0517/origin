#include "play_ctrl_priv.h"
#include "logger.h"
#include "audio_core.h"
#include "product_type.h"

#include <aml_sound_field.h>   // 晶晨声场SDK

static bool g_sound_field_init = false;
static SoundMode_e g_current_sound_mode = SOUND_MODE_NORMAL;

// 不同声场模式对应的EQ增益配置
static const int g_eq_gain_normal[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};     // 正常模式
static const int g_eq_gain_cinema[9] = {5, 4, 3, 2, 1, 2, 3, 4, 5};     // 影院模式
static const int g_eq_gain_music[9] = {3, 2, 1, 0, 0, 0, 1, 2, 3};     // 音乐模式
static const int g_eq_gain_game[9] = {4, 3, 2, 1, 1, 2, 3, 4, 5};      // 游戏模式
static const int g_eq_gain_news[9] = {-2, -1, 0, 1, 5, 3, 2, 1, 0};     // 新闻模式
static const int g_eq_gain_bass_only[9] = {8, 7, 6, 5, 0, 0, 0, 0, 0};  // 低音炮专属模式

/**
 * @brief 声场模式切换函数
 */
static int sound_field_switch_mode(SoundMode_e mode) {
    if (!g_sound_field_init) {
        LOG_ERROR("Sound field: not initialized");
        return -1;
    }
    
    int result = 0;
    
    // 根据不同的声场模式设置相应的音效
    switch (mode) {
        case SOUND_MODE_NORMAL:
            // 关闭虚拟5.1和其他特殊音效，使用标准立体声
            aml_sound_field_set_virtual_5_1(false);
            aml_sound_field_set_bass_boost(false);
            aml_sound_field_set_treble_boost(false);
            
            // 设置正常模式的EQ增益
            aml_eq_set_gain(g_eq_gain_normal, 9);
            break;
            
        case SOUND_MODE_CINEMA:
            // 开启虚拟5.1环绕音效
            aml_sound_field_set_virtual_5_1(true);
            aml_sound_field_set_bass_boost(true);
            aml_sound_field_set_treble_boost(false);
            
            // 设置影院模式的EQ增益
            aml_eq_set_gain(g_eq_gain_cinema, 9);
            break;
            
        case SOUND_MODE_MUSIC:
            // 关闭虚拟5.1，优化音乐播放
            aml_sound_field_set_virtual_5_1(false);
            aml_sound_field_set_bass_boost(true);
            aml_sound_field_set_treble_boost(true);
            
            // 设置音乐模式的EQ增益
            aml_eq_set_gain(g_eq_gain_music, 9);
            break;
            
        case SOUND_MODE_GAME:
            // 开启虚拟5.1和低音增强
            aml_sound_field_set_virtual_5_1(true);
            aml_sound_field_set_bass_boost(true);
            aml_sound_field_set_treble_boost(true);
            
            // 设置游戏模式的EQ增益
            aml_eq_set_gain(g_eq_gain_game, 9);
            break;
            
        case SOUND_MODE_NEWS:
            // 关闭虚拟5.1，增强人声
            aml_sound_field_set_virtual_5_1(false);
            aml_sound_field_set_bass_boost(false);
            aml_sound_field_set_treble_boost(false);
            
            // 设置新闻模式的EQ增益
            aml_eq_set_gain(g_eq_gain_news, 9);
            break;
            
        case SOUND_MODE_BASS_ONLY:
            // 关闭虚拟5.1，只保留低音
            aml_sound_field_set_virtual_5_1(false);
            aml_sound_field_set_bass_boost(true);
            aml_sound_field_set_treble_boost(false);
            
            // 设置低音炮专属模式的EQ增益
            aml_eq_set_gain(g_eq_gain_bass_only, 9);
            break;
            
        default:
            LOG_ERROR("Sound field: invalid mode %d", mode);
            result = -1;
            break;
    }
    
    if (result == 0) {
        g_current_sound_mode = mode;
        LOG_INFO("Sound field: switched to %d", mode);
    }
    
    return result;
}

#ifdef CONFIG_ENABLE_SOUND_FIELD
int sound_field_init(void) {
    if (g_sound_field_init) {
        LOG_INFO("Sound field already initialized");
        return 0;
    }
    
    // 初始化Amlogic声场SDK
    if (aml_sound_field_init() != 0) {
        LOG_ERROR("Sound field init failed: Sound field SDK init error");
        return -1;
    }
    
    // 默认使用正常模式
    sound_field_switch_mode(SOUND_MODE_NORMAL);
    
    g_sound_field_init = true;
    
    LOG_INFO("Sound field module init success");
    LOG_INFO("  Current mode: %d (NORMAL)", g_current_sound_mode);
    
    return 0;
}

void sound_field_deinit(void) {
    if (g_sound_field_init) {
        // 反初始化Amlogic声场SDK
        aml_sound_field_deinit();
        
        g_sound_field_init = false;
        g_current_sound_mode = SOUND_MODE_NORMAL;
        
        LOG_INFO("Sound field module deinit success");
    }
}

/**
 * @brief 设置声场模式
 */
int sound_field_set_mode(SoundMode_e mode) {
    return sound_field_switch_mode(mode);
}

/**
 * @brief 获取当前声场模式
 */
SoundMode_e sound_field_get_mode(void) {
    return g_current_sound_mode;
}

#else
int sound_field_init(void) { return 0; }
void sound_field_deinit(void) {
    LOG_INFO("Sound field deinit called");
    // 清理声场相关资源
    // 虽然是空实现，但保持函数接口一致
}
int sound_field_set_mode(SoundMode_e mode) { return 0; }
SoundMode_e sound_field_get_mode(void) { return SOUND_MODE_NORMAL; }
#endif