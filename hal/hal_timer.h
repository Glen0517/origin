/*
 * 定时器硬件抽象层接口定义
 */

/**
 * @file    hal_timer.h
 * @brief   定时器硬件抽象层接口 - STM32库风格
 * @author  Dear Master
 * @date    2025-11-17
 * @version v1.0
 */

#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include "../include/types.h"
#include "stm32f4xx.h"
#include <stdbool.h>

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

/* PWM功能函数声明 - 使用STM32库风格 */

/**
 * @brief 初始化PWM（独立PWM模块风格）
 * @param timer PWM定时器通道
 * @param channel PWM通道
 * @param frequency PWM频率
 * @return 成功返回true，失败返回false
 */
bool pwm_init(timer_channel_t timer, timer_output_channel_t channel, uint32_t frequency);

/**
 * @brief 启动PWM输出
 * @param timer PWM定时器通道
 * @param channel PWM通道
 * @return 成功返回true，失败返回false
 */
bool pwm_start(timer_channel_t timer, timer_output_channel_t channel);

/**
 * @brief 停止PWM输出
 * @param timer PWM定时器通道
 * @param channel PWM通道
 * @return 成功返回true，失败返回false
 */
bool pwm_stop(timer_channel_t timer, timer_output_channel_t channel);

/**
 * @brief 设置PWM占空比
 * @param timer PWM定时器通道
 * @param channel PWM通道
 * @param duty_cycle 占空比 (0.0f - 1.0f)
 * @return 成功返回true，失败返回false
 */
bool pwm_set_duty_cycle(timer_channel_t timer, timer_output_channel_t channel, float duty_cycle);

/**
 * @brief 设置PWM脉冲宽度
 * @param timer PWM定时器通道
 * @param channel PWM通道
 * @param pulse 脉冲宽度值
 * @return 成功返回true，失败返回false
 */
bool pwm_set_pulse(timer_channel_t timer, timer_output_channel_t channel, uint32_t pulse);

/**
 * @brief 生成PWM脉冲
 * @param timer PWM定时器通道
 * @param channel PWM通道
 * @param pulse 脉冲宽度
 * @param delay_us 脉冲延迟(微秒)
 * @return 成功返回true，失败返回false
 */
bool pwm_generate_pulse(timer_channel_t timer, timer_output_channel_t channel, uint32_t pulse, uint32_t delay_us);

/* ADC功能函数声明 - 使用STM32库风格 */

/**
 * @brief ADC通道枚举
 */
typedef enum {
    ADC_CHANNEL_0 = 0U,      /*!< ADC通道0 - PA0 */
    ADC_CHANNEL_1,           /*!< ADC通道1 - PA1 */
    ADC_CHANNEL_2,           /*!< ADC通道2 - PA2 */
    ADC_CHANNEL_3,           /*!< ADC通道3 - PA3 */
    ADC_CHANNEL_4,           /*!< ADC通道4 - PA4 */
    ADC_CHANNEL_5,           /*!< ADC通道5 - PA5 */
    ADC_CHANNEL_6,           /*!< ADC通道6 - PA6 */
    ADC_CHANNEL_7,           /*!< ADC通道7 - PA7 */
    ADC_CHANNEL_8,           /*!< ADC通道8 - PB0 */
    ADC_CHANNEL_9,           /*!< ADC通道9 - PB1 */
    ADC_CHANNEL_10,          /*!< ADC通道10 - PC0 */
    ADC_CHANNEL_11,          /*!< ADC通道11 - PC1 */
    ADC_CHANNEL_12,          /*!< ADC通道12 - PC2 */
    ADC_CHANNEL_13,          /*!< ADC通道13 - PC3 */
    ADC_CHANNEL_14,          /*!< ADC通道14 - PC4 */
    ADC_CHANNEL_15,          /*!< ADC通道15 - PC5 */
    ADC_CHANNEL_TEMP,        /*!< 内部温度传感器 */
    ADC_CHANNEL_VREFINT,     /*!< 内部参考电压 */
    ADC_CHANNEL_VBAT         /*!< 电池电压 */
} adc_channel_t;

/**
 * @brief ADC分辨率枚举
 */
typedef enum {
    ADC_RESOLUTION_12BIT = 0U,  /*!< 12位分辨率 */
    ADC_RESOLUTION_10BIT,       /*!< 10位分辨率 */
    ADC_RESOLUTION_8BIT,        /*!< 8位分辨率 */
    ADC_RESOLUTION_6BIT         /*!< 6位分辨率 */
} adc_resolution_t;

/**
 * @brief ADC对齐方式枚举
 */
typedef enum {
    ADC_ALIGNMENT_RIGHT = 0U,   /*!< 右对齐 */
    ADC_ALIGNMENT_LEFT          /*!< 左对齐 */
} adc_alignment_t;

/**
 * @brief ADC扫描模式
 */
typedef enum {
    ADC_SCAN_MODE_DISABLED = 0U,   /*!< 禁用扫描模式 */
    ADC_SCAN_MODE_ENABLED          /*!< 启用扫描模式 */
} adc_scan_mode_t;

/**
 * @brief ADC连续转换模式
 */
typedef enum {
    ADC_CONTINUOUS_MODE_DISABLED = 0U,  /*!< 禁用连续转换 */
    ADC_CONTINUOUS_MODE_ENABLED         /*!< 启用连续转换 */
} adc_continuous_mode_t;

/**
 * @brief ADC配置结构体
 */
