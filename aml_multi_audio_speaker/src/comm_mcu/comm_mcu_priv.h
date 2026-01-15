#ifndef __COMM_MCU_PRIV_H__
#define __COMM_MCU_PRIV_H__

#include "comm_mcu.h"
#include "product_type.h"

// 内部函数声明
int uart_txrx_init(void);
void uart_txrx_deinit(void);
int amp_ctrl_init(void);
void amp_ctrl_deinit(void);

#endif // __COMM_MCU_PRIV_H__