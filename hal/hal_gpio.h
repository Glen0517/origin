/**
 * @file hal_gpio.h
 * @brief GPIO硬件抽象层接口定义
 * @details 该模块提供了通用的GPIO操作接口，屏蔽了不同硬件平台之间的差异，
 *          使上层应用可以以统一的方式操作GPIO引脚。
 */
#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include "../include/types.h"

/**
 * @brief GPIO端口枚举
 * @details 定义了系统支持的GPIO端口，不同芯片平台可能支持的端口数量不同
 */
typedef enum {
    GPIO_PORT_A = 0,    // 端口A
    GPIO_PORT_B,        // 端口B
    GPIO_PORT_C,        // 端口C
    GPIO_PORT_D,        // 端口D
    GPIO_PORT_E,        // 端口E
    GPIO_PORT_F,        // 端口F
    GPIO_PORT_G,        // 端口G
    GPIO_PORT_H,        // 端口H
    GPIO_PORT_I,        // 端口I
    GPIO_PORT_J,        // 端口J
    GPIO_PORT_K,        // 端口K
    GPIO_PORT_MAX       // 端口数量最大值，用于边界检查
} gpio_port_t;

/**
 * @brief GPIO引脚定义
 * @details 定义了每个GPIO端口支持的引脚编号
 */
typedef enum {
    GPIO_PIN_0 = 0,     // 引脚0
    GPIO_PIN_1,         // 引脚1
    GPIO_PIN_2,         // 引脚2
    GPIO_PIN_3,         // 引脚3
    GPIO_PIN_4,         // 引脚4
    GPIO_PIN_5,         // 引脚5
    GPIO_PIN_6,         // 引脚6
    GPIO_PIN_7,         // 引脚7
    GPIO_PIN_8,         // 引脚8
    GPIO_PIN_9,         // 引脚9
    GPIO_PIN_10,        // 引脚10
    GPIO_PIN_11,        // 引脚11
    GPIO_PIN_12,        // 引脚12
    GPIO_PIN_13,        // 引脚13
    GPIO_PIN_14,        // 引脚14
    GPIO_PIN_15,        // 引脚15
    GPIO_PIN_MAX        // 引脚数量最大值，用于边界检查
} gpio_pin_t;

/**
 * @brief GPIO模式定义
 * @details 定义了GPIO的各种工作模式
 */
typedef enum {
    GPIO_MODE_INPUT = 0,        // 输入模式
    GPIO_MODE_OUTPUT_PP,        // 推挽输出
    GPIO_MODE_OUTPUT_OD,        // 开漏输出
    GPIO_MODE_AF_PP,            // 复用推挽
    GPIO_MODE_AF_OD,            // 复用开漏
    GPIO_MODE_ANALOG,           // 模拟模式
    GPIO_MODE_MAX               // 模式数量最大值，用于边界检查
} gpio_mode_t;

/**
 * @brief GPIO输出速度定义
 * @details 定义了GPIO的输出驱动能力/速度等级
 */
typedef enum {
    GPIO_SPEED_LOW = 0,         // 低速
    GPIO_SPEED_MEDIUM,          // 中速
    GPIO_SPEED_HIGH,            // 高速
    GPIO_SPEED_VERY_HIGH,       // 超高速
    GPIO_SPEED_MAX              // 速度等级最大值，用于边界检查
} gpio_speed_t;

/**
 * @brief GPIO上拉/下拉定义
 * @details 定义了GPIO的上拉下拉配置选项
 */
typedef enum {
    GPIO_PUPD_NONE = 0,         // 无上拉/下拉
    GPIO_PUPD_PULLUP,           // 上拉
    GPIO_PUPD_PULLDOWN,         // 下拉
    GPIO_PUPD_MAX               // 上拉下拉配置最大值，用于边界检查
} gpio_pupd_t;

/**
 * @brief GPIO中断触发方式定义
 * @details 定义了GPIO外部中断的触发条件
 */
typedef enum {
    GPIO_EXTI_TRIGGER_NONE = 0, // 无中断触发
    GPIO_EXTI_TRIGGER_RISING,   // 上升沿触发
    GPIO_EXTI_TRIGGER_FALLING,  // 下降沿触发
    GPIO_EXTI_TRIGGER_BOTH,     // 双边沿触发
    GPIO_EXTI_TRIGGER_MAX       // 触发方式最大值，用于边界检查
} gpio_exti_trigger_t;

/**
 * @brief GPIO配置结构体
 * @details 包含GPIO的基本配置参数
 */
typedef struct {
    gpio_mode_t mode;           // GPIO模式
    gpio_speed_t speed;         // 输出速度
    gpio_pupd_t pupd;           // 上拉/下拉配置
    gpio_exti_trigger_t trigger; // 外部中断触发方式
} gpio_config_t;

/**
 * @brief GPIO初始化配置结构体
 * @details 包含GPIO初始化所需的全部配置信息
 */
typedef struct {
    gpio_port_t port;           // GPIO端口
    gpio_pin_t pin;             // GPIO引脚
    gpio_config_t config;       // 配置参数
} gpio_init_t;

// 函数声明

/**
 * @brief 初始化GPIO
 * @param init GPIO初始化配置结构体指针
 * @return 是否初始化成功
 * @note init参数不能为空，否则返回false
 */
bool hal_gpio_init(gpio_init_t *init);

/**
 * @brief 设置GPIO输出状态
 * @param port GPIO端口
 * @param pin GPIO引脚
 * @param state 输出状态 (true为高电平, false为低电平)
 * @note 仅当GPIO配置为输出模式时有效
 */
void hal_gpio_set_output(gpio_port_t port, gpio_pin_t pin, bool state);

/**
 * @brief 读取GPIO输入状态
 * @param port GPIO端口
 * @param pin GPIO引脚
 * @return 输入状态 (true为高电平, false为低电平)
 * @note 通常在GPIO配置为输入模式时使用
 */
bool hal_gpio_read_input(gpio_port_t port, gpio_pin_t pin);

/**
 * @brief 切换GPIO输出状态
 * @param port GPIO端口
 * @param pin GPIO引脚
 * @note 仅当GPIO配置为输出模式时有效
 */
void hal_gpio_toggle(gpio_port_t port, gpio_pin_t pin);

/**
 * @brief 配置GPIO外部中断
 * @param port GPIO端口
 * @param pin GPIO引脚
 * @param trigger 触发方式
 * @param callback 中断回调函数
 * @return 是否配置成功
 * @note 配置中断后，需要调用hal_gpio_enable_exti使能中断
 */
bool hal_gpio_config_exti(gpio_port_t port, gpio_pin_t pin, 
                         gpio_exti_trigger_t trigger, 
                         void (*callback)(void));

/**
 * @brief 使能GPIO外部中断
 * @param port GPIO端口
 * @param pin GPIO引脚
 * @note 中断配置完成后，必须调用此函数使能中断
 */
void hal_gpio_enable_exti(gpio_port_t port, gpio_pin_t pin);

/**
 * @brief 禁用GPIO外部中断
 * @param port GPIO端口
 * @param pin GPIO引脚
 * @note 可以临时禁用中断而无需重新配置
 */
void hal_gpio_disable_exti(gpio_port_t port, gpio_pin_t pin);

/**
 * @brief 清除GPIO外部中断标志
 * @param port GPIO端口
 * @param pin GPIO引脚
 * @note 中断处理完成后必须调用此函数清除中断标志，否则会导致重复进入中断
 */
void hal_gpio_clear_exti_flag(gpio_port_t port, gpio_pin_t pin);

#endif /* HAL_GPIO_H */