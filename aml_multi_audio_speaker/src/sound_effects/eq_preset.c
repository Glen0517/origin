#include "sound_effects_priv.h"
#include "logger.h"
#include "product_type.h"

static int g_eq_init = 0;

#ifdef CONFIG_ENABLE_DOLBY_DTS
int eq_preset_init(void)
{
    g_eq_init = 1;
#if (CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END)
    LOG_INFO("Sound: EQ preset init (10 bands FULL)");
#else
    LOG_INFO("Sound: EQ preset init (5 bands BASIC)");
#endif
    return 0;
}

void eq_preset_deinit(void)
{
    g_eq_init = 0;
}
#else
int eq_preset_init(void) { return 0; }
void eq_preset_deinit(void) {}
#endif