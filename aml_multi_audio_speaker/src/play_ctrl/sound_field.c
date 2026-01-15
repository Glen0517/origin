#include "play_ctrl_priv.h"
#include "logger.h"
#include "product_type.h"

static int g_field_init = 0;

#ifdef CONFIG_ENABLE_SOUND_FIELD
int sound_field_init(void) {
    g_field_init = 1;
    LOG_INFO("Sound field: stereo (ENABLED)");
    return 0;
}

void sound_field_deinit(void) {
    g_field_init = 0;
}
#else
int sound_field_init(void) { return 0; }
void sound_field_deinit(void) {}
#endif