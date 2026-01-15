#include "comm_mcu_priv.h"
#include "logger.h"

static int g_uart_init = 0;

int uart_txrx_init(void) {
    g_uart_init = 1;
    LOG_DEBUG("UART TX/RX init success");
    return 0;
}

void uart_txrx_deinit(void) {
    g_uart_init = 0;
}