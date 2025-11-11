/*
 * 电机驱动实现
 */

#include "motor.h"
#include "../hal/hal_timer.h"
#include "../service/system.h"
#include "../config/config.h"

// 电机配置和状态
static motor_config_t motor_config[MOTOR_CHANNEL_MAX];
static motor_status_t motor_status[MOTOR_CHANNEL_MAX];
static bool motor_driver_initialized = false;

/**
 * @brief 初始化电机驱动
 */
bool motor_init(motor_config_t *config) {
    if (config == NULL || config->channel < 0 || config->channel >= MOTOR_CHANNEL_MAX) {
        return false;
    }
    
    motor_channel_t channel = config->channel;
    
    // 保存配置
    motor_config[channel] = *config;
    
    // 初始化定时器PWM
    timer_pwm_config_t pwm_config = {
        .channel = config->pwm_timer_channel,
        .mode = TIMER_PWM_MODE_1,
        .polarity = TIMER_PWM_POLARITY_HIGH,
        .pulse = 0,  // 初始PWM为0
        .output_enable = true
    };
    
    if (!hal_timer_init_pwm(TIMER_1, &pwm_config)) {
        return false;
    }
    
    // 初始化状态
    motor_status[channel].channel = channel;
    motor_status[channel].current_pwm = 0;
    motor_status[channel].is_enabled = true;
    motor_status[channel].duty_cycle = 0.0f;
    
    // 标记驱动已初始化
    motor_driver_initialized = true;
    
    return true;
}

/**
 * @brief 初始化所有电机
 */
bool motor_init_all(void) {
    bool result = true;
    
    // 创建默认配置
    motor_config_t configs[MOTOR_CHANNEL_MAX] = {
        {
            .channel = MOTOR_CHANNEL_1,
            .pwm_timer_channel = TIMER_CHANNEL_1,
            .pwm_frequency = MOTOR_PWM_FREQUENCY,
            .min_pwm = MOTOR_MIN_PWM,
            .max_pwm = MOTOR_MAX_PWM,
            .idle_pwm = MOTOR_IDLE_PWM,
            .direction = true
        },
        {
            .channel = MOTOR_CHANNEL_2,
            .pwm_timer_channel = TIMER_CHANNEL_2,
            .pwm_frequency = MOTOR_PWM_FREQUENCY,
            .min_pwm = MOTOR_MIN_PWM,
            .max_pwm = MOTOR_MAX_PWM,
            .idle_pwm = MOTOR_IDLE_PWM,
            .direction = true
        },
        {
            .channel = MOTOR_CHANNEL_3,
            .pwm_timer_channel = TIMER_CHANNEL_3,
            .pwm_frequency = MOTOR_PWM_FREQUENCY,
            .min_pwm = MOTOR_MIN_PWM,
            .max_pwm = MOTOR_MAX_PWM,
            .idle_pwm = MOTOR_IDLE_PWM,
            .direction = false // 反转
        },
        {
            .channel = MOTOR_CHANNEL_4,
            .pwm_timer_channel = TIMER_CHANNEL_4,
            .pwm_frequency = MOTOR_PWM_FREQUENCY,
            .min_pwm = MOTOR_MIN_PWM,
            .max_pwm = MOTOR_MAX_PWM,
            .idle_pwm = MOTOR_IDLE_PWM,
            .direction = false // 反转
        }
    };
    
    // 初始化所有电机
    for (uint8_t i = 0; i < MOTOR_CHANNEL_MAX; i++) {
        if (!motor_init(&configs[i])) {
            result = false;
        }
    }
    
    return result;
}

/**
 * @brief 设置电机PWM值
 */
bool motor_set_pwm(motor_channel_t channel, uint16_t pwm) {
    if (!motor_driver_initialized || channel < 0 || channel >= MOTOR_CHANNEL_MAX) {
        return false;
    }
    
    if (!motor_status[channel].is_enabled) {
        return false;
    }
    
    // 限制PWM范围
    if (pwm < motor_config[channel].min_pwm) {
        pwm = 0; // 低于最小值时停止
    } else if (pwm > motor_config[channel].max_pwm) {
        pwm = motor_config[channel].max_pwm;
    }
    
    // 计算占空比
    float duty_cycle = (float)pwm / (float)motor_config[channel].max_pwm;
    
    // 设置PWM
    if (!hal_timer_set_pwm_duty_cycle(motor_config[channel].pwm_timer_channel, duty_cycle)) {
        return false;
    }
    
    // 更新状态
    motor_status[channel].current_pwm = pwm;
    motor_status[channel].duty_cycle = duty_cycle;
    
    return true;
}

/**
 * @brief 设置电机占空比
 */
