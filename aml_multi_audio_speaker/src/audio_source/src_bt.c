#include "audio_source_priv.h"
#include "logger.h"

int src_bt_init(void) {
    LOG_INFO("Bluetooth (A2DP) source init success");
    return 0;
}

void src_bt_deinit(void) {}