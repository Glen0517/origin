/**
 * @file hal_peri.h
 * @brief 外设硬件抽象接口
 * @details 定义外设硬件抽象层的接口函数，封装Amlogic GPIO、PWM等SDK的功能
 * @author AML Audio Team
 * @date 2026-01-16
 */

#ifndef HAL_PERI_H
#define HAL_PERI_H

#include "common_def.h"

/**
 * @brief 初始化外设硬件
 * @details 初始化Amlogic GPIO、PWM等SDK，准备外设硬件
 * @return 初始化结果：0表示成功，非0表示失败
 */
int hal_peri_init(void);

/**
 * @brief 反初始化外设硬件
 * @details 反初始化Amlogic GPIO、PWM等SDK，清理外设硬件资源
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int hal_peri_deinit(void);

/**
 * @brief 设置GPIO引脚值
 * @details 设置指定GPIO引脚的输出值
 * @param pin GPIO引脚号
 * @param value 引脚值：1表示高电平，0表示低电平
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_gpio_set_value(int pin, int value);

/**
 * @brief 获取GPIO引脚值
 * @details 获取指定GPIO引脚的输入值
 * @param pin GPIO引脚号
 * @return 引脚值：1表示高电平，0表示低电平，失败返回-1
 */
int hal_gpio_get_value(int pin);

/**
 * @brief 设置GPIO引脚方向
 * @details 设置指定GPIO引脚的方向（输入/输出）
 * @param pin GPIO引脚号
 * @param direction 方向：1表示输出，0表示输入
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_gpio_set_direction(int pin, int direction);

/**
 * @brief 设置PWM占空比
 * @details 设置指定PWM通道的占空比
 * @param channel PWM通道号
 * @param duty 占空比，范围为0-100
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_pwm_set_duty(int channel, int duty);

/**
 * @brief 设置PWM频率
 * @details 设置指定PWM通道的频率
 * @param channel PWM通道号
 * @param freq 频率值，单位为Hz
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_pwm_set_frequency(int channel, int freq);

/**
 * @brief 启用PWM输出
 * @details 启用指定PWM通道的输出
 * @param channel PWM通道号
 * @return 启用结果：0表示成功，非0表示失败
 */
int hal_pwm_enable(int channel);

/**
 * @brief 禁用PWM输出
 * @details 禁用指定PWM通道的输出
 * @param channel PWM通道号
 * @return 禁用结果：0表示成功，非0表示失败
 */
int hal_pwm_disable(int channel);

#endif /* HAL_PERI_H */