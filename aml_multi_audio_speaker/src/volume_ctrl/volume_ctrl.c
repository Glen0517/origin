#include "volume_ctrl.h"
#include "volume_ctrl_priv.h"
#include "logger.h"

static VolumeCtrl_t g_vol_cfg = {0};

int volume_ctrl_init(void) {
    memset(&g_vol_cfg, 0, sizeof(VolumeCtrl_t));
    main_volume_init();
    channel_volume_init();
    g_vol_cfg.mute = 0;
    g_vol_cfg.init_ok = 1;
    LOG_INFO("Volume control init success");
    return 0;
}

void volume_ctrl_deinit(void) {
    if (g_vol_cfg.init_ok) {
        channel_volume_deinit();
        main_volume_deinit();
        g_vol_cfg.init_ok = 0;
        LOG_INFO("Volume control deinit success");
    }
}

int volume_ctrl_mute(int mute) {
    if (!g_vol_cfg.init_ok) return -1;
    g_vol_cfg.mute = mute ? 1 : 0;
    LOG_INFO("Mute: %d", g_vol_cfg.mute);
    return 0;
}

int volume_ctrl_get_main(void) {
    return g_vol_cfg.init_ok ? g_vol_cfg.main_vol : VOLUME_DEFAULT;
}