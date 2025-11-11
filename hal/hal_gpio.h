/*
 * GPIO硬件抽象层接口定义
 */

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include "../include/types.h"

// GPIO端口定义
typedef enum {
    GPIO_PORT_A = 0,
    GPIO_PORT_B,
    GPIO_PORT_C,
    GPIO_PORT_D,
    GPIO_PORT_E,
    GPIO_PORT_F,
    GPIO_PORT_G,
    GPIO_PORT_H,
    GPIO_PORT_I,
    GPIO_PORT_J,
    GPIO_PORT_K,
    GPIO_PORT_MAX
} gpio_port_t;

// GPIO引脚定义
typedef enum {
    GPIO_PIN_0 = 0,
    GPIO_PIN_1,
    GPIO_PIN_2,
    GPIO_PIN_3,
    GPIO_PIN_4,
    GPIO_PIN_5,
    GPIO_PIN_6,
    GPIO_PIN_7,
    GPIO_PIN_8,
    GPIO_PIN_9,
    GPIO_PIN_10,
    GPIO_PIN_11,
    GPIO_PIN_12,
    GPIO_PIN_13,
    GPIO_PIN_14,
    GPIO_PIN_15,
    GPIO_PIN_MAX
} gpio_pin_t;

// GPIO模式定义
typedef enum {
    GPIO_MODE_INPUT = 0,        // 输入模式
    GPIO_MODE_OUTPUT_PP,        // 推挽输出
    GPIO_MODE_OUTPUT_OD,        // 开漏输出
    GPIO_MODE_AF_PP,            // 复用推挽
    GPIO_MODE_AF_OD,            // 复用开漏
    GPIO_MODE_ANALOG,           // 模拟模式
    GPIO_MODE_MAX
} gpio_mode_t;

// GPIO输出速度定义
typedef enum {
    GPIO_SPEED_LOW = 0,         // 低速
    GPIO_SPEED_MEDIUM,          // 中速
    GPIO_SPEED_HIGH,            // 高速
    GPIO_SPEED_VERY_HIGH,       // 超高速
    GPIO_SPEED_MAX
} gpio_speed_t;

// GPIO上拉/下拉定义
typedef enum {
    GPIO_PUPD_NONE = 0,         // 无上拉/下拉
    GPIO_PUPD_PULLUP,           // 上拉
    GPIO_PUPD_PULLDOWN,         // 下拉
    GPIO_PUPD_MAX
} gpio_pupd_t;

// GPIO中断触发方式定义
typedef enum {
    GPIO_EXTI_TRIGGER_NONE = 0, // 无中断触发
    GPIO_EXTI_TRIGGER_RISING,   // 上升沿触发
    GPIO_EXTI_TRIGGER_FALLING,  // 下降沿触发
    GPIO_EXTI_TRIGGER_BOTH,     // 双边沿触发
    GPIO_EXTI_TRIGGER_MAX
} gpio_exti_trigger_t;

// GPIO配置结构体
typedef struct {
    gpio_mode_t mode;           // GPIO模式
    gpio_speed_t speed;         // 输出速度
    gpio_pupd_t pupd;           // 上拉/下拉配置
    gpio_exti_trigger_t trigger; // 外部中断触发方式
} gpio_config_t;

// GPIO初始化配置
typedef struct {
    gpio_port_t port;           // GPIO端口
    gpio_pin_t pin;             // GPIO引脚
    gpio_config_t config;       // 配置参数
} gpio_init_t;

// 函数声明

/**
 * @brief 初始化GPIO
 * @param init GPIO初始化配置结构体
 * @return 是否初始化成功
 */
bool hal_gpio_init(gpio_init_t *init);

/**
 * @brief 设置GPIO输出状态
 * @param port GPIO端口
 * @param pin GPIO引脚
 * @param state 输出状态 (true为高电平, false为低电平)
 */
void hal_gpio_set_output(gpio_port_t port, gpio_pin_t pin, bool state);

/**
 * @brief 读取GPIO输入状态
 * @param port GPIO端口
 * @param pin GPIO引脚
 * @return 输入状态 (true为高电平, false为低电平)
 */
bool hal_gpio_read_input(gpio_port_t port, gpio_pin_t pin);

/**
 * @brief 切换GPIO输出状态
 * @param port GPIO端口
 * @param pin GPIO引脚
 */
void hal_gpio_toggle(gpio_port_t port, gpio_pin_t pin);

/**
 * @brief 配置GPIO外部中断
 * @param port GPIO端口
 * @param pin GPIO引脚
 * @param trigger 触发方式
 * @param callback 中断回调函数
 * @return 是否配置成功
 */
bool hal_gpio_config_exti(gpio_port_t port, gpio_pin_t pin, 
                         gpio_exti_trigger_t trigger, 
                         void (*callback)(void));

/**
 * @brief 使能GPIO外部中断
 * @param port GPIO端口
 * @param pin GPIO引脚
 */
void hal_gpio_enable_exti(gpio_port_t port, gpio_pin_t pin);

/**
 * @brief 禁用GPIO外部中断
 * @param port GPIO端口
 * @param pin GPIO引脚
 */
void hal_gpio_disable_exti(gpio_port_t port, gpio_pin_t pin);

/**
 * @brief 清除GPIO外部中断标志
 * @param port GPIO端口
 * @param pin GPIO引脚
 */
void hal_gpio_clear_exti_flag(gpio_port_t port, gpio_pin_t pin);

#endif /* HAL_GPIO_H */