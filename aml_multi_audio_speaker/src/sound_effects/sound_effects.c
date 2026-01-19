#include "sound_effects.h"
#include "sound_effects_priv.h"
#include "logger.h"

static SoundEffectCfg_t g_se_cfg = {0};

#ifdef CONFIG_ENABLE_DOLBY_DTS
int sound_effects_init(SoundEffectsConfig_t *cfg)
{
    memset(&g_se_cfg, 0, sizeof(SoundEffectCfg_t));
    
    // 初始化配置参数
    if (cfg) {
        memcpy(g_se_cfg.eq_gain, cfg->eq_gain, sizeof(g_se_cfg.eq_gain));
        memcpy(g_se_cfg.freq_band, cfg->freq_band, sizeof(g_se_cfg.freq_band));
        g_se_cfg.dolby_en = cfg->dolby_en;
        g_se_cfg.virtual_5_1_en = cfg->virtual_5_1_en;
    }
    
    dolby_dts_init();
    eq_preset_init();
    g_se_cfg.init_ok = 1;
    LOG_INFO("Sound effects module init success (Dolby/DTS/EQ ENABLE)");
    return 0;
}

int sound_effects_deinit(void)
{
    if (g_se_cfg.init_ok)
    {
        eq_preset_deinit();
        dolby_dts_deinit();
        g_se_cfg.init_ok = 0;
        LOG_INFO("Sound effects module deinit success");
    }
    return 0;
}
#else
// 宏控裁剪：中低端无音效模块 空实现
int sound_effects_init(SoundEffectsConfig_t *cfg) { return 0; }
int sound_effects_deinit(void) {
    LOG_INFO("Sound effects deinit called");
    // 清理音效相关资源
    // 虽然是空实现，但保持函数接口一致
    return 0;
}
#endif