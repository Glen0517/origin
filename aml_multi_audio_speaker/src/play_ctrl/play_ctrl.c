#include "play_ctrl.h"
#include "play_ctrl_priv.h"
#include "logger.h"

static PlayCtrl_t g_play_cfg = {0};

int play_ctrl_init(void) {
    memset(&g_play_cfg, 0, sizeof(PlayCtrl_t));
    play_state_init();
#ifdef CONFIG_ENABLE_SOUND_FIELD
    sound_field_init();
#endif
    g_play_cfg.state = PLAY_STATE_STOP;
    g_play_cfg.init_ok = 1;
    LOG_INFO("Play control init success");
    return 0;
}

void play_ctrl_deinit(void) {
    if (g_play_cfg.init_ok) {
#ifdef CONFIG_ENABLE_SOUND_FIELD
        sound_field_deinit();
#endif
        play_state_deinit();
        g_play_cfg.init_ok = 0;
        LOG_INFO("Play control deinit success");
    }
}

void play_ctrl_play(void) {
    if (!g_play_cfg.init_ok) return;
    g_play_cfg.state = PLAY_STATE_PLAY;
    LOG_INFO("Play: start");
}

void play_ctrl_pause(void) {
    if (!g_play_cfg.init_ok) return;
    g_play_cfg.state = PLAY_STATE_PAUSE;
    LOG_INFO("Play: pause");
}

void play_ctrl_event_poll(void) {
    if (!g_play_cfg.init_ok) return;
}