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

// 平滑控制结构体
typedef struct {
    bool is_active;                // 是否激活平滑控制
    uint16_t target_pwm;           // 目标PWM值
    uint16_t start_pwm;            // 起始PWM值
    uint32_t start_time;           // 起始时间
    uint32_t transition_time;      // 过渡时间(毫秒)
} motor_smooth_control_t;

// 平滑控制状态
static motor_smooth_control_t motor_smooth[MOTOR_CHANNEL_MAX];

// 温度限制配置
#define MOTOR_TEMPERATURE_WARNING 70  // 警告温度(摄氏度)
#define MOTOR_TEMPERATURE_CRITICAL 90 // 临界温度(摄氏度)

// 默认温度读取函数（可由硬件实现替换）
static uint16_t motor_read_temperature_default(motor_channel_t channel) {
    // 默认返回室温作为模拟值
    return 25;
}

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
    motor_status[channel].temperature = 25; // 默认室温
    motor_status[channel].error = MOTOR_ERROR_NONE;
    motor_status[channel].direction_inverted = !config->direction;
    motor_status[channel].last_update_time = 0;
    
    // 初始化平滑控制
    motor_smooth[channel].is_active = false;
    motor_smooth[channel].target_pwm = 0;
    motor_smooth[channel].start_pwm = 0;
    motor_smooth[channel].start_time = 0;
    motor_smooth[channel].transition_time = 0;
    
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
        // 取消平滑控制
        motor_smooth[i].is_active = false;
        
        if (!motor_stop(i)) {
            result = false;
        }
    }
    
    return result;
}

/**
 * @brief 设置电机温度
 */
bool motor_set_temperature(motor_channel_t channel, uint16_t temperature) {
    if (!motor_driver_initialized || channel < 0 || channel >= MOTOR_CHANNEL_MAX) {
        return false;
    }
    
    motor_status[channel].temperature = temperature;
    
    // 检查温度是否过高
    if (temperature >= MOTOR_TEMPERATURE_CRITICAL) {
        motor_status[channel].error = MOTOR_ERROR_OVERHEAT;
        motor_stop(channel);
        return false;
    } else if (temperature >= MOTOR_TEMPERATURE_WARNING && motor_status[channel].error == MOTOR_ERROR_NONE) {
        // 只在没有其他错误时设置警告
        motor_status[channel].error = MOTOR_ERROR_OVERHEAT;
    } else if (temperature < MOTOR_TEMPERATURE_WARNING && motor_status[channel].error == MOTOR_ERROR_OVERHEAT) {
        // 温度恢复正常，清除过热错误
        motor_status[channel].error = MOTOR_ERROR_NONE;
    }
    
    return true;
}

/**
 * @brief 反转电机方向
 */
bool motor_invert_direction(motor_channel_t channel, bool invert) {
    if (!motor_driver_initialized || channel < 0 || channel >= MOTOR_CHANNEL_MAX) {
        return false;
    }
    
    motor_status[channel].direction_inverted = invert;
    
    // 如果电机正在运行，需要重新设置PWM以应用方向变化
    if (motor_status[channel].current_pwm > 0) {
        uint16_t current_pwm = motor_status[channel].current_pwm;
        return motor_set_pwm(channel, current_pwm);
    }
    
    return true;
}

/**
 * @brief 平滑设置电机PWM
 */
bool motor_set_pwm_smooth(motor_channel_t channel, uint16_t target_pwm, uint32_t transition_time) {
    if (!motor_driver_initialized || channel < 0 || channel >= MOTOR_CHANNEL_MAX) {
        return false;
    }
    
    if (!motor_status[channel].is_enabled) {
        return false;
    }
    
    // 限制PWM范围
    if (target_pwm < motor_config[channel].min_pwm) {
        target_pwm = 0; // 低于最小值时停止
    } else if (target_pwm > motor_config[channel].max_pwm) {
        target_pwm = motor_config[channel].max_pwm;
    }
    
    // 如果过渡时间为0或目标值与当前值相同，直接设置
    if (transition_time == 0 || target_pwm == motor_status[channel].current_pwm) {
        return motor_set_pwm(channel, target_pwm);
    }
    
    // 设置平滑控制参数
    motor_smooth[channel].is_active = true;
    motor_smooth[channel].target_pwm = target_pwm;
    motor_smooth[channel].start_pwm = motor_status[channel].current_pwm;
    motor_smooth[channel].start_time = system_get_time_ms();
    motor_smooth[channel].transition_time = transition_time;
    
    return true;
}

/**
 * @brief 检查电机故障
 */
motor_error_t motor_check_error(motor_channel_t channel) {
    if (!motor_driver_initialized || channel < 0 || channel >= MOTOR_CHANNEL_MAX) {
        return MOTOR_ERROR_NONE;
    }
    
    // 读取当前温度
    uint16_t temperature = motor_read_temperature_default(channel);
    motor_set_temperature(channel, temperature);
    
    // 检查PWM是否在有效范围内
    if (motor_status[channel].current_pwm > motor_config[channel].max_pwm) {
        return MOTOR_ERROR_PWM_OUT_OF_RANGE;
    }
    
    // 检查是否超时未更新
    uint32_t current_time = system_get_time_ms();
    if (motor_status[channel].last_update_time > 0 && 
        (current_time - motor_status[channel].last_update_time > 1000)) { // 1秒超时
        return MOTOR_ERROR_ESC_FAILURE;
    }
    
    return motor_status[channel].error;
}