bool motor_set_duty_cycle(motor_channel_t channel, float duty_cycle) {
    if (!motor_driver_initialized || channel < 0 || channel >= MOTOR_CHANNEL_MAX) {
        return false;
    }
    
    if (!motor_status[channel].is_enabled) {
        return false;
    }
    
    // 限制占空比范围
    if (duty_cycle < 0.0f) {
        duty_cycle = 0.0f;
    } else if (duty_cycle > 1.0f) {
        duty_cycle = 1.0f;
    }
    
    // 计算PWM值
    uint16_t pwm = (uint16_t)(duty_cycle * (float)motor_config[channel].max_pwm);
    
    // 使用PWM设置函数
    return motor_set_pwm(channel, pwm);
}

/**
 * @brief 设置所有电机PWM值
 */
bool motor_set_all_pwm(uint16_t pwm_values[MOTOR_CHANNEL_MAX]) {
    if (!motor_driver_initialized || pwm_values == NULL) {
        return false;
    }
    
    bool result = true;
    
    // 设置每个电机的PWM
    for (uint8_t i = 0; i < MOTOR_CHANNEL_MAX; i++) {
        if (!motor_set_pwm(i, pwm_values[i])) {
            result = false;
        }
    }
    
    return result;
}

/**
 * @brief 停止电机
 */
bool motor_stop(motor_channel_t channel) {
    if (!motor_driver_initialized || channel < 0 || channel >= MOTOR_CHANNEL_MAX) {
        return false;
    }
    
    // 设置PWM为0
    return motor_set_pwm(channel, 0);
}

/**
 * @brief 停止所有电机
 */
bool motor_stop_all(void) {
    if (!motor_driver_initialized) {
        return false;
    }
    
    bool result = true;
    
    // 停止每个电机
    for (uint8_t i = 0; i < MOTOR_CHANNEL_MAX; i++) {
        if (!motor_stop(i)) {
            result = false;
        }
    }
    
    return result;
}

/**
 * @brief 启用电机
 */
bool motor_enable(motor_channel_t channel) {
    if (!motor_driver_initialized || channel < 0 || channel >= MOTOR_CHANNEL_MAX) {
        return false;
    }
    
    motor_status[channel].is_enabled = true;
    return true;
}

/**
 * @brief 禁用电机
 */
bool motor_disable(motor_channel_t channel) {
    if (!motor_driver_initialized || channel < 0 || channel >= MOTOR_CHANNEL_MAX) {
        return false;
    }
    
    // 先停止电机
    motor_stop(channel);
    
    motor_status[channel].is_enabled = false;
    return true;
}

/**
 * @brief 获取电机状态
 */
bool motor_get_status(motor_channel_t channel, motor_status_t *status) {
    if (!motor_driver_initialized || channel < 0 || channel >= MOTOR_CHANNEL_MAX || status == NULL) {
        return false;
    }
    
    *status = motor_status[channel];
    return true;
}

/**
 * @brief 获取所有电机状态
 */
bool motor_get_all_status(motor_status_t status[MOTOR_CHANNEL_MAX]) {
    if (!motor_driver_initialized || status == NULL) {
        return false;
    }
    
    // 复制每个电机的状态
    for (uint8_t i = 0; i < MOTOR_CHANNEL_MAX; i++) {
        status[i] = motor_status[i];
    }
    
    return true;
}

/**
 * @brief 测试电机功能
 */
bool motor_test(motor_channel_t channel, uint32_t test_duration) {
    if (!motor_driver_initialized || channel < 0 || channel >= MOTOR_CHANNEL_MAX) {
        return false;
    }
    
    bool prev_state = motor_status[channel].is_enabled;
    
    // 启用电机
    if (!prev_state) {
        motor_enable(channel);
    }
    
    // 先停止所有其他电机
    for (uint8_t i = 0; i < MOTOR_CHANNEL_MAX; i++) {
        if (i != channel) {
            motor_stop(i);
        }
    }
    
    // 以怠速运行测试电机
    uint16_t test_pwm = motor_config[channel].idle_pwm + (motor_config[channel].max_pwm - motor_config[channel].idle_pwm) / 4;
    
    // 确保测试PWM在安全范围内
    if (test_pwm > motor_config[channel].max_pwm) {
        test_pwm = motor_config[channel].max_pwm;
    }
    
    // 设置测试PWM
    if (!motor_set_pwm(channel, test_pwm)) {
        return false;
    }
    
    // 等待测试时间
    system_delay_ms(test_duration);
    
    // 停止测试电机
    motor_stop(channel);
    
    // 恢复之前的状态
    if (!prev_state) {
        motor_disable(channel);
    }
    
    return true;
}