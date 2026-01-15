#include "sound_effects.h"
#include "sound_effects_priv.h"
#include "logger.h"

static SoundEffectCfg_t g_se_cfg = {0};

#ifdef CONFIG_ENABLE_DOLBY_DTS
int sound_effects_init(void)
{
    memset(&g_se_cfg, 0, sizeof(SoundEffectCfg_t));
    dolby_dts_init();
    eq_preset_init();
    g_se_cfg.init_ok = 1;
    LOG_INFO("Sound effects module init success (Dolby/DTS/EQ ENABLE)");
    return 0;
}

void sound_effects_deinit(void)
{
    if (g_se_cfg.init_ok)
    {
        eq_preset_deinit();
        dolby_dts_deinit();
        g_se_cfg.init_ok = 0;
        LOG_INFO("Sound effects module deinit success");
    }
}
#else
// 宏控裁剪：中低端无音效模块 空实现
int sound_effects_init(void) { return 0; }
void sound_effects_deinit(void) {}
#endif