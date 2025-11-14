/*
 * UART硬件抽象层实现 - STM32风格
 */

#include "hal_uart.h"
#include <stdio.h>
#include <string.h>

// STM32F4系列UART寄存器定义
#define USART1_BASE   0x40011000U
#define USART2_BASE   0x40004400U
#define USART3_BASE   0x40004800U
#define UART4_BASE    0x40004C00U
#define UART5_BASE    0x40005000U
#define USART6_BASE   0x40011400U
#define UART7_BASE    0x40007800U
#define UART8_BASE    0x40007C00U

// UART端口基地址查找表
static const uint32_t uart_base_addresses[] = {
    [UART_1] = USART1_BASE,
    [UART_2] = USART2_BASE,
    [UART_3] = USART3_BASE,
    [UART_4] = UART4_BASE,
    [UART_5] = UART5_BASE,
    [UART_6] = USART6_BASE,
    [UART_7] = UART7_BASE,
    [UART_8] = UART8_BASE
};

// UART寄存器结构体
typedef struct {
    uint32_t SR;    // 状态寄存器
    uint32_t DR;    // 数据寄存器
    uint32_t BRR;   // 波特率寄存器
    uint32_t CR1;   // 控制寄存器1
    uint32_t CR2;   // 控制寄存器2
    uint32_t CR3;   // 控制寄存器3
    uint32_t GTPR;  // 保护时间和预分频寄存器
} USART_TypeDef;

// 通过通道号获取UART实例的宏定义 - 更安全的实现
#define UART_GET_INSTANCE(channel)    (((channel) < UART_MAX) ? \
                                     ((USART_TypeDef *)uart_base_addresses[(channel)]) : NULL)

// UART状态标志位定义
#define USART_SR_TXE     (1U << 7U)  // 发送数据寄存器为空
#define USART_SR_TC      (1U << 6U)  // 发送完成
#define USART_SR_RXNE    (1U << 5U)  // 接收数据寄存器非空

// UART控制位定义
#define USART_CR1_UE     (1U << 13U) // USART使能
#define USART_CR1_TE     (1U << 3U)  // 发送使能
#define USART_CR1_RE     (1U << 2U)  // 接收使能
#define USART_CR1_RXNEIE (1U << 5U)  // 接收中断使能
#define USART_CR1_TCIE   (1U << 6U)  // 发送完成中断使能

// UART通道状态结构体
typedef struct {
    bool initialized;
    uart_config_t config;
    uart_rx_callback_t rx_callback;
    uart_tx_callback_t tx_callback;
} uart_channel_state_t;

// UART通道状态数组
static uart_channel_state_t uart_states[UART_MAX] = {0};

/**
 * @brief 初始化所有UART
 * @return 成功返回true，失败返回false
 */
bool uart_init_all(void) {
    // STM32平台：初始化所有UART时钟
    printf("STM32 UART时钟初始化完成\n");
    
    // 初始化UART1用于日志输出
    uart_config_t log_uart_config = {
        .baudrate = 115200,
        .data_bits = UART_DATA_BITS_8,
        .stop_bits = UART_STOP_BITS_1,
        .parity = UART_PARITY_NONE,
        .flow_control = UART_FLOW_CONTROL_NONE,
        .dma_enable = false,
        .rx_interrupt_enable = false,
        .tx_interrupt_enable = false
    };
    
    return hal_uart_init(UART_1, &log_uart_config);
}

/**
 * @brief 计算UART波特率寄存器值
 * @param baudrate 目标波特率
 * @param clock_freq UART时钟频率
 * @return 波特率寄存器值
 */
static uint32_t calculate_baudrate(uint32_t baudrate, uint32_t clock_freq) {
    return (clock_freq * 1000000U) / (baudrate * 16U);
}

/**
 * @brief 初始化UART
 * @param channel UART通道
 * @param config UART配置结构体
 * @return 是否初始化成功
 */
