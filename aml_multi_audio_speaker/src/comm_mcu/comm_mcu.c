#include "comm_mcu.h"
#include "comm_mcu_priv.h"
#include "logger.h"

static CommMcuCfg_t g_mcu_cfg = {0};

int comm_mcu_init(void) {
    memset(&g_mcu_cfg, 0, sizeof(CommMcuCfg_t));
    uart_txrx_init();
    amp_ctrl_init();
    g_mcu_cfg.baud = 9600;
    g_mcu_cfg.init_ok = 1;
    LOG_INFO("Comm MCU init: 9600bps");
    return 0;
}

void comm_mcu_deinit(void) {
    if (g_mcu_cfg.init_ok) {
        amp_ctrl_deinit();
        uart_txrx_deinit();
        g_mcu_cfg.init_ok = 0;
        LOG_INFO("Comm MCU deinit success");
    }
}

int comm_mcu_send(unsigned char *data, int len) {
    if (!g_mcu_cfg.init_ok || !data || len <= 0) {
        LOG_ERROR("Send data failed: invalid param");
        return -1;
    }
    LOG_DEBUG("Send %d bytes to MCU", len);
    return len;
}