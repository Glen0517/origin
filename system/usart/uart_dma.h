/*
 * uart_dma.h
 *
 *  Created on: 2023-xx-xx
 *      Author: xx
 */

#ifndef _UART_DMA_H
#define _UART_DMA_H

//#include "stm32f4xx_hal.h"
#include "uart.h"

// DMA缓冲区大小定义
#define UART_DMA_TX_BUFFER_SIZE  1024
#define UART_DMA_RX_BUFFER_SIZE  1024


// UART DMA句柄结构体
typedef struct {
    UART_HandleTypeDef *huart;       // UART句柄
    DMA_HandleTypeDef hdma_tx;       // TX DMA句柄
    DMA_HandleTypeDef hdma_rx;       // RX DMA句柄
    uint8_t tx_buffer[UART_DMA_TX_BUFFER_SIZE]; // TX DMA缓冲区
    uint8_t rx_buffer[UART_DMA_RX_BUFFER_SIZE]; // RX DMA缓冲区
    uint32_t rx_data_len;            // 接收到的数据长度
    bool tx_complete;                // 发送完成标志
    bool rx_complete;                // 接收完成标志

    void (*uart_dma_init)(UART_DMA_STRUCT *huart_dma, UART_HandleTypeDef *huart);
    void (*uart_dma_send)(UART_DMA_STRUCT *huart_dma, uint8_t *data, uint32_t len);
    void (*uart_dma_receive)(UART_DMA_STRUCT *huart_dma, uint32_t len);
    void (*uart_dma_tx_complete_callback)(UART_DMA_STRUCT *huart_dma);
    void (*uart_dma_rx_complete_callback)(UART_DMA_STRUCT *huart_dma);
    void (*uart_dma_error_callback)(UART_DMA_STRUCT *huart_dma);
    void (*DMA2_Stream5_IRQHandler)(void);
    void (*DMA2_Stream7_IRQHandler)(void);
} UART_DMA_STRUCT;

// 函数声明
void stm32_uart_dma_init(UART_DMA_STRUCT *huart_dma, UART_HandleTypeDef *huart);
void stm32_uart_dma_send(UART_DMA_STRUCT *huart_dma, uint8_t *data, uint32_t len);
void stm32_uart_dma_receive(UART_DMA_STRUCT *huart_dma, uint32_t len);
void stm32_uart_dma_tx_complete_callback(UART_DMA_STRUCT *huart_dma);
void stm32_uart_dma_rx_complete_callback(UART_DMA_STRUCT *huart_dma);
void stm32_uart_dma_error_callback(UART_DMA_STRUCT *huart_dma);
void stm32_DMA2_Stream5_IRQHandler(void);
void stm32_DMA2_Stream7_IRQHandler(void);
#endif /* _UART_DMA_H */