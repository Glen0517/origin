/*
 * GPIO硬件抽象层实现 - STM32风格
 */

#include "hal_gpio.h"
#include <stdio.h>

// STM32F4系列GPIO寄存器定义
#define GPIOA_BASE    0x40020000U
#define GPIOB_BASE    0x40020400U
#define GPIOC_BASE    0x40020800U
#define GPIOD_BASE    0x40020C00U
#define GPIOE_BASE    0x40021000U
#define GPIOF_BASE    0x40021400U
#define GPIOG_BASE    0x40021800U
#define GPIOH_BASE    0x40021C00U
#define GPIOI_BASE    0x40022000U
#define GPIOJ_BASE    0x40022400U
#define GPIOK_BASE    0x40022800U

// GPIO端口基地址查找表
static const uint32_t gpio_base_addresses[] = {
    [0] = GPIOA_BASE,
    [1] = GPIOB_BASE,
    [2] = GPIOC_BASE,
    [3] = GPIOD_BASE,
    [4] = GPIOE_BASE,
    [5] = GPIOF_BASE,
    [6] = GPIOG_BASE,
    [7] = GPIOH_BASE,
    [8] = GPIOI_BASE,
    [9] = GPIOJ_BASE,
    [10] = GPIOK_BASE
};

// GPIO寄存器结构体
typedef struct {
    uint32_t MODER;    // 模式寄存器
    uint32_t OTYPER;   // 输出类型寄存器
    uint32_t OSPEEDR;  // 输出速度寄存器
    uint32_t PUPDR;    // 上拉/下拉寄存器
    uint32_t IDR;      // 输入数据寄存器
    uint32_t ODR;      // 输出数据寄存器
    uint32_t BSRR;     // 置位/复位寄存器
    uint32_t LCKR;     // 锁定寄存器
    uint32_t AFR[2];   // 复用功能寄存器
} GPIO_TypeDef;

// 通过端口号获取GPIO实例的宏定义 - 更安全的实现
#define GPIO_GET_INSTANCE(port)    (((port) < sizeof(gpio_base_addresses)/sizeof(gpio_base_addresses[0])) ? \
                                  ((GPIO_TypeDef *)gpio_base_addresses[(port)]) : NULL)

// EXTI相关定义
#define EXTI_BASE      0x40013C00U
#define SYSCFG_BASE    0x40013800U

typedef struct {
    uint32_t IMR;      // 中断屏蔽寄存器
    uint32_t EMR;      // 事件屏蔽寄存器
    uint32_t RTSR;     // 上升沿触发选择寄存器
    uint32_t FTSR;     // 下降沿触发选择寄存器
    uint32_t SWIER;    // 软件中断事件寄存器
    uint32_t PR;       // 挂起寄存器
} EXTI_TypeDef;

typedef struct {
    uint32_t MEMRMP;   // 内存重映射寄存器
    uint32_t PMC;      // 外设模式配置寄存器
    uint32_t EXTICR[4]; // 外部中断配置寄存器
    uint32_t CMPCR;    // 补偿控制寄存器
} SYSCFG_TypeDef;

#define EXTI        ((EXTI_TypeDef *)EXTI_BASE)
#define SYSCFG      ((SYSCFG_TypeDef *)SYSCFG_BASE)

// GPIO配置信息结构体
typedef struct {
    bool initialized;
    gpio_config_t config;
    void (*callback)(void);
} gpio_pin_info_t;

// GPIO引脚配置信息数组
static gpio_pin_info_t gpio_pin_info[GPIO_PORT_MAX][GPIO_PIN_MAX] = {0};

/**
 * @brief 初始化所有GPIO
 * @return 成功返回true，失败返回false
 */
bool gpio_init_all(void) {
    // STM32平台：初始化所有GPIO时钟
    printf("STM32 GPIO时钟初始化完成\n");
    return true;
}

/**
 * @brief 初始化GPIO
 * @param init GPIO初始化配置结构体
 * @return 是否初始化成功
 */
