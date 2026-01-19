#include "volume_ctrl_priv.h"
#include "logger.h"
#include "product_type.h"

static int g_chan_init = 0;

int channel_volume_init(void) {
    extern VolumeCtrl_t g_vol_cfg;
#if (CURRENT_PRODUCT_TYPE == PRODUCT_LOW_END_BT_SPEAKER) || (CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER)
    g_vol_cfg.track = VOLUME_TRACK_SINGLE;
#elif (CURRENT_PRODUCT_TYPE == PRODUCT_MID_END_SOUNDBAR)
    g_vol_cfg.track = VOLUME_TRACK_DOUBLE;
    g_vol_cfg.left = VOLUME_DEFAULT;
    g_vol_cfg.right = VOLUME_DEFAULT;
#else
    g_vol_cfg.track = VOLUME_TRACK_TRIPLE;
    g_vol_cfg.left = VOLUME_DEFAULT;
    g_vol_cfg.right = VOLUME_DEFAULT;
    g_vol_cfg.sub = VOLUME_DEFAULT + 5;
#endif
    g_chan_init = 1;
    LOG_INFO("Channel volume: track=%d", g_vol_cfg.track);
    return 0;
}

void channel_volume_deinit(void) {
    g_chan_init = 0;
}