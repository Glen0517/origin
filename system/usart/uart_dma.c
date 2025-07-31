/*
 * uart_dma.c
 *
 *  Created on: 2023-xx-xx
 *      Author: xx
 */
#include "uart_dma.h"
//#include "stm32f4xx_hal.h"

/**
 * @brief  初始化UART DMA
 * @param  huart_dma: UART DMA句柄
 * @param  huart: UART句柄
 * @retval HAL状态
 */
void stm32_uart_dma_init(UART_DMA_STRUCT *huart_dma, UART_HandleTypeDef *huart)
{
    // 初始化句柄
    huart_dma->huart = huart;
    // 将DMA句柄关联到UART句柄的私有数据字段
    huart->pvData = (void *)huart_dma;
    huart_dma->tx_complete = true;
    huart_dma->rx_complete = true;
    huart_dma->rx_data_len = 0;

    // 配置DMA发送
    __HAL_RCC_DMA2_CLK_ENABLE();  // 使能DMA时钟，根据实际使用的DMA通道调整

    // 配置TX DMA
    huart_dma->hdma_tx.Instance = DMA2_Stream7;  // 根据实际使用的DMA流和通道调整
    huart_dma->hdma_tx.Init.Channel = DMA_CHANNEL_4;  // 根据实际使用的通道调整
    huart_dma->hdma_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    huart_dma->hdma_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    huart_dma->hdma_tx.Init.MemInc = DMA_MINC_ENABLE;
    huart_dma->hdma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    huart_dma->hdma_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    huart_dma->hdma_tx.Init.Mode = DMA_NORMAL;
    huart_dma->hdma_tx.Init.Priority = DMA_PRIORITY_MEDIUM;
    huart_dma->hdma_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    huart_dma->hdma_tx.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    huart_dma->hdma_tx.Init.MemBurst = DMA_MBURST_SINGLE;
    huart_dma->hdma_tx.Init.PeriphBurst = DMA_PBURST_SINGLE;

    // 关联DMA和UART
    __HAL_LINKDMA(huart, hdmatx, huart_dma->hdma_tx);

    // 配置RX DMA
    huart_dma->hdma_rx.Instance = DMA2_Stream5;  // 根据实际使用的DMA流和通道调整
    huart_dma->hdma_rx.Init.Channel = DMA_CHANNEL_4;  // 根据实际使用的通道调整
    huart_dma->hdma_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    huart_dma->hdma_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    huart_dma->hdma_rx.Init.MemInc = DMA_MINC_ENABLE;
    huart_dma->hdma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    huart_dma->hdma_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    huart_dma->hdma_rx.Init.Mode = DMA_NORMAL;
    huart_dma->hdma_rx.Init.Priority = DMA_PRIORITY_MEDIUM;
    huart_dma->hdma_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    huart_dma->hdma_rx.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    huart_dma->hdma_rx.Init.MemBurst = DMA_MBURST_SINGLE;
    huart_dma->hdma_rx.Init.PeriphBurst = DMA_PBURST_SINGLE;

    if (HAL_DMA_Init(&huart_dma->hdma_rx) != HAL_OK)
    {
  //      return HAL_ERROR;
    }

    // 关联DMA和UART
    __HAL_LINKDMA(huart, hdmarx, huart_dma->hdma_rx);

    // 配置DMA中断
    HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 5, 0);  // TX DMA中断优先级
    HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);

    HAL_NVIC_SetPriority(DMA2_Stream5_IRQn, 5, 1);  // RX DMA中断优先级
    HAL_NVIC_EnableIRQ(DMA2_Stream5_IRQn);

    // 注册UART回调函数
    HAL_UART_RegisterCallback(huart, HAL_UART_TX_COMPLETE_CB_ID, stm32_uart_dma_tx_complete);
    HAL_UART_RegisterCallback(huart, HAL_UART_RX_COMPLETE_CB_ID, stm32_uart_dma_rx_complete);
    HAL_UART_RegisterCallback(huart, HAL_UART_ERROR_CB_ID, stm32_uart_dma_error);

    //return HAL_OK;
}

/**
 * @brief  通过DMA发送数据
 * @param  huart_dma: UART DMA句柄
 * @param  data: 要发送的数据
 * @param  len: 数据长度
 * @retval HAL状态
 */
HAL_StatusTypeDef stm32_uart_dma_send(UART_DMA_STRUCT *huart_dma, uint8_t *data, uint32_t len)
{
    if (huart_dma == NULL || data == NULL || len == 0)
    {
        return HAL_ERROR;
    }

    // 检查发送是否完成
    if (!huart_dma->tx_complete)
    {
        return HAL_BUSY;
    }

    // 检查数据长度是否超过缓冲区大小
    if (len > UART_DMA_TX_BUFFER_SIZE)
    {
        return HAL_ERROR;
    }

    // 复制数据到发送缓冲区
    memcpy(huart_dma->tx_buffer, data, len);

    // 重置发送完成标志
    huart_dma->tx_complete = false;

    // 启动DMA发送
    if (HAL_UART_Transmit_DMA(huart_dma->huart, huart_dma->tx_buffer, len) != HAL_OK)
    {
        huart_dma->tx_complete = true;
        return HAL_ERROR;
    }

    return HAL_OK;
}