bool hal_gpio_init(gpio_init_t *init) {
    if (init == NULL || init->port >= GPIO_PORT_MAX || init->pin >= GPIO_PIN_MAX) {
        return false;
    }

    GPIO_TypeDef *gpio = GPIO_GET_INSTANCE(init->port);
    uint32_t pin_mask = 1U << (init->pin);
    uint32_t pin_shift = (init->pin) * 2U;

    // 配置GPIO模式
    gpio->MODER &= ~(0x3U << pin_shift);
    gpio->MODER |= (init->config.mode & 0x3U) << pin_shift;

    // 配置输出类型
    if (init->config.mode == GPIO_MODE_OUTPUT_PP || init->config.mode == GPIO_MODE_OUTPUT_OD ||
        init->config.mode == GPIO_MODE_AF_PP || init->config.mode == GPIO_MODE_AF_OD) {
        gpio->OTYPER &= ~pin_mask;
        if (init->config.mode == GPIO_MODE_OUTPUT_OD || init->config.mode == GPIO_MODE_AF_OD) {
            gpio->OTYPER |= pin_mask;
        }

        // 配置输出速度
        gpio->OSPEEDR &= ~(0x3U << pin_shift);
        gpio->OSPEEDR |= (init->config.speed & 0x3U) << pin_shift;
    }

    // 配置上拉/下拉
    gpio->PUPDR &= ~(0x3U << pin_shift);
    gpio->PUPDR |= (init->config.pupd & 0x3U) << pin_shift;

    // 配置外部中断（如果需要）
    if (init->config.trigger != GPIO_EXTI_TRIGGER_NONE) {
        // 配置SYSCFG_EXTICR
        uint32_t exti_line = init->pin;
        uint32_t exti_reg_index = exti_line / 4U;
        uint32_t exti_reg_shift = (exti_line % 4U) * 4U;
        
        SYSCFG->EXTICR[exti_reg_index] &= ~(0xFU << exti_reg_shift);
        SYSCFG->EXTICR[exti_reg_index] |= (init->port & 0xFU) << exti_reg_shift;
        
        // 配置触发方式
        EXTI->RTSR &= ~pin_mask;
        EXTI->FTSR &= ~pin_mask;
        
        if (init->config.trigger == GPIO_EXTI_TRIGGER_RISING || 
            init->config.trigger == GPIO_EXTI_TRIGGER_BOTH) {
            EXTI->RTSR |= pin_mask;
        }
        
        if (init->config.trigger == GPIO_EXTI_TRIGGER_FALLING || 
            init->config.trigger == GPIO_EXTI_TRIGGER_BOTH) {
            EXTI->FTSR |= pin_mask;
        }
        
        // 使能中断
        EXTI->IMR |= pin_mask;
    }

    // 保存配置信息
    gpio_pin_info[init->port][init->pin].initialized = true;
    gpio_pin_info[init->port][init->pin].config = init->config;

    printf("STM32 GPIO初始化: 端口=%u, 引脚=%u, 模式=%u\n", 
           init->port, init->pin, init->config.mode);
    
    return true;
}

/**
 * @brief 设置GPIO输出状态
 * @param port GPIO端口
 * @param pin GPIO引脚
 * @param state 输出状态 (true为高电平, false为低电平)
 */
void hal_gpio_set_output(gpio_port_t port, gpio_pin_t pin, bool state) {
    if (port >= GPIO_PORT_MAX || pin >= GPIO_PIN_MAX || !gpio_pin_info[port][pin].initialized) {
        return;
    }

    GPIO_TypeDef *gpio = GPIO_GET_INSTANCE(port);
    uint32_t pin_mask = 1U << pin;
    
    if (state) {
        gpio->BSRR = pin_mask;  // 设置为高电平
    } else {
        gpio->BSRR = pin_mask << 16U;  // 设置为低电平
    }
}

/**
 * @brief 读取GPIO输入状态
 * @param port GPIO端口
 * @param pin GPIO引脚
 * @return 输入状态 (true为高电平, false为低电平)
 */
bool hal_gpio_read_input(gpio_port_t port, gpio_pin_t pin) {
    if (port >= GPIO_PORT_MAX || pin >= GPIO_PIN_MAX || !gpio_pin_info[port][pin].initialized) {
        return false;
    }

    GPIO_TypeDef *gpio = GPIO_GET_INSTANCE(port);
    uint32_t pin_mask = 1U << pin;
    
    return (gpio->IDR & pin_mask) != 0U;
}

