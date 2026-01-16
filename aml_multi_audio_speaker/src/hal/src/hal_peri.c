/**
 * @file hal_peri.c
 * @brief 外设硬件抽象实现
 * @details 实现外设硬件抽象层的接口函数，封装Amlogic GPIO、PWM等SDK的功能
 * @author AML Audio Team
 * @date 2026-01-16
 */

#include "hal_peri.h"
#include "logger.h"
#include <aml_gpio.h>
#include <aml_pwm.h>

static bool g_peri_init = false;

/**
 * @brief 初始化外设硬件
 * @details 初始化Amlogic GPIO、PWM等SDK，准备外设硬件
 * @return 初始化结果：0表示成功，非0表示失败
 */
int hal_peri_init(void) {
    if (g_peri_init) {
        LOG_INFO("HAL peripheral already initialized");
        return SUCCESS;
    }
    
    // 初始化GPIO SDK
    if (aml_gpio_init() != 0) {
        LOG_ERROR("Amlogic GPIO SDK init failed");
        return FAILURE;
    }
    
    // 初始化PWM SDK
    if (aml_pwm_init() != 0) {
        LOG_ERROR("Amlogic PWM SDK init failed");
        // 即使PWM初始化失败，也继续执行，因为GPIO可能仍然可用
        LOG_WARN("Continue with GPIO only");
    }
    
    g_peri_init = true;
    LOG_INFO("HAL peripheral init success");
    return SUCCESS;
}

/**
 * @brief 反初始化外设硬件
 * @details 反初始化Amlogic GPIO、PWM等SDK，清理外设硬件资源
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int hal_peri_deinit(void) {
    if (!g_peri_init) {
        LOG_INFO("HAL peripheral not initialized");
        return SUCCESS;
    }
    
    // 反初始化PWM SDK
    if (aml_pwm_deinit() != 0) {
        LOG_ERROR("Amlogic PWM SDK deinit failed");
        // 即使PWM反初始化失败，也继续执行
    }
    
    // 反初始化GPIO SDK
    if (aml_gpio_deinit() != 0) {
        LOG_ERROR("Amlogic GPIO SDK deinit failed");
        return FAILURE;
    }
    
    g_peri_init = false;
    LOG_INFO("HAL peripheral deinit success");
    return SUCCESS;
}

/**
 * @brief 设置GPIO引脚值
 * @details 设置指定GPIO引脚的输出值
 * @param pin GPIO引脚号
 * @param value 引脚值：1表示高电平，0表示低电平
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_gpio_set_value(int pin, int value) {
    if (!g_peri_init) {
        LOG_ERROR("HAL peripheral not initialized");
        return FAILURE;
    }
    
    if (value != 0 && value != 1) {
        LOG_ERROR("Invalid GPIO value: %d", value);
        return FAILURE;
    }
    
    int ret = aml_gpio_set_value(pin, value);
    if (ret != 0) {
        LOG_ERROR("Set GPIO %d value failed: %d", pin, ret);
        return FAILURE;
    }
    
    LOG_DEBUG("GPIO %d set to: %d", pin, value);
    return SUCCESS;
}

/**
 * @brief 获取GPIO引脚值
 * @details 获取指定GPIO引脚的输入值
 * @param pin GPIO引脚号
 * @return 引脚值：1表示高电平，0表示低电平，失败返回-1
 */
int hal_gpio_get_value(int pin) {
    if (!g_peri_init) {
        LOG_ERROR("HAL peripheral not initialized");
        return -1;
    }
    
    int value = aml_gpio_get_value(pin);
    if (value < 0) {
        LOG_ERROR("Get GPIO %d value failed", pin);
        return -1;
    }
    
    LOG_DEBUG("GPIO %d value: %d", pin, value);
    return value;
}

/**
 * @brief 设置GPIO引脚方向
 * @details 设置指定GPIO引脚的方向（输入/输出）
 * @param pin GPIO引脚号
 * @param direction 方向：1表示输出，0表示输入
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_gpio_set_direction(int pin, int direction) {
    if (!g_peri_init) {
        LOG_ERROR("HAL peripheral not initialized");
        return FAILURE;
    }
    
    if (direction != 0 && direction != 1) {
        LOG_ERROR("Invalid GPIO direction: %d", direction);
        return FAILURE;
    }
    
    int ret = aml_gpio_set_direction(pin, direction);
    if (ret != 0) {
        LOG_ERROR("Set GPIO %d direction failed: %d", pin, ret);
        return FAILURE;
    }
    
    LOG_DEBUG("GPIO %d direction set to: %s", pin, direction ? "output" : "input");
    return SUCCESS;
}

/**
 * @brief 设置PWM占空比
 * @details 设置指定PWM通道的占空比
 * @param channel PWM通道号
 * @param duty 占空比，范围为0-100
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_pwm_set_duty(int channel, int duty) {
    if (!g_peri_init) {
        LOG_ERROR("HAL peripheral not initialized");
        return FAILURE;
    }
    
    if (duty < 0 || duty > 100) {
        LOG_ERROR("Invalid PWM duty: %d", duty);
        return FAILURE;
    }
    
    int ret = aml_pwm_set_duty(channel, duty);
    if (ret != 0) {
        LOG_ERROR("Set PWM %d duty failed: %d", channel, ret);
        return FAILURE;
    }
    
    LOG_DEBUG("PWM %d duty set to: %d%%", channel, duty);
    return SUCCESS;
}

/**
 * @brief 设置PWM频率
 * @details 设置指定PWM通道的频率
 * @param channel PWM通道号
 * @param freq 频率值，单位为Hz
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_pwm_set_frequency(int channel, int freq) {
    if (!g_peri_init) {
        LOG_ERROR("HAL peripheral not initialized");
        return FAILURE;
    }
    
    if (freq <= 0) {
        LOG_ERROR("Invalid PWM frequency: %d", freq);
        return FAILURE;
    }
    
    int ret = aml_pwm_set_frequency(channel, freq);
    if (ret != 0) {
        LOG_ERROR("Set PWM %d frequency failed: %d", channel, ret);
        return FAILURE;
    }
    
    LOG_DEBUG("PWM %d frequency set to: %d Hz", channel, freq);
    return SUCCESS;
}

/**
 * @brief 启用PWM输出
 * @details 启用指定PWM通道的输出
 * @param channel PWM通道号
 * @return 启用结果：0表示成功，非0表示失败
 */
int hal_pwm_enable(int channel) {
    if (!g_peri_init) {
        LOG_ERROR("HAL peripheral not initialized");
        return FAILURE;
    }
    
    int ret = aml_pwm_enable(channel);
    if (ret != 0) {
        LOG_ERROR("Enable PWM %d failed: %d", channel, ret);
        return FAILURE;
    }
    
    LOG_DEBUG("PWM %d enabled", channel);
    return SUCCESS;
}

/**
 * @brief 禁用PWM输出
 * @details 禁用指定PWM通道的输出
 * @param channel PWM通道号
 * @return 禁用结果：0表示成功，非0表示失败
 */
int hal_pwm_disable(int channel) {
    if (!g_peri_init) {
        LOG_ERROR("HAL peripheral not initialized");
        return FAILURE;
    }
    
    int ret = aml_pwm_disable(channel);
    if (ret != 0) {
        LOG_ERROR("Disable PWM %d failed: %d", channel, ret);
        return FAILURE;
    }
    
    LOG_DEBUG("PWM %d disabled", channel);
    return SUCCESS;
}