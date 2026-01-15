#include "peripheral_priv.h"
#include "logger.h"
#include "product_type.h"
#include "res_manager.h"

#include <aml_audio.h>        // 晶晨音频SDK
#include <aml_res.h>          // 晶晨资源管理SDK

static bool g_prompt_init = false;
static bool g_prompt_playing = false;

// 提示音ID定义
typedef enum {
    PROMPT_SOUND_POWER_ON = 1,
    PROMPT_SOUND_POWER_OFF,
    PROMPT_SOUND_BT_CONNECT,
    PROMPT_SOUND_BT_DISCONNECT,
    PROMPT_SOUND_VOLUME_UP,
    PROMPT_SOUND_VOLUME_DOWN,
    PROMPT_SOUND_SOURCE_SWITCH,
    PROMPT_SOUND_ERROR,
    PROMPT_SOUND_MAX
} PromptSoundId_e;

// 提示音资源路径
static const char *g_prompt_sound_paths[PROMPT_SOUND_MAX] = {
    NULL,
    "res/sounds/power_on.wav",
    "res/sounds/power_off.wav",
    "res/sounds/bt_connect.wav",
    "res/sounds/bt_disconnect.wav",
    "res/sounds/volume_up.wav",
    "res/sounds/volume_down.wav",
    "res/sounds/source_switch.wav",
    "res/sounds/error.wav",
};

// 提示音资源句柄
static void *g_prompt_sound_res[PROMPT_SOUND_MAX] = {NULL};

// 提示音音量（百分比）
#define PROMPT_SOUND_VOLUME   60

/**
 * @brief 提示音播放完成回调函数
 */
static void prompt_sound_complete_callback(void)
{
    g_prompt_playing = false;
    LOG_DEBUG("Prompt sound play completed");
}

int prompt_sound_init(void)
{
    if (g_prompt_init) {
        LOG_INFO("Prompt sound already initialized");
        return 0;
    }
    
    g_prompt_init = false;
    g_prompt_playing = false;
    
    // 检查音频模块是否初始化
    if (!aml_audio_is_init()) {
        LOG_ERROR("Audio module not initialized, cannot initialize prompt sound");
        return -1;
    }
    
    // 初始化资源管理
    if (aml_res_init() != 0) {
        LOG_ERROR("Resource manager init failed");
        return -1;
    }
    
    // 根据产品类型加载不同数量的提示音资源
#if (CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END)
    // 高端产品：加载所有提示音
    LOG_INFO("Peripheral: Prompt sound init (full resource load)");
    for (int i = 1; i < PROMPT_SOUND_MAX; i++) {
        g_prompt_sound_res[i] = aml_res_load(g_prompt_sound_paths[i]);
        if (!g_prompt_sound_res[i]) {
            LOG_ERROR("Failed to load prompt sound: %s", g_prompt_sound_paths[i]);
            // 继续加载其他提示音
        } else {
            LOG_DEBUG("Loaded prompt sound: %s", g_prompt_sound_paths[i]);
        }
    }
    
#elif (CURRENT_PRODUCT_TYPE == PRODUCT_MID_END)
    // 中端产品：加载部分提示音
    LOG_INFO("Peripheral: Prompt sound init (mid resource load)");
    int mid_end_sounds[] = {
        PROMPT_SOUND_POWER_ON,
        PROMPT_SOUND_POWER_OFF,
        PROMPT_SOUND_BT_CONNECT,
        PROMPT_SOUND_BT_DISCONNECT,
        PROMPT_SOUND_VOLUME_UP,
        PROMPT_SOUND_VOLUME_DOWN,
        PROMPT_SOUND_SOURCE_SWITCH
    };
    
    for (int i = 0; i < sizeof(mid_end_sounds)/sizeof(mid_end_sounds[0]); i++) {
        int sound_id = mid_end_sounds[i];
        g_prompt_sound_res[sound_id] = aml_res_load(g_prompt_sound_paths[sound_id]);
        if (!g_prompt_sound_res[sound_id]) {
            LOG_ERROR("Failed to load prompt sound: %s", g_prompt_sound_paths[sound_id]);
        } else {
            LOG_DEBUG("Loaded prompt sound: %s", g_prompt_sound_paths[sound_id]);
        }
    }
    
#else
    // 低端产品：仅加载基本提示音
    LOG_INFO("Peripheral: Prompt sound init (basic resource load) [ALL PRODUCT]");
    int basic_sounds[] = {
        PROMPT_SOUND_POWER_ON,
        PROMPT_SOUND_POWER_OFF,
        PROMPT_SOUND_ERROR
    };
    
    for (int i = 0; i < sizeof(basic_sounds)/sizeof(basic_sounds[0]); i++) {
        int sound_id = basic_sounds[i];
        g_prompt_sound_res[sound_id] = aml_res_load(g_prompt_sound_paths[sound_id]);
        if (!g_prompt_sound_res[sound_id]) {
            LOG_ERROR("Failed to load prompt sound: %s", g_prompt_sound_paths[sound_id]);
        } else {
            LOG_DEBUG("Loaded prompt sound: %s", g_prompt_sound_paths[sound_id]);
        }
    }
#endif
    
    g_prompt_init = true;
    
    LOG_INFO("Prompt sound init success");
    
    return 0;
}

void prompt_sound_deinit(void)
{
    if (!g_prompt_init) {
        return;
    }
    
    // 停止正在播放的提示音
    if (g_prompt_playing) {
        aml_audio_stop();
        g_prompt_playing = false;
    }
    
    // 释放所有提示音资源
    for (int i = 1; i < PROMPT_SOUND_MAX; i++) {
        if (g_prompt_sound_res[i]) {
            aml_res_unload(g_prompt_sound_res[i]);
            g_prompt_sound_res[i] = NULL;
        }
    }
    
    // 反初始化资源管理
    aml_res_deinit();
    
    g_prompt_init = false;
    
    LOG_INFO("Prompt sound deinitialized");
}

/**
 * @brief 播放提示音
 */
void prompt_sound_play(int sound_id)
{
    if (!g_prompt_init || sound_id <= 0 || sound_id >= PROMPT_SOUND_MAX) {
        LOG_ERROR("Invalid prompt sound ID or not initialized");
        return;
    }
    
    // 检查提示音资源是否存在
    if (!g_prompt_sound_res[sound_id]) {
        LOG_ERROR("Prompt sound resource not loaded: %d", sound_id);
        return;
    }
    
    // 如果正在播放提示音，先停止
    if (g_prompt_playing) {
        aml_audio_stop();
    }
    
    // 设置提示音音量
    int current_volume = aml_audio_get_volume();
    aml_audio_set_volume(PROMPT_SOUND_VOLUME);
    
    // 播放提示音
    if (aml_audio_play_wav(g_prompt_sound_res[sound_id], prompt_sound_complete_callback) == 0) {
        g_prompt_playing = true;
        LOG_INFO("Playing prompt sound: %d", sound_id);
    } else {
        LOG_ERROR("Failed to play prompt sound: %d", sound_id);
    }
    
    // 恢复原音量
    aml_audio_set_volume(current_volume);
}