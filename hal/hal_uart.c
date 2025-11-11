/*
 * UART硬件抽象层实现 - 简化版
 */

#include "hal_uart.h"

/**
 * @brief 初始化所有UART
 * @return 成功返回true，失败返回false
 */
bool uart_init_all(void) {
    // 简化实现：直接返回成功
    return true;
}

/**
 * @brief 初始化UART
 */
bool uart_init(uart_config_t *config) {
    // 使用(void)标记未使用的参数，避免编译警告
    (void)config;
    // 简化实现：直接返回成功
    return true;
}

/**
 * @brief UART发送数据
 */
bool hal_uart_transmit(uart_channel_t channel, uint8_t *data, uint16_t length, uint32_t timeout) {
    // 使用(void)标记未使用的参数，避免编译警告
    (void)channel;
    (void)data;
    (void)length;
    (void)timeout;
    // 简化实现：直接返回成功
    return true;
}

/**
 * @brief UART接收数据
 */
bool hal_uart_receive(uart_channel_t channel, uint8_t *data, uint16_t length, uint32_t timeout) {
    // 使用(void)标记未使用的参数，避免编译警告
    (void)channel;
    (void)data;
    (void)length;
    (void)timeout;
    // 简化实现：直接返回成功
    return true;
}

/**
 * @brief 检查UART是否有数据
 */
bool uart_has_data(uart_channel_t channel) {
    // 使用(void)标记未使用的参数，避免编译警告
    (void)channel;
    // 简化实现：返回false
    return false;
}

/**
 * @brief 读取UART数据
 */
int32_t uart_read(uart_channel_t channel, uint8_t *buffer, uint32_t size) {
    // 使用(void)标记未使用的参数，避免编译警告
    (void)channel;
    (void)buffer;
    (void)size;
    // 简化实现：返回0（没有读取到数据）
    return 0;
}

/**
 * @brief 写入UART数据
 */
int32_t uart_write(uart_channel_t channel, uint8_t *buffer, uint32_t size) {
    // 使用(void)标记未使用的参数，避免编译警告
    (void)channel;
    (void)buffer;
    (void)size;
    // 简化实现：返回size（假设有数据写入）
    return size;
}