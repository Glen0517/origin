#include "play_ctrl_priv.h"
#include "logger.h"

static int g_state_init = 0;

int play_state_init(void) {
    g_state_init = 1;
    LOG_DEBUG("Play state machine init success");
    return 0;
}

void play_state_deinit(void) {
    g_state_init = 0;
}