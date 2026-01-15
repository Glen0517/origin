#include "sound_effects_priv.h"
#include "logger.h"
#include "product_type.h"

static int g_dolby_dts_init = 0;

#ifdef CONFIG_ENABLE_DOLBY_DTS
int dolby_dts_init(void)
{
    g_dolby_dts_init = 1;
    LOG_INFO("Sound: Dolby/DTS decode enhance init success (HIGH END)");
    return 0;
}

void dolby_dts_deinit(void)
{
    g_dolby_dts_init = 0;
}
#else
int dolby_dts_init(void) { return 0; }
void dolby_dts_deinit(void) {}
#endif