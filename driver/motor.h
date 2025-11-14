/*
 * 电机驱动接口定义
 */

#ifndef MOTOR_H
#define MOTOR_H

#include "../include/types.h"
#include "../config/config.h"

// 电机通道定义
typedef enum {
    MOTOR_CHANNEL_1,
    MOTOR_CHANNEL_2,
    MOTOR_CHANNEL_3,
    MOTOR_CHANNEL_4,
    MOTOR_CHANNEL_MAX
} motor_channel_t;

// 电机配置结构体
typedef struct {
    motor_channel_t channel;           // 电机通道
    uint8_t pwm_timer_channel;         // PWM定时器通道
    uint32_t pwm_frequency;            // PWM频率
    uint16_t min_pwm;                  // 最小PWM值
    uint16_t max_pwm;                  // 最大PWM值
    uint16_t idle_pwm;                 // 空闲PWM值（电机启动阈值）
    bool direction;                    // 旋转方向 (true=正向, false=反向)
} motor_config_t;

// 电机故障类型
typedef enum {
    MOTOR_ERROR_NONE = 0,              // 无故障
    MOTOR_ERROR_OVERHEAT,              // 过热
    MOTOR_ERROR_ESC_FAILURE,           // ESC故障
    MOTOR_ERROR_SPEED_MISMATCH,        // 速度不匹配
    MOTOR_ERROR_PWM_OUT_OF_RANGE,      // PWM超出范围
    MOTOR_ERROR_CALIBRATION_FAILED     // 校准失败
} motor_error_t;

// 电机状态结构体
typedef struct {
    motor_channel_t channel;           // 电机通道
    uint16_t current_pwm;              // 当前PWM值
    bool is_enabled;                   // 是否启用
    float duty_cycle;                  // 占空比 (0.0f - 1.0f)
    uint16_t temperature;              // 温度 (摄氏度)
    motor_error_t error;               // 错误状态
    bool direction_inverted;           // 方向是否反转
    uint32_t last_update_time;         // 最后更新时间
} motor_status_t;

/**
 * @brief 初始化电机驱动
 * @param config 电机配置结构体
 * @return 初始化是否成功
 */
bool motor_init(motor_config_t *config);

/**
 * @brief 初始化所有电机
 * @return 是否全部初始化成功
 */
bool motor_init_all(void);

/**
 * @brief 设置电机PWM值
 * @param channel 电机通道
 * @param pwm PWM值
 * @return 设置是否成功
 */
bool motor_set_pwm(motor_channel_t channel, uint16_t pwm);

/**
 * @brief 设置电机占空比
 * @param channel 电机通道
 * @param duty_cycle 占空比 (0.0f - 1.0f)
 * @return 设置是否成功
 */
bool motor_set_duty_cycle(motor_channel_t channel, float duty_cycle);

/**
 * @brief 设置所有电机PWM值
 * @param pwm_values 4个电机的PWM值数组
 * @return 设置是否成功
 */
bool motor_set_all_pwm(uint16_t pwm_values[MOTOR_CHANNEL_MAX]);

/**
 * @brief 停止电机
 * @param channel 电机通道
 * @return 停止是否成功
 */
bool motor_stop(motor_channel_t channel);

/**
 * @brief 停止所有电机
 * @return 是否全部停止成功
 */
bool motor_stop_all(void);

/**
 * @brief 启用电机
 * @param channel 电机通道
 * @return 启用是否成功
 */
bool motor_enable(motor_channel_t channel);

/**
 * @brief 禁用电机
 * @param channel 电机通道
 * @return 禁用是否成功
 */
bool motor_disable(motor_channel_t channel);

/**
 * @brief 设置电机温度
 * @param channel 电机通道
 * @param temperature 温度值（摄氏度）
 * @return 设置是否成功
 */
bool motor_set_temperature(motor_channel_t channel, uint16_t temperature);

/**
 * @brief 反转电机方向
 * @param channel 电机通道
 * @param invert 是否反转
 * @return 设置是否成功
 */
bool motor_invert_direction(motor_channel_t channel, bool invert);

/**
 * @brief 平滑设置电机PWM
 * @param channel 电机通道
 * @param target_pwm 目标PWM值
 * @param transition_time 过渡时间（毫秒）
 * @return 设置是否成功
 */
bool motor_set_pwm_smooth(motor_channel_t channel, uint16_t target_pwm, uint32_t transition_time);

/**
 * @brief 检查电机故障
 * @param channel 电机通道
 * @return 故障状态
 */
motor_error_t motor_check_error(motor_channel_t channel);

/**
 * @brief 清除电机错误
 * @param channel 电机通道
 * @return 操作是否成功
 */
bool motor_clear_error(motor_channel_t channel);

/**
 * @brief 电机校准
 * @param channel 电机通道
 * @return 校准是否成功
 */
bool motor_calibrate(motor_channel_t channel);

/**
 * @brief 更新电机状态（用于平滑控制和监控）
 */
void motor_update(void);

/**
 * @brief 获取电机状态
 * @param channel 电机通道
 * @param status 电机状态结构体
 * @return 获取是否成功
 */
bool motor_get_status(motor_channel_t channel, motor_status_t *status);

/**
 * @brief 获取所有电机状态
 * @param status 电机状态结构体数组
 * @return 获取是否成功
 */
bool motor_get_all_status(motor_status_t status[MOTOR_CHANNEL_MAX]);

/**
 * @brief 测试电机功能
 * @param channel 电机通道
 * @param test_duration 测试持续时间(ms)
 * @return 测试是否成功
 */
bool motor_test(motor_channel_t channel, uint32_t test_duration);

#endif // MOTOR_H