/**
 * @brief 清除电机错误
 */
bool motor_clear_error(motor_channel_t channel) {
    if (!motor_driver_initialized || channel < 0 || channel >= MOTOR_CHANNEL_MAX) {
        return false;
    }
    
    motor_status[channel].error = MOTOR_ERROR_NONE;
    return true;
}

/**
 * @brief 电机校准
 */
bool motor_calibrate(motor_channel_t channel) {
    if (!motor_driver_initialized || channel < 0 || channel >= MOTOR_CHANNEL_MAX) {
        return false;
    }
    
    // 保存当前状态
    bool was_enabled = motor_status[channel].is_enabled;
    uint16_t prev_pwm = motor_status[channel].current_pwm;
    
    // 禁用平滑控制
    motor_smooth[channel].is_active = false;
    
    // 启用电机
    if (!was_enabled) {
        motor_enable(channel);
    }
    
    // 停止其他电机
    for (uint8_t i = 0; i < MOTOR_CHANNEL_MAX; i++) {
        if (i != channel) {
            motor_stop(i);
        }
    }
    
    // 执行校准序列：从最小值到最大值再回到零
    uint16_t cal_steps = 10;
    uint16_t step_delay = 500; // 毫秒
    
    // 校准失败标志
    bool calibration_failed = false;
    
    // 步骤1：设置为最大值
    if (!motor_set_pwm(channel, motor_config[channel].max_pwm)) {
        calibration_failed = true;
    }
    system_delay_ms(step_delay);
    
    // 步骤2：设置为最小值
    if (!calibration_failed && !motor_set_pwm(channel, motor_config[channel].min_pwm)) {
        calibration_failed = true;
    }
    system_delay_ms(step_delay);
    
    // 步骤3：停止电机
    if (!calibration_failed && !motor_stop(channel)) {
        calibration_failed = true;
    }
    
    // 恢复之前的状态
    if (!was_enabled) {
        motor_disable(channel);
    } else if (prev_pwm > 0) {
        motor_set_pwm(channel, prev_pwm);
    }
    
    // 设置校准结果
    if (calibration_failed) {
        motor_status[channel].error = MOTOR_ERROR_CALIBRATION_FAILED;
        return false;
    }
    
    return true;
}

/**
 * @brief 更新电机状态（用于平滑控制和监控）
 */
void motor_update(void) {
    if (!motor_driver_initialized) {
        return;
    }
    
    uint32_t current_time = system_get_time_ms();
    
    // 更新每个电机
    for (uint8_t i = 0; i < MOTOR_CHANNEL_MAX; i++) {
        // 更新最后活动时间
        motor_status[i].last_update_time = current_time;
        
        // 处理平滑控制
        if (motor_smooth[i].is_active && motor_status[i].is_enabled) {
            uint32_t elapsed = current_time - motor_smooth[i].start_time;
            
            // 检查是否已完成过渡
            if (elapsed >= motor_smooth[i].transition_time) {
                // 完成平滑过渡
                motor_set_pwm(i, motor_smooth[i].target_pwm);
                motor_smooth[i].is_active = false;
            } else {
                // 计算当前PWM值（线性插值）
                float progress = (float)elapsed / (float)motor_smooth[i].transition_time;
                uint16_t current_pwm = motor_smooth[i].start_pwm + 
                                     (uint16_t)((float)(motor_smooth[i].target_pwm - motor_smooth[i].start_pwm) * progress);
                
                // 更新PWM，但不修改平滑控制状态
                if (current_pwm < motor_config[i].min_pwm && current_pwm > 0) {
                    current_pwm = 0;
                } else if (current_pwm > motor_config[i].max_pwm) {
                    current_pwm = motor_config[i].max_pwm;
                }
                
                // 直接设置PWM（不调用motor_set_pwm以避免干扰平滑控制）
                if (current_pwm > 0) {
                    float duty_cycle = (float)current_pwm / (float)motor_config[i].max_pwm;
                    hal_timer_set_pwm_duty_cycle(motor_config[i].pwm_timer_channel, duty_cycle);
                    motor_status[i].current_pwm = current_pwm;
                    motor_status[i].duty_cycle = duty_cycle;
                } else {
                    hal_timer_set_pwm_duty_cycle(motor_config[i].pwm_timer_channel, 0.0f);
                    motor_status[i].current_pwm = 0;
                    motor_status[i].duty_cycle = 0.0f;
                }
            }
        }
        
        // 检查电机错误状态
        motor_error_t error = motor_check_error(i);
        if (error != MOTOR_ERROR_NONE && error != motor_status[i].error) {
            motor_status[i].error = error;
            
            // 如果是严重错误，停止电机
            if (error == MOTOR_ERROR_OVERHEAT || error == MOTOR_ERROR_ESC_FAILURE) {
                motor_stop(i);
            }
        }
    }
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