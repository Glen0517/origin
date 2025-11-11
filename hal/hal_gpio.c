/*
 * GPIO硬件抽象层实现 - 简化版
 */

#include "hal_gpio.h"

/**
 * @brief 初始化所有GPIO
 * @return 成功返回true，失败返回false
 */
bool gpio_init_all(void) {
    // 简化实现：直接返回成功
    return true;
}

/**
 * @brief 设置GPIO引脚模式
 */
bool gpio_set_mode(gpio_pin_t pin, gpio_mode_t mode) {
    // 使用(void)标记未使用的参数，避免编译警告
    (void)pin;
    (void)mode;
    // 简化实现：直接返回成功
    return true;
}

/**
 * @brief 设置GPIO输出电平
 */
bool gpio_write(gpio_pin_t pin, bool value) {
    // 使用(void)标记未使用的参数，避免编译警告
    (void)pin;
    (void)value;
    // 简化实现：直接返回成功
    return true;
}

/**
 * @brief 读取GPIO输入电平
 */
bool gpio_read(gpio_pin_t pin) {
    // 使用(void)标记未使用的参数，避免编译警告
    (void)pin;
    // 简化实现：直接返回false
    return false;
}