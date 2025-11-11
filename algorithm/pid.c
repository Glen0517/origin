/*
 * PID控制器算法实现
 */

#include "pid.h"
#include "../service/system.h"
#include "../config/config.h"
#include <math.h>

// 静态变量
static pid_config_t pid_config;
static pid_state_t pid_state;
static bool pid_initialized = false;

/**
 * @brief 初始化PID控制器
 */
bool pid_init(pid_config_t *config) {
    if (config == NULL) {
        return false;
    }
    
    // 保存配置
    pid_config = *config;
    
    // 初始化状态
    pid_state.setpoint = 0.0f;
    pid_state.process_value = 0.0f;
    pid_state.error = 0.0f;
    pid_state.prev_error = 0.0f;
    pid_state.prev_prev_error = 0.0f;
    pid_state.integral = 0.0f;
    pid_state.derivative = 0.0f;
    pid_state.output = 0.0f;
    pid_state.kp = config->kp;
    pid_state.ki = config->ki;
    pid_state.kd = config->kd;
    pid_state.last_update_time = system_get_time_ms();
    pid_state.update_count = 0;
    pid_state.is_enabled = true;
    
    pid_initialized = true;
    return true;
}

/**
 * @brief 设置PID参数
 */
void pid_set_params(float kp, float ki, float kd) {
    if (!pid_initialized) {
        return;
    }
    
    pid_config.kp = kp;
    pid_config.ki = ki;
    pid_config.kd = kd;
    
    pid_state.kp = kp;
    pid_state.ki = ki;
    pid_state.kd = kd;
}

/**
 * @brief 设置PID输出限制
 */
void pid_set_output_limits(float min, float max) {
    if (!pid_initialized || min > max) {
        return;
    }
    
    pid_config.output_min = min;
    pid_config.output_max = max;
}

/**
 * @brief 设置PID积分限制
 */
void pid_set_integrator_limit(float max) {
    if (!pid_initialized || max < 0.0f) {
        return;
    }
    
    pid_config.integrator_max = max;
    
    // 立即应用限制到当前积分值
    if (pid_state.integral > max) {
        pid_state.integral = max;
    } else if (pid_state.integral < -max) {
        pid_state.integral = -max;
    }
}

/**
 * @brief 设置PID微分限制
 */
void pid_set_differentiator_limit(float max) {
    if (!pid_initialized || max < 0.0f) {
        return;
    }
    
    pid_config.differentiator_max = max;
    
    // 立即应用限制到当前微分值
    if (pid_state.derivative > max) {
        pid_state.derivative = max;
    } else if (pid_state.derivative < -max) {
        pid_state.derivative = -max;
    }
}

/**
 * @brief 设置PID抗饱和
 */
void pid_set_anti_windup(bool enable) {
    if (!pid_initialized) {
        return;
    }
    
    pid_config.anti_windup = enable;
}

/**
 * @brief 设置PID死区
 */
void pid_set_deadband(float deadband) {
    if (!pid_initialized || deadband < 0.0f) {
        return;
    }
    
    pid_config.deadband = deadband;
}

/**
 * @brief 更新PID控制器
 */
float pid_update(float setpoint, float process_value, float dt) {
    if (!pid_initialized || !pid_state.is_enabled || dt <= 0.0f) {
        return 0.0f;
    }
    
    // 更新状态
    pid_state.setpoint = setpoint;
    pid_state.process_value = process_value;
    
    // 计算误差
    pid_state.error = setpoint - process_value;
    
    // 死区处理
    if (fabs(pid_state.error) <= pid_config.deadband) {
        pid_state.error = 0.0f;
    }
    
    // 根据模式选择计算方法
    float output;
    switch (pid_config.mode) {
        case PID_MODE_POSITION:
            output = pid_position_calculate(pid_state.error, dt);
            break;
        case PID_MODE_VELOCITY:
            output = pid_velocity_calculate(pid_state.error, dt);
            break;
        default:
            return 0.0f;
    }
    
    // 输出限幅
    if (output > pid_config.output_max) {
        output = pid_config.output_max;
    } else if (output < pid_config.output_min) {
        output = pid_config.output_min;
    }
    
    // 更新状态
    pid_state.output = output;
    pid_state.prev_prev_error = pid_state.prev_error;
    pid_state.prev_error = pid_state.error;
    pid_state.last_update_time = system_get_time_ms();
    pid_state.update_count++;
    
    return output;
}

