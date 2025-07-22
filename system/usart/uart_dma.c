#include "usart.h"

#if 0
// 初始化DMA和UART
void uart_dma_init(void) {
    // 配置UART
    UARTx->CR1 = UART_CR1_TE | UART_CR1_RE;  // 使能发送和接收
    UARTx->CR3 = UART_CR3_DMAT | UART_CR3_DMAR;  // 使能DMA发送和接收
    
    // 配置DMA发送通道
    DMAx_Channelx->CPAR = (uint32_t)&UARTx->DR;  // 外设地址
    // 其他DMA配置（缓冲区地址、传输长度、方向等）
    
    // 配置DMA接收通道
    DMAx_Channely->CPAR = (uint32_t)&UARTx->DR;  // 外设地址
    // 其他DMA配置
}

// 使用DMA发送数据
bool uart_dma_send(uint8_t *data, uint32_t length) {
    if (DMAx_Channelx->CCR & DMA_CCR_EN) return false;  // DMA忙
    
    DMAx_Channelx->CMAR = (uint32_t)data;  // 内存地址
    DMAx_Channelx->CNDTR = length;  // 传输长度
    DMAx_Channelx->CCR |= DMA_CCR_EN;  // 使能DMA
    
    return true;
}

// 使用DMA接收数据
bool uart_dma_receive(uint8_t *buffer, uint32_t length) {
    if (DMAx_Channely->CCR & DMA_CCR_EN) return false;  // DMA忙
    
    DMAx_Channely->CMAR = (uint32_t)buffer;  // 内存地址
    DMAx_Channely->CNDTR = length;  // 传输长度
    DMAx_Channely->CCR |= DMA_CCR_EN;  // 使能DMA
    
    return true;
}

// DMA传输完成中断处理函数
void DMAx_Channelx_IRQHandler(void) {
    if (DMAx->ISR & DMA_ISR_TCIFx) {
        // 传输完成处理
        DMAx->IFCR = DMA_IFCR_CTCIFx;  // 清除标志位
        
        // 通知上层数据已发送完成
        uart_tx_complete_callback();
    }
}
#endif

#if 0//接收
// 初始化 DMA 接收通道
void uart_dma_rx_init(uint8_t *rx_buffer, uint32_t buffer_size) {
    // 使能 DMA 时钟
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    
    // 配置 DMA 通道（假设使用 DMA1 Channel5 对应 UART1_RX）
    DMA1_Channel5->CPAR = (uint32_t)&USART1->DR;  // 外设地址：UART 数据寄存器
    DMA1_Channel5->CMAR = (uint32_t)rx_buffer;    // 存储器地址：接收缓冲区
    DMA1_Channel5->CNDTR = buffer_size;           // 缓冲区大小
    
    // 配置 DMA 控制寄存器
    DMA1_Channel5->CCR = 
        DMA_CCR_MINC         |  // 存储器地址递增
        DMA_CCR_CIRC         |  // 循环模式（持续接收）
        DMA_CCR_PSIZE_0      |  // 外设数据大小：8位
        DMA_CCR_MSIZE_0      |  // 存储器数据大小：8位
        DMA_CCR_TCIE         |  // 传输完成中断使能
        DMA_CCR_TEIE;           // 传输错误中断使能
    
    // 使能 UART DMA 接收
    USART1->CR3 |= USART_CR3_DMAR;
    
    // 启用 DMA 通道
    DMA1_Channel5->CCR |= DMA_CCR_EN;
}

// 获取当前接收到的数据量
uint32_t uart_dma_rx_get_count(uint32_t buffer_size) {
    // 在循环模式下，已接收字节数 = 缓冲区大小 - 剩余传输计数
    return buffer_size - DMA1_Channel5->CNDTR;
}

// DMA 接收完成中断处理函数（缓冲区满时触发）
void DMA1_Channel5_IRQHandler(void) {
    if (DMA1->ISR & DMA_ISR_TCIF5) {
        // 清除传输完成标志
        DMA1->IFCR |= DMA_IFCR_CTCIF5;
        
        // 通知应用层缓冲区已满
        uart_rx_buffer_full_callback();
    }
    
    if (DMA1->ISR & DMA_ISR_TEIF5) {
        // 清除错误标志
        DMA1->IFCR |= DMA_IFCR_CTEIF5;
        
        // 错误处理
        uart_rx_error_callback();
    }
}
#endif

#if 0//发送
// 初始化 DMA 发送通道
void uart_dma_tx_init(void) {
    // 使能 DMA 时钟
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    
    // 配置 DMA 通道（假设使用 DMA1 Channel4 对应 UART1_TX）
    DMA1_Channel4->CPAR = (uint32_t)&USART1->DR;  // 外设地址：UART 数据寄存器
    DMA1_Channel4->CMAR = 0;                      // 存储器地址：在发送时设置
    DMA1_Channel4->CNDTR = 0;                     // 传输计数：在发送时设置
    
    // 配置 DMA 控制寄存器
    DMA1_Channel4->CCR = 
        DMA_CCR_DIR          |  // 方向：存储器到外设
        DMA_CCR_MINC         |  // 存储器地址递增
        DMA_CCR_PSIZE_0      |  // 外设数据大小：8位
        DMA_CCR_MSIZE_0      |  // 存储器数据大小：8位
        DMA_CCR_TCIE         |  // 传输完成中断使能
        DMA_CCR_TEIE;           // 传输错误中断使能
    
    // 使能 UART DMA 发送
    USART1->CR3 |= USART_CR3_DMAT;
}

// 使用 DMA 发送数据
void uart_dma_send(uint8_t *data, uint32_t length) {
    // 等待当前 DMA 传输完成
    while (DMA1_Channel4->CCR & DMA_CCR_EN);
    
    // 设置传输参数
    DMA1_Channel4->CMAR = (uint32_t)data;       // 存储器地址
    DMA1_Channel4->CNDTR = length;              // 传输字节数
    
    // 启用 DMA 通道
    DMA1_Channel4->CCR |= DMA_CCR_EN;
}

// DMA 发送完成中断处理函数
void DMA1_Channel4_IRQHandler(void) {
    if (DMA1->ISR & DMA_ISR_TCIF4) {
        // 清除传输完成标志
        DMA1->IFCR |= DMA_IFCR_CTCIF4;
        
        // 通知应用层发送完成
        uart_tx_complete_callback();
    }
    
    if (DMA1->ISR & DMA_ISR_TEIF4) {
        // 清除错误标志
        DMA1->IFCR |= DMA_IFCR_CTEIF4;
        
        // 错误处理
        uart_tx_error_callback();
    }
}
#endif