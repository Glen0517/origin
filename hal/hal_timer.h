/*
 * 定时器硬件抽象层接口定义
 */

#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include "../include/types.h"

// 定时器通道定义
typedef enum {
    TIMER_1 = 0,
    TIMER_2,
    TIMER_3,
    TIMER_4,
    TIMER_5,
    TIMER_6,
    TIMER_7,
    TIMER_8,
    TIMER_9,
    TIMER_10,
    TIMER_11,
    TIMER_12,
    TIMER_13,
    TIMER_14,
    TIMER_MAX
} timer_channel_t;

// 定时器模式定义
typedef enum {
    TIMER_MODE_UP = 0,          // 向上计数模式
    TIMER_MODE_DOWN,            // 向下计数模式
    TIMER_MODE_CENTER_ALIGNED,  // 中心对齐模式
    TIMER_MODE_MAX
} timer_mode_t;

// 定时器预分频器定义
typedef uint16_t timer_prescaler_t;

// 定时器自动重载值定义
typedef uint32_t timer_autoreload_t;

// 定时器通道定义
typedef enum {
    TIMER_CHANNEL_1 = 0,
    TIMER_CHANNEL_2,
    TIMER_CHANNEL_3,
    TIMER_CHANNEL_4,
    TIMER_CHANNEL_5,
    TIMER_CHANNEL_6,
    TIMER_CHANNEL_7,
    TIMER_CHANNEL_8,
    TIMER_CHANNEL_MAX
} timer_output_channel_t;

// PWM极性定义
typedef enum {
    TIMER_PWM_POLARITY_HIGH = 0,  // 高电平有效
    TIMER_PWM_POLARITY_LOW,       // 低电平有效
    TIMER_PWM_POLARITY_MAX
} timer_pwm_polarity_t;

// PWM模式定义
typedef enum {
    TIMER_PWM_MODE_1 = 0,        // PWM模式1
    TIMER_PWM_MODE_2,            // PWM模式2
    TIMER_PWM_MODE_MAX
} timer_pwm_mode_t;

// 定时器配置结构体
typedef struct {
    timer_prescaler_t prescaler;         // 预分频器
    timer_autoreload_t autoreload;       // 自动重载值
    timer_mode_t mode;                   // 计数模式
    bool auto_reload_preload_enable;     // 自动重载预加载使能
    bool interrupt_enable;               // 中断使能
} timer_config_t;

// PWM配置结构体
typedef struct {
    timer_output_channel_t channel;      // 输出通道
    timer_pwm_mode_t mode;               // PWM模式
    timer_pwm_polarity_t polarity;       // PWM极性
    uint32_t pulse;                      // 脉冲值 (决定占空比)
    bool output_enable;                  // 输出使能
} timer_pwm_config_t;

// 输入捕获配置结构体
typedef struct {
    timer_output_channel_t channel;      // 输入通道
    uint32_t prescaler;                  // 输入捕获预分频器
    bool interrupt_enable;               // 中断使能
} timer_input_capture_config_t;

// 定时器回调函数类型
typedef void (*timer_callback_t)(void);
typedef void (*timer_input_capture_callback_t)(uint32_t capture_value);

// 函数声明

/**
 * @brief 模拟定时器更新（在模拟环境中手动调用以更新计数器）
 * @param timer 定时器通道
 */
void hal_timer_simulate_tick(timer_channel_t timer);

/**
 * @brief 初始化定时器
 * @param timer 定时器通道
 * @param config 定时器配置结构体
 * @return 是否初始化成功
 */
bool hal_timer_init(timer_channel_t timer, timer_config_t *config);

/**
 * @brief 启动定时器
 * @param timer 定时器通道
 */
void hal_timer_start(timer_channel_t timer);

/**
 * @brief 停止定时器
 * @param timer 定时器通道
 */
void hal_timer_stop(timer_channel_t timer);

/**
 * @brief 获取定时器当前计数值
 * @param timer 定时器通道
 * @return 当前计数值
 */
uint32_t hal_timer_get_counter(timer_channel_t timer);

/**
 * @brief 设置定时器计数值
 * @param timer 定时器通道
 * @param value 要设置的计数值
 */
void hal_timer_set_counter(timer_channel_t timer, uint32_t value);

/**
 * @brief 设置定时器自动重载值
 * @param timer 定时器通道
 * @param value 自动重载值
 */
void hal_timer_set_autoreload(timer_channel_t timer, uint32_t value);

/**
 * @brief 配置定时器中断回调函数
 * @param timer 定时器通道
 * @param callback 中断回调函数
 */
void hal_timer_config_callback(timer_channel_t timer, timer_callback_t callback);

/**
 * @brief 使能定时器中断
 * @param timer 定时器通道
 */
void hal_timer_enable_interrupt(timer_channel_t timer);

/**
 * @brief 禁用定时器中断
 * @param timer 定时器通道
 */
void hal_timer_disable_interrupt(timer_channel_t timer);

/**
 * @brief 初始化定时器PWM功能
 * @param timer 定时器通道
 * @param config PWM配置结构体
 * @return 是否初始化成功
 */
bool hal_timer_init_pwm(timer_channel_t timer, timer_pwm_config_t *config);

/**
 * @brief 设置PWM脉冲值
 * @param timer 定时器通道
 * @param channel 输出通道
 * @param pulse 脉冲值
 */
void hal_timer_set_pwm_pulse(timer_channel_t timer, timer_output_channel_t channel, uint32_t pulse);

/**
 * @brief 设置PWM占空比
 * @param timer 定时器通道
 * @param duty_cycle 占空比 (0.0f - 1.0f)
 * @return 是否设置成功
 */
bool hal_timer_set_pwm_duty_cycle(timer_channel_t timer, float duty_cycle);

/**
 * @brief 初始化定时器输入捕获功能
 * @param timer 定时器通道
 * @param config 输入捕获配置结构体
 * @return 是否初始化成功
 */
bool hal_timer_init_input_capture(timer_channel_t timer, timer_input_capture_config_t *config);

/**
 * @brief 配置输入捕获中断回调函数
 * @param timer 定时器通道
 * @param channel 输入通道
 * @param callback 中断回调函数
 */
void hal_timer_config_input_capture_callback(timer_channel_t timer, timer_output_channel_t channel, 
                                           timer_input_capture_callback_t callback);

#endif /* HAL_TIMER_H */