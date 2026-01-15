#include "comm_mcu_priv.h"
#include "logger.h"
#include "product_type.h"

static int g_amp_init = 0;

// 功放控制仅中/高端支持
#if (CURRENT_PRODUCT_TYPE == PRODUCT_LOW_END) || (CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER)
int amp_ctrl_init(void) {
    g_amp_init = 0;
    return 0;
}

void amp_ctrl_deinit(void) {}
#else
int amp_ctrl_init(void) {
    g_amp_init = 1;
    LOG_INFO("AMP control init success");
    return 0;
}

void amp_ctrl_deinit(void) {
    g_amp_init = 0;
}
#endif