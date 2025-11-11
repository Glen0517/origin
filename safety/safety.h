/*
 * 安全模块接口定义
 */

#ifndef SAFETY_H
#define SAFETY_H

#include "../include/types.h"
#include "../config/config.h"
#include "safety_monitor.h"

// 故障类型枚举
typedef enum {
    FAILURE_TYPE_NONE,                // 无故障
    FAILURE_TYPE_MPU6050_ERROR,       // MPU6050错误
    FAILURE_TYPE_MOTOR_ERROR,         // 电机错误
    FAILURE_TYPE_BATTERY_LOW,         // 电池电量低
    FAILURE_TYPE_BATTERY_CRITICAL,    // 电池电量严重不足
    FAILURE_TYPE_RC_LOSS,             // 遥控器信号丢失
    FAILURE_TYPE_SENSOR_DRIFT,        // 传感器漂移
    FAILURE_TYPE_IMU_NOT_READY,       // IMU未准备好
    FAILURE_TYPE_CPU_OVERLOAD,        // CPU过载
    FAILURE_TYPE_MEMORY_ERROR,        // 内存错误
    FAILURE_TYPE_SYSTEM_ERROR         // 系统错误
} failure_type_t;

// 安全状态枚举
typedef enum {
    SAFETY_STATE_NORMAL,              // 正常状态
    SAFETY_STATE_WARNING,             // 警告状态
    SAFETY_STATE_CRITICAL,            // 临界状态
    SAFETY_STATE_EMERGENCY            // 紧急状态
} safety_state_t;

// 故障检测配置结构体
typedef struct {
    bool enable_mpu6050_check;        // 是否启用MPU6050检查
    bool enable_motor_check;          // 是否启用电机检查
    bool enable_battery_monitor;      // 是否启用电池监控
    bool enable_rc_loss_detection;    // 是否启用信号丢失检测
    bool enable_sensor_drift_check;   // 是否启用传感器漂移检查
    bool enable_cpu_monitor;          // 是否启用CPU监控
    bool enable_memory_monitor;       // 是否启用内存监控
    
    uint16_t battery_warning_level;   // 电池警告电压（mV）
    uint16_t battery_critical_level;  // 电池临界电压（mV）
    uint32_t rc_loss_timeout;         // 遥控信号丢失超时时间（ms）
    uint32_t imu_error_timeout;       // IMU错误超时时间（ms）
    uint16_t cpu_load_warning;        // CPU负载警告阈值（%）
} safety_config_t;

// 安全状态结构体
typedef struct {
    safety_state_t state;             // 当前安全状态
    failure_type_t active_failures[16]; // 活跃的故障列表
    uint8_t failure_count;            // 故障数量
    uint32_t state_timestamp;         // 状态更新时间戳
    uint32_t last_heartbeat;          // 最后心跳时间
    uint16_t battery_voltage;         // 当前电池电压（mV）
    uint8_t battery_level;            // 电池电量百分比
    uint16_t cpu_load;                // CPU负载（%）
    bool rc_connected;                // 遥控器是否连接
} safety_status_t;

/**
 * @brief 初始化安全模块
 * @param config 安全配置参数
 * @return 是否初始化成功
 */
bool safety_init(safety_config_t *config);

/**
 * @brief 更新安全状态
 */
void safety_update(void);

/**
 * @brief 获取当前安全状态
 * @param status 安全状态结构体
 * @return 是否获取成功
 */
bool safety_get_status(safety_status_t *status);

/**
 * @brief 触发紧急停止
 * @param reason 紧急停止原因
 */
void safety_emergency_stop(const char *reason);

/**
 * @brief 记录故障
 * @param type 故障类型
 * @param message 故障消息
 */
void safety_report_failure(failure_type_t type, const char *message);

/**
 * @brief 清除故障
 * @param type 故障类型
 */
void safety_clear_failure(failure_type_t type);

/**
 * @brief 检查系统是否安全
 * @return 系统是否安全
 */
bool safety_is_system_safe(void);

/**
 * @brief 检查是否可以解锁电机
 * @return 是否可以解锁
 */
bool safety_can_arm(void);

/**
 * @brief 设置电池电压
 * @param voltage 电池电压（mV）
 */
void safety_set_battery_voltage(uint16_t voltage);

/**
 * @brief 设置IMU角度数据（用于解锁检测）
 * @param roll 横滚角度（度）
 * @param pitch 俯仰角度（度）
 */
void safety_set_imu_angles(float roll, float pitch);

/**
 * @brief 设置遥控器油门值（用于解锁检测）
 * @param throttle 油门值（0.0-1.0）
 */
void safety_set_rc_throttle(float throttle);

/**
 * @brief 设置遥控器连接状态
 * @param connected 是否连接
 */
void safety_set_rc_connection(bool connected);

/**
 * @brief 设置CPU负载
 * @param load CPU负载（%）
 */
void safety_set_cpu_load(uint16_t load);

/**
 * @brief 检查传感器数据有效性
 * @param accel_x X轴加速度
 * @param accel_y Y轴加速度
 * @param accel_z Z轴加速度
 * @param gyro_x X轴角速度
 * @param gyro_y Y轴角速度
 * @param gyro_z Z轴角速度
 * @return 数据是否有效
 */
bool safety_check_sensor_data(float32_t accel_x, float32_t accel_y, float32_t accel_z,
                             float32_t gyro_x, float32_t gyro_y, float32_t gyro_z);

/**
 * @brief 执行故障恢复措施
 * @param type 故障类型
 */
void safety_perform_recovery(failure_type_t type);

/**
 * @brief 获取故障描述
 * @param type 故障类型
 * @return 故障描述字符串
 */
const char *safety_get_failure_description(failure_type_t type);

/**
 * @brief 心跳函数，定期调用以表明系统正常运行
 */
void safety_heartbeat(void);

#endif // SAFETY_H