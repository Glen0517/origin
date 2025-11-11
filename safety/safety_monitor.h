/*
 * 安全监控器模块头文件
 * 用于持续监控系统关键参数，提供预警和保护功能
 */

#ifndef SAFETY_MONITOR_H
#define SAFETY_MONITOR_H

#include "../include/types.h"
#include "arming_detector.h"

// 监控参数类型枚举
typedef enum {
    MONITOR_PARAM_BATTERY,
    MONITOR_PARAM_RC,
    MONITOR_PARAM_IMU,
    MONITOR_PARAM_MOTOR,
    MONITOR_PARAM_CPU,
    MONITOR_PARAM_MEMORY,
    MONITOR_PARAM_FLIGHT_TIME,
    MONITOR_PARAM_MOTOR_TEMPERATURE,
    MONITOR_PARAM_SYSTEM_TEMPERATURE,
    MONITOR_PARAM_GPS
} monitor_param_type_t;

// 监控阈值配置结构体
typedef struct {
    // 电池监控阈值
    uint16_t min_battery_voltage;    // 最低电池电压（mV）
    uint16_t warning_battery_voltage; // 警告电池电压（mV）
    
    // RC信号监控阈值
    uint32_t rc_loss_timeout_ms;     // 遥控信号丢失超时（ms）
    float min_throttle_value;        // 最小油门值
    float max_throttle_value;        // 最大油门值
    
    // IMU监控阈值
    float max_imu_accel_value;       // 最大加速度值
    float max_imu_gyro_value;        // 最大角速度值
    uint32_t imu_error_timeout_ms;   // IMU错误超时（ms）
    
    // CPU监控阈值
    uint16_t max_cpu_load;           // 最大CPU负载（%）
    uint32_t min_loop_time_us;       // 最小循环时间（μs）
    
    // 飞行时间监控
    uint32_t max_flight_time_ms;     // 最大飞行时间（ms）
    
    // 温度监控
    int16_t max_motor_temperature;   // 最大电机温度（℃）
    int16_t max_system_temperature;  // 最大系统温度（℃）
} monitor_thresholds_t;

// 监控器配置结构体
typedef struct {
    bool enable_battery_monitor;     // 是否启用电池监控
    bool enable_rc_monitor;          // 是否启用RC监控
    bool enable_imu_monitor;         // 是否启用IMU监控
    bool enable_motor_monitor;       // 是否启用电机监控
    bool enable_cpu_monitor;         // 是否启用CPU监控
    bool enable_memory_monitor;      // 是否启用内存监控
    bool enable_flight_time_monitor; // 是否启用飞行时间监控
    bool enable_temperature_monitor; // 是否启用温度监控
    bool enable_gps_monitor;         // 是否启用GPS监控
    
    monitor_thresholds_t thresholds; // 监控阈值
    uint32_t monitor_interval_ms;    // 监控间隔（ms）
} safety_monitor_config_t;

// 监控器状态结构体
typedef struct {
    bool is_monitoring;              // 是否正在监控
    uint32_t last_monitor_time;      // 上次监控时间戳
    uint32_t flight_time_ms;         // 当前飞行时间
    
    // 参数历史记录（用于趋势分析）
    float battery_voltage_history[10]; // 电池电压历史
    float cpu_load_history[10];      // CPU负载历史
    int16_t temperature_history[10]; // 温度历史
    
    // 异常计数
    uint16_t battery_warning_count;  // 电池警告计数
    uint16_t rc_warning_count;       // RC警告计数
    uint16_t imu_warning_count;      // IMU警告计数
    uint16_t motor_warning_count;    // 电机警告计数
    
    bool in_soft_landing;            // 是否在软着陆过程中
} safety_monitor_status_t;

/**
 * @brief 初始化安全监控器
 * @param config 监控器配置
 * @return 是否初始化成功
 */
bool safety_monitor_init(safety_monitor_config_t *config);

/**
 * @brief 启动监控器
 */
void safety_monitor_start(void);

/**
 * @brief 停止监控器
 */
void safety_monitor_stop(void);

/**
 * @brief 更新监控器状态
 */
void safety_monitor_update(void);

/**
 * @brief 设置监控参数
 * @param param_type 参数类型
 * @param value 参数值
 */
void safety_monitor_set_param(monitor_param_type_t param_type, float value);

/**
 * @brief 获取监控器状态
 * @param status 状态指针
 * @return 是否获取成功
 */
bool safety_monitor_get_status(safety_monitor_status_t *status);

/**
 * @brief 触发软着陆
 * @return 是否成功触发
 */
bool safety_monitor_trigger_soft_landing(void);

/**
 * @brief 检查参数是否在安全范围内
 * @param param_type 参数类型
 * @param value 参数值
 * @return 是否安全
 */
bool safety_monitor_check_param(monitor_param_type_t param_type, float value);

/**
 * @brief 重置飞行时间
 */
void safety_monitor_reset_flight_time(void);

/**
 * @brief 获取参数异常计数
 * @param param_type 参数类型
 * @return 异常计数
 */
uint16_t safety_monitor_get_warning_count(monitor_param_type_t param_type);

#endif /* SAFETY_MONITOR_H */