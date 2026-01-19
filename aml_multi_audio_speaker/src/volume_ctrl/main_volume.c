#include "volume_ctrl_priv.h"
#include "logger.h"

static int g_main_init = 0;

int main_volume_init(void) {
    extern VolumeCtrl_t g_vol_cfg;
    g_vol_cfg.main_vol = VOLUME_DEFAULT;
    g_main_init = 1;
    LOG_DEBUG("Main volume init: %d", VOLUME_DEFAULT);
    return 0;
}

void main_volume_deinit(void) {
    g_main_init = 0;
}

int volume_ctrl_set_master(int vol) {
    extern VolumeCtrl_t g_vol_cfg;
    if (!g_main_init || vol < MIN_VOLUME_VAL || vol > MAX_VOLUME_VAL) {
        LOG_ERROR("Set main vol failed: %d", vol);
        return INVALID_PARAM;
    }
    g_vol_cfg.main_vol = vol;
    LOG_DEBUG("Main vol: %d", vol);
    return SUCCESS;
}