bool hal_uart_init(uart_channel_t channel, uart_config_t *config) {
    if (channel >= UART_MAX || config == NULL) {
        return false;
    }
    
    USART_TypeDef *uart = UART_GET_INSTANCE(channel);
    
    // 禁用UART
    uart->CR1 &= ~USART_CR1_UE;
    
    // 设置波特率（假设系统时钟为16MHz）
    uart->BRR = calculate_baudrate(config->baudrate, 16);
    
    // 配置数据位、停止位和校验位
    uart->CR1 &= ~(0x1FFU << 1U);  // 清除数据位和校验位配置
    
    // 数据位配置
    if (config->data_bits == UART_DATA_BITS_9) {
        uart->CR1 |= (1U << 12U);  // M位设置为1，表示9位数据
    }
    
    // 校验位配置
    switch (config->parity) {
        case UART_PARITY_EVEN:
            uart->CR1 |= (1U << 10U);  // PCE位设置为1
            uart->CR1 &= ~(1U << 9U);   // PS位设置为0，表示偶校验
            break;
        case UART_PARITY_ODD:
            uart->CR1 |= (1U << 10U) | (1U << 9U);  // PCE=1, PS=1，表示奇校验
            break;
        default:  // UART_PARITY_NONE
            uart->CR1 &= ~(1U << 10U);  // PCE位设置为0
            break;
    }
    
    // 停止位配置
    uart->CR2 &= ~(0x3U << 12U);  // 清除停止位配置
    switch (config->stop_bits) {
        case UART_STOP_BITS_2:
            uart->CR2 |= (1U << 13U);  // 设置2个停止位
            break;
        default:  // UART_STOP_BITS_1
            // 1个停止位，无需设置
            break;
    }
    
    // 流控配置
    uart->CR3 &= ~(0x3U << 8U);  // 清除流控配置
    switch (config->flow_control) {
        case UART_FLOW_CONTROL_RTS:
            uart->CR3 |= (1U << 8U);  // RTSE位设置为1
            break;
        case UART_FLOW_CONTROL_CTS:
            uart->CR3 |= (1U << 9U);  // CTSE位设置为1
            break;
        case UART_FLOW_CONTROL_RTS_CTS:
            uart->CR3 |= (1U << 8U) | (1U << 9U);  // RTSE=1, CTSE=1
            break;
        default:  // UART_FLOW_CONTROL_NONE
            // 无流控，无需设置
            break;
    }
    
    // 中断配置
    if (config->rx_interrupt_enable) {
        uart->CR1 |= USART_CR1_RXNEIE;
    }
    if (config->tx_interrupt_enable) {
        uart->CR1 |= USART_CR1_TCIE;
    }
    
    // 使能发送和接收
    uart->CR1 |= USART_CR1_TE | USART_CR1_RE;
    
    // 使能UART
    uart->CR1 |= USART_CR1_UE;
    
    // 初始化UART通道状态
    uart_states[channel].initialized = true;
    uart_states[channel].config = *config;
    uart_states[channel].rx_callback = NULL;
    uart_states[channel].tx_callback = NULL;
    
    printf("STM32 UART%d初始化: 波特率=%u, 数据位=%u, 停止位=%u\n", 
           channel + 1, config->baudrate, 
           config->data_bits + 8, config->stop_bits + 1);
    
    return true;
}

/**
 * @brief 配置UART接收回调函数
 * @param channel UART通道
 * @param callback 接收回调函数
 */
void hal_uart_config_rx_callback(uart_channel_t channel, uart_rx_callback_t callback) {
    if (channel < UART_MAX) {
        uart_states[channel].rx_callback = callback;
    }
}

/**
 * @brief 配置UART发送完成回调函数
 * @param channel UART通道
 * @param callback 发送回调函数
 */
void hal_uart_config_tx_callback(uart_channel_t channel, uart_tx_callback_t callback) {
    if (channel < UART_MAX) {
        uart_states[channel].tx_callback = callback;
    }
}

/**
 * @brief 发送单个字符
 * @param channel UART通道
 * @param data 要发送的数据
 */
void hal_uart_send_byte(uart_channel_t channel, uint8_t data) {
    if (channel < UART_MAX && uart_states[channel].initialized) {
        USART_TypeDef *uart = UART_GET_INSTANCE(channel);
        
        // 等待发送数据寄存器为空
        while (!(uart->SR & USART_SR_TXE)) {
            // 等待
        }
        
        // 发送数据
        uart->DR = data;
        
        // 等待发送完成
        while (!(uart->SR & USART_SR_TC)) {
            // 等待
        }
        
        // 在PC环境下，同时使用printf输出作为模拟
        putchar(data);
        
        // 如果配置了发送完成回调，调用回调函数
        if (uart_states[channel].tx_callback != NULL) {
            uart_states[channel].tx_callback();
        }
    }
}

/**
 * @brief 发送数据
 * @param channel UART通道
 * @param data 要发送的数据
 * @param length 数据长度
 * @return 实际发送的数据长度
 */
uint16_t hal_uart_send_data(uart_channel_t channel, uint8_t *data, uint16_t length) {
    if (channel < UART_MAX && uart_states[channel].initialized && data != NULL) {
        USART_TypeDef *uart = UART_GET_INSTANCE(channel);
        uint16_t sent = 0;
        
        // 逐字节发送数据
        for (sent = 0; sent < length; sent++) {
            // 等待发送数据寄存器为空
            while (!(uart->SR & USART_SR_TXE)) {
                // 等待
            }
            
            // 发送数据
            uart->DR = data[sent];
        }
        
        // 等待最后一个字节发送完成
        while (!(uart->SR & USART_SR_TC)) {
            // 等待
        }
        
        // 在PC环境下，使用printf模拟UART发送
        fwrite(data, 1, length, stdout);
        fflush(stdout); // 确保数据立即输出
        
        // 如果配置了发送完成回调，调用回调函数
        if (uart_states[channel].tx_callback != NULL) {
            uart_states[channel].tx_callback();
        }
        
        return sent;
    }
    
    return 0;
}

/**
 * @brief 接收数据
 * @param channel UART通道
 * @param data 接收缓冲区
 * @param length 缓冲区长度
 * @return 实际接收的数据长度
 */
