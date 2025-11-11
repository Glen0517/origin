/*
 * 解锁检测模块头文件
 * 用于检测和验证无人机的解锁条件
 */

#ifndef ARMING_DETECTOR_H
#define ARMING_DETECTOR_H

#include "../include/types.h"

// 解锁检测配置结构体
typedef struct {
    float throttle_threshold;      // 油门最低阈值
    float roll_angle_threshold;    // 横滚角度阈值（度）
    float pitch_angle_threshold;   // 俯仰角度阈值（度）
    uint32_t arm_delay_ms;         // 解锁延迟（毫秒）
    uint32_t arm_check_interval_ms; // 解锁条件检查间隔（毫秒）
} arming_detector_config_t;

// 解锁状态枚举
typedef enum {
    ARMING_STATE_DISARMED,         // 未解锁
    ARMING_STATE_READY_TO_ARM,     // 准备解锁
    ARMING_STATE_ARMING,           // 解锁中
    ARMING_STATE_ARMED,            // 已解锁
    ARMING_STATE_DISARMING         // 正在解锁
} arming_state_t;

// 解锁检测状态结构体
typedef struct {
    arming_state_t state;          // 当前解锁状态
    bool conditions_met;           // 解锁条件是否满足
    uint32_t condition_check_time; // 条件检查时间戳
    uint32_t arm_request_time;     // 解锁请求时间
    bool safety_switch_state;      // 安全开关状态
    uint32_t last_arm_command_time; // 最后解锁命令时间
} arming_detector_status_t;

/**
 * @brief 初始化解锁检测模块
 * @param config 配置参数
 * @return 初始化结果
 */
bool arming_detector_init(arming_detector_config_t *config);

/**
 * @brief 更新解锁检测状态
 * @param throttle 油门值
 * @param roll 横滚角度（度）
 * @param pitch 俯仰角度（度）
 * @param imu_ready IMU是否就绪
 * @return 更新后的解锁状态
 */
arming_state_t arming_detector_update(float throttle, float roll, float pitch, bool imu_ready);

/**
 * @brief 请求解锁
 * @return 是否可以执行解锁
 */
bool arming_detector_request_arm(void);

/**
 * @brief 请求锁定
 * @return 是否可以执行锁定
 */
bool arming_detector_request_disarm(void);

/**
 * @brief 获取解锁状态
 * @param status 状态指针
 * @return 是否获取成功
 */
bool arming_detector_get_status(arming_detector_status_t *status);

/**
 * @brief 设置安全开关状态
 * @param state 开关状态
 */
void arming_detector_set_safety_switch(bool state);

/**
 * @brief 强制锁定
 * @return 是否锁定成功
 */
bool arming_detector_force_disarm(void);

/**
 * @brief 检查是否可以解锁
 * @return 是否可以解锁
 */
bool arming_detector_can_arm(void);

#endif /* ARMING_DETECTOR_H */