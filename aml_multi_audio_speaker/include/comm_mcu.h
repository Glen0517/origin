#ifndef __UART_MCU_COMM_H__
#define __UART_MCU_COMM_H__

#include "common_def.h"

#define UART_DEV_PATH       "/dev/ttyS1"
#define UART_BAUDRATE       115200
#define UART_DATA_BITS      8
#define UART_STOP_BITS      1
#define UART_PARITY         0

int uart_mcu_comm_init(void);
int uart_mcu_comm_deinit(void);
int uart_mcu_send_cmd(uint8_t *cmd, int len);
int uart_mcu_recv_data(uint8_t *buf, int len, int timeout_ms);

#endif // __UART_MCU_COMM_H__