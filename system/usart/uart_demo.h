/*
* @file    usart_demo.h
* @brief   UART驱动使用示例头文件
* @author  AI Assistant
* @date    2023-11-10
*/
#ifndef _USART_DEMO_H
#define _USART_DEMO_H

#include "uart.h"
//#include <stdio.h>

#define STM32_UART_FLAG 1

// 定义UART实例（根据实际硬件配置修改）
#define USARTx_INSTANCE USART1

// 缓冲区大小定义（根据实际需求调整）
#define DEMO_TX_BUFFER_SIZE 128
#define DEMO_RX_BUFFER_SIZE 128

// 缓冲区大小定义
#define DEMO_TX_BUFFER_SIZE 128
#define DEMO_RX_BUFFER_SIZE 128

// 函数声明
void uart_demo(void);
void uart_send_demo(void);
void uart_receive_demo(void);
void hal_uart_mspInit(void);

#endif /* _USART_DEMO_H */