/**
 * @brief  通过DMA接收数据
 * @param  huart_dma: UART DMA句柄
 * @param  len: 要接收的数据长度
 * @retval HAL状态
 */
HAL_StatusTypeDef stm32_uart_dma_receive(UART_DMA_STRUCT *huart_dma, uint32_t len)
{
    if (huart_dma == NULL || len == 0)
    {
        return HAL_ERROR;
    }

    // 检查接收是否完成
    if (!huart_dma->rx_complete)
    {
        return HAL_BUSY;
    }

    // 检查数据长度是否超过缓冲区大小
    if (len > UART_DMA_RX_BUFFER_SIZE)
    {
        return HAL_ERROR;
    }

    // 重置接收完成标志
    huart_dma->rx_complete = false;
    huart_dma->rx_data_len = len;

    // 启动DMA接收
    if (HAL_UART_Receive_DMA(huart_dma->huart, huart_dma->rx_buffer, len) != HAL_OK)
    {
        huart_dma->rx_complete = true;
        huart_dma->rx_data_len = 0;
        return HAL_ERROR;
    }

    return HAL_OK;
}

/**
 * @brief  UART DMA发送完成回调
 * @param  huart_dma: UART DMA句柄
 * @retval 无
 */
void stm32_uart_dma_tx_complete_callback(UART_DMA_STRUCT *huart_dma)
{
    if (huart_dma != NULL)
    {
        huart_dma->tx_complete = true;
        // 可以在这里添加用户定义的操作
    }
}

/**
 * @brief  UART DMA接收完成回调
 * @param  huart_dma: UART DMA句柄
 * @retval 无
 */
void stm32_uart_dma_rx_complete_callback(UART_DMA_STRUCT *huart_dma)
{
    if (huart_dma != NULL)
    {
        huart_dma->rx_complete = true;
        // 可以在这里添加用户定义的操作，如数据处理
    }
}

/**
 * @brief  UART DMA错误回调
 * @param  huart_dma: UART DMA句柄
 * @retval 无
 */
void stm32_uart_dma_error_callback(UART_DMA_STRUCT *huart_dma)
{
    if (huart_dma != NULL)
    {
        // 重置标志
        huart_dma->tx_complete = true;
        huart_dma->rx_complete = true;
        huart_dma->rx_data_len = 0;

        // 停止DMA传输
        HAL_UART_Abort(huart_dma->huart);

        // 可以在这里添加错误处理代码
    }
}

// UART TX DMA完成回调函数
void stm32_uart_dma_tx_complete(UART_HandleTypeDef *huart)
{
    // 获取关联的DMA句柄
    UART_DMA_STRUCT *huart_dma = (UART_DMA_STRUCT *)huart->pvData;
    if (huart_dma != NULL)
    {
        stm32_uart_dma_tx_complete_callback(huart_dma);
    }
}

// UART RX DMA完成回调函数
void stm32_uart_dma_rx_complete(UART_HandleTypeDef *huart)
{
    // 获取关联的DMA句柄
    UART_DMA_STRUCT *huart_dma = (UART_DMA_STRUCT *)huart->pvData;
    if (huart_dma != NULL)
    {
        stm32_uart_dma_rx_complete_callback(huart_dma);
    }
}

// UART DMA错误回调函数
void stm32_uart_dma_error(UART_HandleTypeDef *huart)
{
    // 获取关联的DMA句柄
    UART_DMA_STRUCT *huart_dma = (UART_DMA_STRUCT *)huart->pvData;
    if (huart_dma != NULL)
    {
        stm32_uart_dma_error_callback(huart_dma);
    }
}

// DMA中断处理函数
void stm32_DMA2_Stream7_IRQHandler(void)
{
    // 这里需要根据实际的UART DMA句柄调用HAL_DMA_IRQHandler
    // 注意：在实际应用中，需要获取当前使用的UART DMA句柄
    // 示例中假设使用全局变量huart_dma
    // HAL_DMA_IRQHandler(&huart_dma.hdma_tx);
}

void stm32_DMA2_Stream5_IRQHandler(void)
{
    // 这里需要根据实际的UART DMA句柄调用HAL_DMA_IRQHandler
    // 注意：在实际应用中，需要获取当前使用的UART DMA句柄
    // 示例中假设使用全局变量huart_dma
    // HAL_DMA_IRQHandler(&huart_dma.hdma_rx);
}