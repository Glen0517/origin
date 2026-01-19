#include "volume_ctrl.h"
#include "volume_ctrl_priv.h"
#include "logger.h"

static VolumeCtrl_t g_vol_cfg = {0};

int volume_ctrl_init(VolumeInfo_t *vol) {
    memset(&g_vol_cfg, 0, sizeof(VolumeCtrl_t));
    main_volume_init();
    channel_volume_init();
    
    // 使用传入的音量值初始化，如果没有传入则使用默认值
    if (vol) {
        g_vol_cfg.main_vol = vol->master_volume;
        g_vol_cfg.bass_vol = vol->bass_volume;
        g_vol_cfg.treble_vol = vol->treble_volume;
        g_vol_cfg.mute = vol->is_mute ? 1 : 0;
    } else {
        g_vol_cfg.main_vol = VOLUME_DEFAULT;
        g_vol_cfg.bass_vol = VOLUME_DEFAULT;
        g_vol_cfg.treble_vol = VOLUME_DEFAULT;
        g_vol_cfg.mute = 0;
    }
    
    g_vol_cfg.init_ok = 1;
    LOG_INFO("Volume control init success");
    return SUCCESS;
}

int volume_ctrl_deinit(void) {
    if (g_vol_cfg.init_ok) {
        channel_volume_deinit();
        main_volume_deinit();
        g_vol_cfg.init_ok = 0;
        LOG_INFO("Volume control deinit success");
    }
    return SUCCESS;
}

int volume_ctrl_set_mute(bool is_mute) {
    if (!g_vol_cfg.init_ok) return FAILURE;
    g_vol_cfg.mute = is_mute ? 1 : 0;
    LOG_INFO("Mute: %d", g_vol_cfg.mute);
    return SUCCESS;
}

int volume_ctrl_get_main(void) {
    return g_vol_cfg.init_ok ? g_vol_cfg.main_vol : VOLUME_DEFAULT;
}

int volume_ctrl_set_master(int vol) {
    if (!g_vol_cfg.init_ok) return FAILURE;
    
    // 检查音量值是否在有效范围内
    if (vol < MIN_VOLUME_VAL || vol > MAX_VOLUME_VAL) {
        return INVALID_PARAM;
    }
    
    g_vol_cfg.main_vol = vol;
    LOG_INFO("Set master volume to %d", vol);
    return SUCCESS;
}

int volume_ctrl_master_up(void) {
    if (!g_vol_cfg.init_ok) return VOLUME_DEFAULT;
    
    // 增加音量，确保不超过最大值
    if (g_vol_cfg.main_vol < MAX_VOLUME_VAL) {
        g_vol_cfg.main_vol += VOLUME_STEP;
        LOG_INFO("Master volume up to %d", g_vol_cfg.main_vol);
    }
    
    return g_vol_cfg.main_vol;
}

int volume_ctrl_master_down(void) {
    if (!g_vol_cfg.init_ok) return VOLUME_DEFAULT;
    
    // 减少音量，确保不低于最小值
    if (g_vol_cfg.main_vol > MIN_VOLUME_VAL) {
        g_vol_cfg.main_vol -= VOLUME_STEP;
        LOG_INFO("Master volume down to %d", g_vol_cfg.main_vol);
    }
    
    return g_vol_cfg.main_vol;
}

int volume_ctrl_get_info(VolumeInfo_t *vol) {
    if (!g_vol_cfg.init_ok || !vol) return FAILURE;
    
    // 填充音量信息
    vol->master_volume = g_vol_cfg.main_vol;
    vol->bass_volume = g_vol_cfg.bass_vol;
    vol->treble_volume = g_vol_cfg.treble_vol;
    vol->is_mute = (g_vol_cfg.mute == 1);
    
    LOG_INFO("Get volume info: master=%d, bass=%d, treble=%d, mute=%d", 
             g_vol_cfg.main_vol, g_vol_cfg.bass_vol, g_vol_cfg.treble_vol, g_vol_cfg.mute);
    
    return SUCCESS;
}

#if CONFIG_ENABLE_2VOL_CTRL || CONFIG_ENABLE_3VOL_CTRL
/**
 * @brief  设置低音音量
 * @param  vol 音量值 0~20
 * @return SUCCESS/FAILURE
 */
int volume_ctrl_set_bass(int vol) {
    if (!g_vol_cfg.init_ok) return FAILURE;
    
    // 检查音量值是否在有效范围内
    if (vol < MIN_VOLUME_VAL || vol > 20) {
        return INVALID_PARAM;
    }
    
    g_vol_cfg.bass_vol = vol;
    LOG_INFO("Set bass volume to %d", vol);
    return SUCCESS;
}

/**
 * @brief  低音音量加
 * @return 加后的音量值
 */
int volume_ctrl_bass_up(void) {
    if (!g_vol_cfg.init_ok) return VOLUME_DEFAULT;
    
    // 增加音量，确保不超过最大值
    if (g_vol_cfg.bass_vol < 20) {
        g_vol_cfg.bass_vol += VOLUME_STEP;
        LOG_INFO("Bass volume up to %d", g_vol_cfg.bass_vol);
    }
    
    return g_vol_cfg.bass_vol;
}

/**
 * @brief  低音音量减
 * @return 减后的音量值
 */
int volume_ctrl_bass_down(void) {
    if (!g_vol_cfg.init_ok) return VOLUME_DEFAULT;
    
    // 减少音量，确保不低于最小值
    if (g_vol_cfg.bass_vol > MIN_VOLUME_VAL) {
        g_vol_cfg.bass_vol -= VOLUME_STEP;
        LOG_INFO("Bass volume down to %d", g_vol_cfg.bass_vol);
    }
    
    return g_vol_cfg.bass_vol;
}
#endif

#if CONFIG_ENABLE_3VOL_CTRL
/**
 * @brief  设置高音音量
 * @param  vol 音量值 0~20
 * @return SUCCESS/FAILURE
 */
int volume_ctrl_set_treble(int vol) {
    if (!g_vol_cfg.init_ok) return FAILURE;
    
    // 检查音量值是否在有效范围内
    if (vol < MIN_VOLUME_VAL || vol > 20) {
        return INVALID_PARAM;
    }
    
    g_vol_cfg.treble_vol = vol;
    LOG_INFO("Set treble volume to %d", vol);
    return SUCCESS;
}

/**
 * @brief  高音音量加
 * @return 加后的音量值
 */
int volume_ctrl_treble_up(void) {
    if (!g_vol_cfg.init_ok) return VOLUME_DEFAULT;
    
    // 增加音量，确保不超过最大值
    if (g_vol_cfg.treble_vol < 20) {
        g_vol_cfg.treble_vol += VOLUME_STEP;
        LOG_INFO("Treble volume up to %d", g_vol_cfg.treble_vol);
    }
    
    return g_vol_cfg.treble_vol;
}

/**
 * @brief  高音音量减
 * @return 减后的音量值
 */
int volume_ctrl_treble_down(void) {
    if (!g_vol_cfg.init_ok) return VOLUME_DEFAULT;
    
    // 减少音量，确保不低于最小值
    if (g_vol_cfg.treble_vol > MIN_VOLUME_VAL) {
        g_vol_cfg.treble_vol -= VOLUME_STEP;
        LOG_INFO("Treble volume down to %d", g_vol_cfg.treble_vol);
    }
    
    return g_vol_cfg.treble_vol;
}
#endif