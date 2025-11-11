/*
 * 飞行控制业务逻辑接口定义
 */

#ifndef FLIGHT_CONTROL_H
#define FLIGHT_CONTROL_H

#include "../include/types.h"
#include "../config/config.h"
#include "../algorithm/attitude.h"
#include "../algorithm/pid.h"

// 飞行控制状态
typedef enum {
    FLIGHT_STATE_DISARMED,      // 未解锁
    FLIGHT_STATE_ARMED,         // 已解锁
    FLIGHT_STATE_READY,         // 准备起飞
    FLIGHT_STATE_FLYING,        // 飞行中
    FLIGHT_STATE_LANDING,       // 着陆中
    FLIGHT_STATE_EMERGENCY      // 紧急状态
} flight_state_t;

// 飞行控制配置结构体
typedef struct {
    bool use_gyro_calibration;  // 是否使用陀螺仪校准
    bool use_accel_calibration; // 是否使用加速度计校准
    bool use_magnetometer;      // 是否使用磁力计
    float min_throttle;         // 最小油门值
    float max_throttle;         // 最大油门值
    float hover_throttle;       // 悬停油门值
    float yaw_boost;            // 偏航增益
    bool stabilization_enabled; // 是否启用姿态稳定
} flight_control_config_t;

// 飞行控制状态结构体
typedef struct {
    flight_state_t state;               // 飞行状态
    flight_mode_t mode;                 // 飞行模式
    rc_data_t rc_input;                 // 遥控器输入
    control_channels_t control_output;  // 控制输出
    motor_output_t motor_output;        // 电机输出
    attitude_state_t attitude;          // 姿态状态
    bool is_calibrating;                // 是否正在校准
    uint32_t arm_time;                  // 解锁时间
    uint32_t flight_time;               // 飞行时间
    bool failsafe_triggered;            // 失控保护是否触发
} flight_control_status_t;

// PID控制器结构体
typedef struct {
    pid_controller_t roll_pid;          // 横滚PID
    pid_controller_t pitch_pid;         // 俯仰PID
    pid_controller_t yaw_pid;           // 偏航PID
    pid_controller_t throttle_pid;      // 油门PID
    pid_controller_t altitude_pid;      // 高度PID
    pid_controller_t pos_x_pid;         // X位置PID
    pid_controller_t pos_y_pid;         // Y位置PID
} pid_controllers_t;

/**
 * @brief 初始化飞行控制系统
 * @param config 飞行控制配置
 * @return 是否初始化成功
 */
bool flight_control_init(flight_control_config_t *config);

/**
 * @brief 飞行控制主循环
 * @param dt 时间间隔(秒)
 */
void flight_control_loop(float dt);

/**
 * @brief 获取飞行控制状态
 * @param status 飞行控制状态结构体
 * @return 是否获取成功
 */
bool flight_control_get_status(flight_control_status_t *status);

/**
 * @brief 设置飞行模式
 * @param mode 飞行模式
 * @return 是否设置成功
 */
bool flight_control_set_mode(flight_mode_t mode);

/**
 * @brief 解锁电机
 * @return 是否解锁成功
 */
bool flight_control_arm(void);

/**
 * @brief 锁定电机
 * @return 是否锁定成功
 */
bool flight_control_disarm(void);

/**
 * @brief 处理遥控器输入
 * @param rc_data 遥控器数据
 */
void flight_control_process_rc_input(rc_data_t *rc_data);

/**
 * @brief 紧急停止
 */
void flight_control_emergency_stop(void);

/**
 * @brief 执行姿态校准
 * @param samples 采样次数
 * @return 是否校准成功
 */
bool flight_control_calibrate_attitude(uint16_t samples);

/**
 * @brief 执行IMU校准
 * @param type 校准类型
 * @param samples 采样次数
 * @return 是否校准成功
 */
bool flight_control_calibrate_imu(imu_calib_type_t type, uint16_t samples);

/**
 * @brief 设置PID参数
 * @param channel 控制通道
 * @param kp 比例系数
 * @param ki 积分系数
 * @param kd 微分系数
 */
void flight_control_set_pid_params(control_channel_t channel, float kp, float ki, float kd);

/**
 * @brief 获取PID参数
 * @param channel 控制通道
 * @param params PID参数结构体
 * @return 是否获取成功
 */
bool flight_control_get_pid_params(control_channel_t channel, pid_params_t *params);

/**
 * @brief 执行失控保护
 */
void flight_control_failsafe(void);

/**
 * @brief 设置失控保护状态
 * @param enabled 是否启用
 */
void flight_control_set_failsafe(bool enabled);

/**
 * @brief 获取电机输出
 * @param output 电机输出结构体
 * @return 是否获取成功
 */
bool flight_control_get_motor_output(motor_output_t *output);

#endif // FLIGHT_CONTROL_H