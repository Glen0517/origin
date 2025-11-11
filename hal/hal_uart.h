/*
 * UART硬件抽象层接口定义
 */

#ifndef HAL_UART_H
#define HAL_UART_H

#include "../include/types.h"

// UART通道定义
typedef enum {
    UART_1 = 0,
    UART_2,
    UART_3,
    UART_4,
    UART_5,
    UART_6,
    UART_7,
    UART_8,
    UART_MAX
} uart_channel_t;

// 数据位定义
typedef enum {
    UART_DATA_BITS_8 = 0,
    UART_DATA_BITS_9,
    UART_DATA_BITS_MAX
} uart_data_bits_t;

// 停止位定义
typedef enum {
    UART_STOP_BITS_1 = 0,
    UART_STOP_BITS_2,
    UART_STOP_BITS_MAX
} uart_stop_bits_t;

// 校验位定义
typedef enum {
    UART_PARITY_NONE = 0,
    UART_PARITY_EVEN,
    UART_PARITY_ODD,
    UART_PARITY_MAX
} uart_parity_t;

// 流控定义
typedef enum {
    UART_FLOW_CONTROL_NONE = 0,
    UART_FLOW_CONTROL_RTS,
    UART_FLOW_CONTROL_CTS,
    UART_FLOW_CONTROL_RTS_CTS,
    UART_FLOW_CONTROL_MAX
} uart_flow_control_t;

// UART配置结构体
typedef struct {
    uint32_t baudrate;              // 波特率
    uart_data_bits_t data_bits;     // 数据位
    uart_stop_bits_t stop_bits;     // 停止位
    uart_parity_t parity;           // 校验位
    uart_flow_control_t flow_control; // 流控
    bool dma_enable;                // DMA使能
    bool rx_interrupt_enable;       // 接收中断使能
    bool tx_interrupt_enable;       // 发送中断使能
} uart_config_t;

// 接收/发送回调函数类型
typedef void (*uart_rx_callback_t)(uint8_t data);
typedef void (*uart_tx_callback_t)(void);

// 函数声明

/**
 * @brief 初始化UART
 * @param channel UART通道
 * @param config UART配置结构体
 * @return 是否初始化成功
 */
bool hal_uart_init(uart_channel_t channel, uart_config_t *config);

/**
 * @brief 配置UART接收回调函数
 * @param channel UART通道
 * @param callback 接收回调函数
 */
void hal_uart_config_rx_callback(uart_channel_t channel, uart_rx_callback_t callback);

/**
 * @brief 配置UART发送完成回调函数
 * @param channel UART通道
 * @param callback 发送回调函数
 */
void hal_uart_config_tx_callback(uart_channel_t channel, uart_tx_callback_t callback);

/**
 * @brief 发送单个字符
 * @param channel UART通道
 * @param data 要发送的数据
 */
void hal_uart_send_byte(uart_channel_t channel, uint8_t data);

/**
 * @brief 发送数据
 * @param channel UART通道
 * @param data 要发送的数据缓冲区
 * @param length 数据长度
 * @return 实际发送的字节数
 */
uint16_t hal_uart_send_data(uart_channel_t channel, uint8_t *data, uint16_t length);

/**
 * @brief 接收数据
 * @param channel UART通道
 * @param data 接收数据缓冲区
 * @param length 最大接收长度
 * @return 实际接收的字节数
 */
uint16_t hal_uart_receive_data(uart_channel_t channel, uint8_t *data, uint16_t length);

/**
 * @brief 检查是否有数据可读
 * @param channel UART通道
 * @return 是否有数据可读
 */
bool hal_uart_is_data_available(uart_channel_t channel);

/**
 * @brief 清空接收缓冲区
 * @param channel UART通道
 */
void hal_uart_clear_rx_buffer(uart_channel_t channel);

/**
 * @brief 清空发送缓冲区
 * @param channel UART通道
 */
void hal_uart_clear_tx_buffer(uart_channel_t channel);

#endif /* HAL_UART_H */