/*
 * uart_dma_demo.h
 *
 *  Created on: 2023-xx-xx
 *      Author: xx
 */

#ifndef _UART_DMA_DEMO_H
#define _UART_DMA_DEMO_H

#include "uart_dma.h"
#include "uart.h"

// 函数声明
bool uart_dma_init_demo(void);
bool uart_dma_send_demo(void);
bool uart_dma_receive_demo(void);
void uart_dma_demo(void);

#endif /* _UART_DMA_DEMO_H */