/**
 * @brief 获取PID状态
 */
bool pid_get_state(pid_state_t *state) {
    if (!pid_initialized || state == NULL) {
        return false;
    }
    
    *state = pid_state;
    return true;
}

/**
 * @brief 重置PID控制器
 */
void pid_reset(void) {
    if (!pid_initialized) {
        return;
    }
    
    pid_state.setpoint = 0.0f;
    pid_state.process_value = 0.0f;
    pid_state.error = 0.0f;
    pid_state.prev_error = 0.0f;
    pid_state.prev_prev_error = 0.0f;
    pid_state.integral = 0.0f;
    pid_state.derivative = 0.0f;
    pid_state.output = 0.0f;
    pid_state.update_count = 0;
}

/**
 * @brief 启用PID控制器
 */
void pid_enable(void) {
    if (!pid_initialized) {
        return;
    }
    
    pid_state.is_enabled = true;
    // 启用时重置积分项，避免积分饱和
    pid_state.integral = 0.0f;
}

/**
 * @brief 禁用PID控制器
 */
void pid_disable(void) {
    if (!pid_initialized) {
        return;
    }
    
    pid_state.is_enabled = false;
    pid_state.output = 0.0f;
}

/**
 * @brief 位置式PID计算
 */
float pid_position_calculate(float error, float dt) {
    // 计算比例项
    float proportional = pid_state.kp * error;
    
    // 计算积分项
    pid_state.integral += pid_state.ki * error * dt;
    
    // 积分限幅
    if (pid_state.integral > pid_config.integrator_max) {
        pid_state.integral = pid_config.integrator_max;
    } else if (pid_state.integral < -pid_config.integrator_max) {
        pid_state.integral = -pid_config.integrator_max;
    }
    
    // 计算微分项
    if (dt > 0.0f) {
        pid_state.derivative = pid_state.kd * (error - pid_state.prev_error) / dt;
    } else {
        pid_state.derivative = 0.0f;
    }
    
    // 微分限幅
    if (pid_state.derivative > pid_config.differentiator_max) {
        pid_state.derivative = pid_config.differentiator_max;
    } else if (pid_state.derivative < -pid_config.differentiator_max) {
        pid_state.derivative = -pid_config.differentiator_max;
    }
    
    // 计算输出
    float output = proportional + pid_state.integral + pid_state.derivative;
    
    // 抗饱和处理
    if (pid_config.anti_windup) {
        // 如果输出超出限制，则停止积分积累
        if ((output > pid_config.output_max && error > 0.0f) ||
            (output < pid_config.output_min && error < 0.0f)) {
            // 停止积分积累，但保持当前积分值
            pid_state.integral -= pid_state.ki * error * dt;
        }
    }
    
    return output;
}

/**
 * @brief 速度式PID计算
 */
float pid_velocity_calculate(float error, float dt) {
    // 计算比例项变化
    float proportional = pid_state.kp * (error - pid_state.prev_error);
    
    // 计算积分项
    float integral = pid_state.ki * error * dt;
    
    // 计算微分项 (使用差分公式，减少噪声影响)
    float derivative = pid_state.kd * ((error - pid_state.prev_error) - 
                                      (pid_state.prev_error - pid_state.prev_prev_error)) / dt;
    
    // 微分限幅
    if (derivative > pid_config.differentiator_max) {
        derivative = pid_config.differentiator_max;
    } else if (derivative < -pid_config.differentiator_max) {
        derivative = -pid_config.differentiator_max;
    }
    
    // 计算输出增量
    float output_increment = proportional + integral + derivative;
    
    // 计算新的输出值
    float new_output = pid_state.output + output_increment;
    
    // 输出限幅
    if (new_output > pid_config.output_max) {
        new_output = pid_config.output_max;
    } else if (new_output < pid_config.output_min) {
        new_output = pid_config.output_min;
    }
    
    return new_output;
}