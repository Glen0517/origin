#include "bluetooth_priv.h"
#include "logger.h"

static int g_bt_a2dp_init = 0;

int bt_a2dp_init(void)
{
    g_bt_a2dp_init = 1;
    LOG_INFO("Bluetooth: A2DP core init success [ALL PRODUCT]");
    return 0;
}

void bt_a2dp_deinit(void)
{
    g_bt_a2dp_init = 0;
}