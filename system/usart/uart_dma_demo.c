/*
 * uart_dma_demo.c
 *
 *  Created on: 2023-xx-xx
 *      Author: xx
 */

#include "uart_dma_demo.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdio.h>

// UART句柄（假设使用USART1）
extern UART_HandleTypeDef huart1;

// 全局UART DMA句柄
UART_DMA_STRUCT huart_dma = {
    .huart = &huart1,
    .tx_buffer = {0},
    .rx_buffer = {0},
    .rx_data_len = 0,
    .tx_complete = true,
    .rx_complete = true
};

// 测试数据
uint8_t test_send_data[] = "Hello, UART+DMA! This is a test message.";
uint8_t test_receive_data[UART_DMA_RX_BUFFER_SIZE];

/**
 * @brief  UART+DMA初始化演示
 * @param  无
 * @retval 初始化结果
 */
bool uart_dma_init_demo(void)
{
    // 初始化UART（已在系统中完成）
    // stm32_uart_init(115200);

    // 获取UART句柄（假设已在uart.c中定义）
    extern UART_HandleTypeDef huart1;  // 假设使用USART1

    // 初始化UART DMA
    if (UART_DMA_INSTANCE.uart_dma_init(&huart_dma, &huart1) != HAL_OK)
    {
        printf("UART DMA initialization failed!\n");
        return false;
    }

    printf("UART DMA initialization successful!\n");
    return true;
}

/**
 * @brief  UART+DMA发送演示
 * @param  无
 * @retval 发送结果
 */
bool uart_dma_send_demo(void)
{
    if (stm32_uart_dma_send(&huart_dma, test_send_data, sizeof(test_send_data) - 1) != HAL_OK)
    {
        printf("UART DMA send failed!\n");
        return false;
    }

    // 等待发送完成
    uint32_t timeout = 5000;  // 5秒超时
    uint32_t start_time = HAL_GetTick();
    while (!huart_dma.tx_complete)
    {
        if (HAL_GetTick() - start_time > timeout)
        {
            printf("UART DMA send timeout!\n");
            stm32_uart_dma_error_callback(&huart_dma);
            return false;
        }
    }

    printf("UART DMA send completed! Data sent: %s\n", test_send_data);
    return true;
}

/**
 * @brief  UART+DMA接收演示
 * @param  无
 * @retval 接收结果
 */
bool uart_dma_receive_demo(void)
{
    uint32_t receive_len = 20;  // 接收20个字节

    if (stm32_uart_dma_receive(&huart_dma, receive_len) != HAL_OK)
    {
        printf("UART DMA receive failed!\n");
        return false;
    }

    // 等待接收完成
    uint32_t timeout = 5000;  // 5秒超时
    uint32_t start_time = HAL_GetTick();

    while (!huart_dma.rx_complete)
    {
        if (HAL_GetTick() - start_time > timeout)
        {
            printf("UART DMA receive timeout!\n");
            stm32_uart_dma_error_callback(&huart_dma);
            return false;
        }
    }

    // 打印接收到的数据
    printf("UART DMA receive completed! Received data: ");
    for (uint32_t i = 0; i < huart_dma.rx_data_len; i++)
    {
        printf("%c", huart_dma.rx_buffer[i]);
    }
    printf("\n");

    return true;
}

/**
 * @brief  UART+DMA完整演示
 * @param  无
 * @retval 无
 */
void uart_dma_demo(void)
{
    printf("Starting UART+DMA demo...\n");

    // 初始化
    if (!uart_dma_init_demo())
    {
        return;
    }

    // 发送演示
    uart_dma_send_demo();

    // 接收演示
    printf("Waiting for data to receive...\n");
    uart_dma_receive_demo();

    printf("UART+DMA demo completed!\n");
}