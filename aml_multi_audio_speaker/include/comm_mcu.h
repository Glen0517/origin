#ifndef __UART_MCU_COMM_H__
#define __UART_MCU_COMM_H__

#include "common_def.h"

#define UART_DEV_PATH       "/dev/ttyS0"
#define UART_BAUDRATE       9600
#define UART_DATA_BITS      8
#define UART_STOP_BITS      1
#define UART_PARITY         0

int comm_mcu_init(void);
void comm_mcu_deinit(void);
int comm_mcu_send(unsigned char *data, int len);

#endif // __UART_MCU_COMM_H__