typedef struct {
    ADC_TypeDef *Instance;          /*!< ADC寄存器基地址 */
    adc_resolution_t Resolution;    /*!< ADC分辨率 */
    adc_alignment_t DataAlignment;  /*!< 数据对齐方式 */
    adc_scan_mode_t ScanConvMode;   /*!< 扫描模式 */
    adc_continuous_mode_t ContinuousConvMode; /*!< 连续转换模式 */
    uint32_t NbrOfConversion;       /*!< 转换数量 */
    uint32_t DMA_AccessMode;        /*!< DMA访问模式 */
    uint32_t ScanConvMode1;         /*!< 扫描转换模式1 */
} adc_config_t;

/**
 * @brief ADC句柄结构体
 */
typedef struct {
    ADC_TypeDef *Instance;          /*!< ADC寄存器基地址 */
    adc_config_t Init;              /*!< ADC初始化参数 */
    uint32_t State;                 /*!< ADC状态 */
    uint32_t ErrorCode;             /*!< ADC错误代码 */
} adc_handle_t;

/* ADC默认配置 - STM32标准配置 */
#define ADC_DEFAULT_CONFIG                                        \
    .Instance = ADC1,                                         \
    .Resolution = ADC_RESOLUTION_12BIT,                       \
    .DataAlignment = ADC_ALIGNMENT_RIGHT,                     \
    .ScanConvMode = ADC_SCAN_MODE_DISABLED,                   \
    .ContinuousConvMode = ADC_CONTINUOUS_MODE_DISABLED,       \
    .NbrOfConversion = 1U,                                   \
    .DMA_AccessMode = ADC_DMAACCESSMODE_DISABLED,             \
    .ScanConvMode1 = ADC_SCAN_CONV_DISABLE

/* ADC错误代码定义 */
#define ADC_ERROR_NONE                 (0x00U)
#define ADC_ERROR_INVALID_CHANNEL      (0x01U)
#define ADC_ERROR_INVALID_RESOLUTION   (0x02U)
#define ADC_ERROR_INVALID_INSTANCE     (0x04U)
#define ADC_ERROR_TIMEOUT              (0x08U)
#define ADC_ERROR_NOT_READY            (0x10U)
#define ADC_ERROR_CALIBRATION          (0x20U)

/**
 * @brief 初始化ADC
 * @param adc_handle ADC句柄指针
 * @param config ADC配置指针
 * @return 成功返回true，失败返回false
 */
bool adc_init(adc_handle_t *adc_handle, adc_config_t *config);

/**
 * @brief 校准ADC
 * @param adc_handle ADC句柄指针
 * @return 成功返回true，失败返回false
 */
bool adc_calibrate(adc_handle_t *adc_handle);

/**
 * @brief 启动ADC转换
 * @param adc_handle ADC句柄指针
 * @param channel ADC通道
 * @return 成功返回true，失败返回false
 */
bool adc_start_conversion(adc_handle_t *adc_handle, adc_channel_t channel);

/**
 * @brief 停止ADC转换
 * @param adc_handle ADC句柄指针
 * @return 成功返回true，失败返回false
 */
bool adc_stop_conversion(adc_handle_t *adc_handle);

/**
 * @brief 读取ADC转换值
 * @param adc_handle ADC句柄指针
 * @return ADC转换值
 */
uint16_t adc_read_value(adc_handle_t *adc_handle);

/**
 * @brief 读取指定通道的ADC值
 * @param adc_handle ADC句柄指针
 * @param channel ADC通道
 * @return ADC转换值
 */
uint16_t adc_read_channel(adc_handle_t *adc_handle, adc_channel_t channel);

/**
 * @brief 配置ADC通道
 * @param adc_handle ADC句柄指针
 * @param channel ADC通道
 * @param rank 通道排名
 * @return 成功返回true，失败返回false
 */
bool adc_config_channel(adc_handle_t *adc_handle, adc_channel_t channel, uint32_t rank);

/**
 * @brief 启用ADC
 * @param adc_handle ADC句柄指针
 * @return 成功返回true，失败返回false
 */
bool adc_enable(adc_handle_t *adc_handle);

/**
 * @brief 禁用ADC
 * @param adc_handle ADC句柄指针
 * @return 成功返回true，失败返回false
 */
bool adc_disable(adc_handle_t *adc_handle);

/**
 * @brief 读取ADC状态
 * @param adc_handle ADC句柄指针
 * @return ADC状态
 */
uint32_t adc_get_state(adc_handle_t *adc_handle);

/**
 * @brief 获取ADC错误代码
 * @param adc_handle ADC句柄指针
 * @return ADC错误代码
 */
uint32_t adc_get_error(adc_handle_t *adc_handle);

/**
 * @brief ADC中断处理函数
 * @param adc_handle ADC句柄指针
 */
void adc_irq_handler(adc_handle_t *adc_handle);

/**
 * @brief 启用ADC中断
 * @param adc_handle ADC句柄指针
 */
void adc_enable_irq(adc_handle_t *adc_handle);

/**
 * @brief 禁用ADC中断
 * @param adc_handle ADC句柄指针
 */
void adc_disable_irq(adc_handle_t *adc_handle);

/**
 * @brief 配置ADC模拟看门狗
 * @param adc_handle ADC句柄指针
 * @param low_threshold 低阈值
 * @param high_threshold 高阈值
 * @param channel ADC通道
 * @return 成功返回true，失败返回false
 */
bool adc_analog_watchdog_config(adc_handle_t *adc_handle, uint16_t low_threshold, 
                                uint16_t high_threshold, adc_channel_t channel);

/**
 * @brief 启用ADC模拟看门狗
 * @param adc_handle ADC句柄指针
 * @return 成功返回true，失败返回false
 */
bool adc_analog_watchdog_enable(adc_handle_t *adc_handle);

/**
 * @brief 禁用ADC模拟看门狗
 * @param adc_handle ADC句柄指针
 * @return 成功返回true，失败返回false
 */
bool adc_analog_watchdog_disable(adc_handle_t *adc_handle);

#endif /* HAL_TIMER_H */