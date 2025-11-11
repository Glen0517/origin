/*
 * PID控制器算法接口定义
 */

#ifndef PID_H
#define PID_H

#include "../include/types.h"
#include "../config/config.h"

// PID控制模式
typedef enum {
    PID_MODE_POSITION,  // 位置式PID
    PID_MODE_VELOCITY   // 速度式PID
} pid_mode_t;

// PID控制器配置结构体
typedef struct {
    pid_mode_t mode;          // PID模式
    float kp;                 // 比例系数
    float ki;                 // 积分系数
    float kd;                 // 微分系数
    float output_max;         // 输出最大值
    float output_min;         // 输出最小值
    float integrator_max;     // 积分限幅
    float differentiator_max; // 微分限幅
    bool anti_windup;         // 积分抗饱和
    float deadband;           // 死区范围
} pid_config_t;

// PID控制器状态结构体
typedef struct {
    float setpoint;           // 设定值
    float process_value;      // 实际值
    float error;              // 当前误差
    float prev_error;         // 上一次误差
    float prev_prev_error;    // 上上次误差（用于速度式PID）
    float integral;           // 积分项
    float derivative;         // 微分项
    float output;             // 输出值
    float kp;                 // 当前比例系数
    float ki;                 // 当前积分系数
    float kd;                 // 当前微分系数
    uint32_t last_update_time; // 上次更新时间
    uint32_t update_count;     // 更新次数
    bool is_enabled;          // 是否启用
} pid_state_t;

/**
 * @brief 初始化PID控制器
 * @param config PID配置结构体
 * @return 是否初始化成功
 */
bool pid_init(pid_config_t *config);

/**
 * @brief 设置PID参数
 * @param kp 比例系数
 * @param ki 积分系数
 * @param kd 微分系数
 */
void pid_set_params(float kp, float ki, float kd);

/**
 * @brief 设置PID输出限制
 * @param min 最小值
 * @param max 最大值
 */
void pid_set_output_limits(float min, float max);

/**
 * @brief 设置PID积分限制
 * @param max 最大值
 */
void pid_set_integrator_limit(float max);

/**
 * @brief 设置PID微分限制
 * @param max 最大值
 */
void pid_set_differentiator_limit(float max);

/**
 * @brief 设置PID抗饱和
 * @param enable 是否启用
 */
void pid_set_anti_windup(bool enable);

/**
 * @brief 设置PID死区
 * @param deadband 死区范围
 */
void pid_set_deadband(float deadband);

/**
 * @brief 更新PID控制器
 * @param setpoint 设定值
 * @param process_value 实际值
 * @param dt 时间间隔(秒)
 * @return 控制输出值
 */
float pid_update(float setpoint, float process_value, float dt);

/**
 * @brief 获取PID状态
 * @param state PID状态结构体
 * @return 是否获取成功
 */
bool pid_get_state(pid_state_t *state);

/**
 * @brief 重置PID控制器
 */
void pid_reset(void);

/**
 * @brief 启用PID控制器
 */
void pid_enable(void);

/**
 * @brief 禁用PID控制器
 */
void pid_disable(void);

/**
 * @brief 位置式PID计算
 * @param error 误差
 * @param dt 时间间隔(秒)
 * @return 控制输出值
 */
float pid_position_calculate(float error, float dt);

/**
 * @brief 速度式PID计算
 * @param error 误差
 * @param dt 时间间隔(秒)
 * @return 控制输出值
 */
float pid_velocity_calculate(float error, float dt);

#endif // PID_H