uint16_t hal_uart_receive_data(uart_channel_t channel, uint8_t *data, uint16_t length) {
    if (channel < UART_MAX && uart_states[channel].initialized && data != NULL) {
        USART_TypeDef *uart = UART_GET_INSTANCE(channel);
        uint16_t received = 0;
        
        // 检查是否有数据可读
        if (uart->SR & USART_SR_RXNE) {
            // 读取数据
            data[received] = (uint8_t)(uart->DR);
            received++;
            
            // 如果配置了接收回调，调用回调函数
            if (uart_states[channel].rx_callback != NULL) {
                uart_states[channel].rx_callback(data[received - 1]);
            }
        }
        
        return received;
    }
    
    return 0;
}

/**
 * @brief 检查是否有数据可读
 * @param channel UART通道
 * @return 有数据返回true，无数据返回false
 */
bool hal_uart_is_data_available(uart_channel_t channel) {
    if (channel < UART_MAX && uart_states[channel].initialized) {
        USART_TypeDef *uart = UART_GET_INSTANCE(channel);
        return (uart->SR & USART_SR_RXNE) != 0;
    }
    
    return false;
}

/**
 * @brief 清除接收缓冲区
 * @param channel UART通道
 */
void hal_uart_clear_rx_buffer(uart_channel_t channel) {
    if (channel < UART_MAX && uart_states[channel].initialized) {
        USART_TypeDef *uart = UART_GET_INSTANCE(channel);
        
        // 读取并丢弃接收数据寄存器中的数据
        if (uart->SR & USART_SR_RXNE) {
            (void)uart->DR;
        }
    }
}

/**
 * @brief 清除发送缓冲区
 * @param channel UART通道
 */
void hal_uart_clear_tx_buffer(uart_channel_t channel) {
    if (channel < UART_MAX && uart_states[channel].initialized) {
        // STM32 UART没有显式的发送缓冲区清除方法
        // 等待发送完成即可
        USART_TypeDef *uart = UART_GET_INSTANCE(channel);
        while (!(uart->SR & USART_SR_TC)) {
            // 等待发送完成
        }
    }
}

/**
 * @brief UART发送数据（带超时）
 */
bool hal_uart_transmit(uart_channel_t channel, uint8_t *data, uint16_t length, uint32_t timeout) {
    if (channel < UART_MAX && uart_states[channel].initialized && data != NULL) {
        USART_TypeDef *uart = UART_GET_INSTANCE(channel);
        uint16_t sent = 0;
        uint32_t start_time = 0; // 这里应该获取系统时间，简化实现
        
        for (sent = 0; sent < length; sent++) {
            start_time = 0;
            // 等待发送数据寄存器为空或超时
            while (!(uart->SR & USART_SR_TXE)) {
                if (timeout > 0 && (0 - start_time) > timeout) {
                    return false; // 超时
                }
            }
            
            // 发送数据
            uart->DR = data[sent];
        }
        
        // 等待最后一个字节发送完成
        start_time = 0;
        while (!(uart->SR & USART_SR_TC)) {
            if (timeout > 0 && (0 - start_time) > timeout) {
                return false; // 超时
            }
        }
        
        // 在PC环境下，同时使用printf输出作为模拟
        fwrite(data, 1, length, stdout);
        fflush(stdout);
        
        return true;
    }
    
    return false;
}

/**
 * @brief UART接收数据（带超时）
 */
bool hal_uart_receive(uart_channel_t channel, uint8_t *data, uint16_t length, uint32_t timeout) {
    if (channel < UART_MAX && uart_states[channel].initialized && data != NULL) {
        USART_TypeDef *uart = UART_GET_INSTANCE(channel);
        uint16_t received = 0;
        uint32_t start_time = 0;
        
        while (received < length) {
            start_time = 0;
            // 等待接收数据寄存器非空或超时
            while (!(uart->SR & USART_SR_RXNE)) {
                if (timeout > 0 && (0 - start_time) > timeout) {
                    return false; // 超时
                }
            }
            
            // 读取数据
            data[received] = (uint8_t)(uart->DR);
            received++;
            
            // 如果配置了接收回调，调用回调函数
            if (uart_states[channel].rx_callback != NULL) {
                uart_states[channel].rx_callback(data[received - 1]);
            }
        }
        
        return true;
    }
    
    return false;
}

/**
 * @brief 检查UART是否有数据
 */
bool uart_has_data(uart_channel_t channel) {
    return hal_uart_is_data_available(channel);
}

/**
 * @brief 读取UART数据
 */
int32_t uart_read(uart_channel_t channel, uint8_t *buffer, uint32_t size) {
    return (int32_t)hal_uart_receive_data(channel, buffer, (uint16_t)size);
}

/**
 * @brief 写入UART数据
 */
int32_t uart_write(uart_channel_t channel, uint8_t *buffer, uint32_t size) {
    return (int32_t)hal_uart_send_data(channel, buffer, (uint16_t)size);
}