# UART+DMA功能使用说明

## 功能概述
本实现提供了基于STM32 HAL库的UART+DMA功能，支持非阻塞式的高速数据传输。通过DMA(直接内存访问)控制器，可以减轻CPU负担，实现更高效的数据收发。

## 文件结构
- `uart_dma.h`: 定义UART+DMA相关结构体和函数声明
- `uart_dma.c`: 实现UART+DMA初始化、发送、接收和中断处理
- `uart_dma_demo.h`: 声明UART+DMA演示函数
- `uart_dma_demo.c`: 实现UART+DMA演示功能
- `README_DMA.md`: UART+DMA功能使用说明

## 使用方法

### 1. 初始化UART+DMA
```c
#include "uart_dma.h"

// 定义UART DMA句柄
UART_DMA_HandleTypeDef huart_dma;

// 初始化UART DMA
void init_uart_dma(void)
{
    // 获取UART句柄（假设使用USART1）
    extern UART_HandleTypeDef huart1;

    // 初始化UART DMA
    if (uart_dma_init(&huart_dma, &huart1) != HAL_OK)
    {
        // 初始化失败处理
    }
}
```

### 2. 使用DMA发送数据
```c
uint8_t send_data[] = "Hello, UART+DMA!";

// 发送数据
if (uart_dma_send(&huart_dma, send_data, sizeof(send_data) - 1) == HAL_OK)
{
    // 等待发送完成
    while (!huart_dma.tx_complete)
    {
        // 可以添加超时处理
    }
    // 发送完成
}
```

### 3. 使用DMA接收数据
```c
uint32_t receive_len = 20;  // 要接收的数据长度

// 开始接收
if (uart_dma_receive(&huart_dma, receive_len) == HAL_OK)
{
    // 等待接收完成
    uint32_t timeout = 5000;  // 5秒超时
    uint32_t start_time = HAL_GetTick();

    while (!huart_dma.rx_complete)
    {
        if (HAL_GetTick() - start_time > timeout)
        {
            // 超时处理
            uart_dma_error_callback(&huart_dma);
            break;
        }
    }

    if (huart_dma.rx_complete)
    {
        // 处理接收到的数据
        // 数据存放在huart_dma.rx_buffer中，长度为huart_dma.rx_data_len
    }
}
```

### 4. 实现DMA中断处理
需要在对应的DMA中断处理函数中调用HAL库的中断处理函数，并在HAL回调中调用我们的自定义回调函数。

## 示例使用
1. 在FreeRTOS任务中调用`uart_dma_demo()`函数启动完整演示
2. 演示功能包括初始化、发送测试和接收测试
3. 确保正确连接UART设备以进行测试

## 注意事项
1. 确保正确配置DMA通道和流，根据实际硬件调整`uart_dma.c`中的DMA配置
2. 确保正确设置中断优先级
3. 在使用DMA接收时，注意设置合理的超时时间
4. 接收数据前确保有足够的缓冲区空间
5. 发送和接收完成后，检查状态标志以确保操作成功

## 版本历史
- V1.0: 初始实现UART+DMA功能