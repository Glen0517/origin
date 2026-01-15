#include "peripheral_priv.h"
#include "logger.h"

static int g_led_init = 0;

int led_ctrl_init(void)
{
    g_led_init = 1;
    LOG_INFO("Peripheral: LED control init success [ALL PRODUCT]");
    return 0;
}

void led_ctrl_deinit(void)
{
    g_led_init = 0;
}