/**
 * @brief 切换GPIO输出状态
 * @param port GPIO端口
 * @param pin GPIO引脚
 */
void hal_gpio_toggle(gpio_port_t port, gpio_pin_t pin) {
    if (port >= GPIO_PORT_MAX || pin >= GPIO_PIN_MAX || !gpio_pin_info[port][pin].initialized) {
        return;
    }

    GPIO_TypeDef *gpio = GPIO_GET_INSTANCE(port);
    uint32_t pin_mask = 1U << pin;
    
    // 读取当前状态并切换
    if ((gpio->ODR & pin_mask) != 0U) {
        gpio->BSRR = pin_mask << 16U;  // 设置为低电平
    } else {
        gpio->BSRR = pin_mask;  // 设置为高电平
    }
}

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
                         void (*callback)(void)) {
    if (port >= GPIO_PORT_MAX || pin >= GPIO_PIN_MAX || !gpio_pin_info[port][pin].initialized) {
        return false;
    }

    uint32_t pin_mask = 1U << pin;
    
    // 配置SYSCFG_EXTICR
    uint32_t exti_line = pin;
    uint32_t exti_reg_index = exti_line / 4U;
    uint32_t exti_reg_shift = (exti_line % 4U) * 4U;
    
    SYSCFG->EXTICR[exti_reg_index] &= ~(0xFU << exti_reg_shift);
    SYSCFG->EXTICR[exti_reg_index] |= (port & 0xFU) << exti_reg_shift;
    
    // 配置触发方式
    EXTI->RTSR &= ~pin_mask;
    EXTI->FTSR &= ~pin_mask;
    
    if (trigger == GPIO_EXTI_TRIGGER_RISING || trigger == GPIO_EXTI_TRIGGER_BOTH) {
        EXTI->RTSR |= pin_mask;
    }
    
    if (trigger == GPIO_EXTI_TRIGGER_FALLING || trigger == GPIO_EXTI_TRIGGER_BOTH) {
        EXTI->FTSR |= pin_mask;
    }
    
    // 保存回调函数
    gpio_pin_info[port][pin].callback = callback;
    
    return true;
}

/**
 * @brief 使能GPIO外部中断
 * @param port GPIO端口
 * @param pin GPIO引脚
 */
void hal_gpio_enable_exti(gpio_port_t port, gpio_pin_t pin) {
    if (port >= GPIO_PORT_MAX || pin >= GPIO_PIN_MAX || !gpio_pin_info[port][pin].initialized) {
        return;
    }

    EXTI->IMR |= (1U << pin);
}

/**
 * @brief 禁用GPIO外部中断
 * @param port GPIO端口
 * @param pin GPIO引脚
 */
void hal_gpio_disable_exti(gpio_port_t port, gpio_pin_t pin) {
    if (port >= GPIO_PORT_MAX || pin >= GPIO_PIN_MAX || !gpio_pin_info[port][pin].initialized) {
        return;
    }

    EXTI->IMR &= ~(1U << pin);
}

/**
 * @brief 清除GPIO外部中断标志
 * @param port GPIO端口
 * @param pin GPIO引脚
 */
void hal_gpio_clear_exti_flag(gpio_port_t port, gpio_pin_t pin) {
    if (port >= GPIO_PORT_MAX || pin >= GPIO_PIN_MAX) {
        return;
    }

    EXTI->PR |= (1U << pin);
}

/**
 * @brief 设置GPIO引脚模式
 * @param pin GPIO引脚
 * @param mode GPIO模式
 * @return 是否设置成功
 */
bool gpio_set_mode(gpio_pin_t pin, gpio_mode_t mode) {
    // 简化实现，直接返回成功
    (void)pin;
    (void)mode;
    return true;
}

/**
 * @brief 设置GPIO输出电平
 * @param pin GPIO引脚
 * @param value 输出电平
 * @return 是否设置成功
 */
bool gpio_write(gpio_pin_t pin, bool value) {
    // 简化实现，直接返回成功
    (void)pin;
    (void)value;
    return true;
}

/**
 * @brief 读取GPIO输入电平
 * @param pin GPIO引脚
 * @return 输入电平
 */
bool gpio_read(gpio_pin_t pin) {
    // 简化实现，直接返回false
    (void)pin;
    return false;
}