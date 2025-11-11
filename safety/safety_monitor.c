/*
 * 安全监控器模块实现
 */

#include "safety_monitor.h"
#include "../service/system.h"
#include "../config/config.h"
#include "../driver/motor.h"
#include "../driver/mpu6050.h"
#include "../communication/communication.h"
#include "arming_detector.h"
#include "safety.h"
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

// 静态变量
static safety_monitor_config_t monitor_config;
static safety_monitor_status_t monitor_status;
static bool monitor_initialized = false;

// 默认配置
static const safety_monitor_config_t DEFAULT_CONFIG = {
    .enable_battery_monitor = true,
    .enable_rc_monitor = true,
    .enable_imu_monitor = true,
    .enable_motor_monitor = true,
    .enable_cpu_monitor = true,
    .enable_memory_monitor = false,
    .enable_flight_time_monitor = true,
    .enable_temperature_monitor = false,
    .enable_gps_monitor = false,
    .thresholds = {
        .min_battery_voltage = 3300,
        .warning_battery_voltage = 3500,
        .rc_loss_timeout_ms = 2000,
        .min_throttle_value = 0.0f,
        .max_throttle_value = 1.0f,
        .max_imu_accel_value = 2.0f,
        .max_imu_gyro_value = 5.0f,
        .imu_error_timeout_ms = 100,
        .max_cpu_load = 80,
        .min_loop_time_us = 500,
        .max_flight_time_ms = 1800000,  // 30分钟
        .max_motor_temperature = 80,
        .max_system_temperature = 60
    },
    .monitor_interval_ms = 100
};

/**
 * @brief 初始化安全监控器
 */
bool safety_monitor_init(safety_monitor_config_t *config) {
    // 使用默认配置或用户配置
    if (config != NULL) {
        monitor_config = *config;
    } else {
        monitor_config = DEFAULT_CONFIG;
    }
    
    // 初始化状态
    monitor_status.is_monitoring = false;
    monitor_status.last_monitor_time = 0;
    monitor_status.flight_time_ms = 0;
    monitor_status.in_soft_landing = false;
    
    // 初始化历史数据
    for (int i = 0; i < 10; i++) {
        monitor_status.battery_voltage_history[i] = 0.0f;
        monitor_status.cpu_load_history[i] = 0.0f;
        monitor_status.temperature_history[i] = 0;
    }
    
    // 初始化警告计数
    monitor_status.battery_warning_count = 0;
    monitor_status.rc_warning_count = 0;
    monitor_status.imu_warning_count = 0;
    monitor_status.motor_warning_count = 0;
    
    monitor_initialized = true;
    system_log(LOG_LEVEL_INFO, "安全监控器初始化成功\n");
    return true;
}

/**
 * @brief 启动监控器
 */
void safety_monitor_start(void) {
    if (!monitor_initialized) {
        return;
    }
    
    monitor_status.is_monitoring = true;
    monitor_status.last_monitor_time = system_get_time_ms();
    system_log(LOG_LEVEL_INFO, "安全监控器已启动\n");
}

/**
 * @brief 停止监控器
 */
void safety_monitor_stop(void) {
    if (!monitor_initialized) {
        return;
    }
    
    monitor_status.is_monitoring = false;
    system_log(LOG_LEVEL_INFO, "安全监控器已停止\n");
}

/**
 * @brief 更新监控器状态
 */
void safety_monitor_update(void) {
    if (!monitor_initialized || !monitor_status.is_monitoring) {
        return;
    }
    
    uint32_t current_time = system_get_time_ms();
    
    // 检查是否到了监控时间间隔
    if (current_time - monitor_status.last_monitor_time < monitor_config.monitor_interval_ms) {
        return;
    }
    
    monitor_status.last_monitor_time = current_time;
    
    // 更新飞行时间
    if (monitor_config.enable_flight_time_monitor) {
        arming_detector_status_t arm_status;
        if (arming_detector_get_status(&arm_status) && arm_status.state == ARMING_STATE_ARMED) {
            monitor_status.flight_time_ms += monitor_config.monitor_interval_ms;
            
            // 检查飞行时间是否超限
            if (monitor_status.flight_time_ms > monitor_config.thresholds.max_flight_time_ms) {
                system_log(LOG_LEVEL_WARNING, "飞行时间超限，触发软着陆\n");
                safety_monitor_trigger_soft_landing();
            }
        }
    }
    
    // 检查电池状态
    if (monitor_config.enable_battery_monitor) {
        // 从安全模块获取电池电压
        safety_status_t safety_status;
        if (safety_get_status(&safety_status) && safety_status.battery_voltage > 0) {
            if (safety_status.battery_voltage < monitor_config.thresholds.min_battery_voltage) {
                safety_report_failure(FAILURE_TYPE_BATTERY_CRITICAL, "电池电压严重不足");
                monitor_status.battery_warning_count++;
                
                // 严重低电量时触发软着陆
                if (monitor_status.battery_warning_count > 5) {
                    safety_monitor_trigger_soft_landing();
                }
            } else if (safety_status.battery_voltage < monitor_config.thresholds.warning_battery_voltage) {
                safety_report_failure(FAILURE_TYPE_BATTERY_LOW, "电池电量低");
                monitor_status.battery_warning_count++;
            } else {
                // 电池电压正常，清除警告
                safety_clear_failure(FAILURE_TYPE_BATTERY_LOW);
                safety_clear_failure(FAILURE_TYPE_BATTERY_CRITICAL);
                monitor_status.battery_warning_count = 0;
            }
            
            // 更新电压历史记录
            for (int i = 9; i > 0; i--) {
                monitor_status.battery_voltage_history[i] = monitor_status.battery_voltage_history[i-1];
            }
            monitor_status.battery_voltage_history[0] = safety_status.battery_voltage;
        }
    }
    
    // 检查CPU负载
    if (monitor_config.enable_cpu_monitor) {
        safety_status_t safety_status;
        if (safety_get_status(&safety_status)) {
            if (safety_status.cpu_load > monitor_config.thresholds.max_cpu_load) {
                safety_report_failure(FAILURE_TYPE_CPU_OVERLOAD, "CPU负载过高");
                
                // 更新CPU负载历史记录
                for (int i = 9; i > 0; i--) {
                    monitor_status.cpu_load_history[i] = monitor_status.cpu_load_history[i-1];
                }
                monitor_status.cpu_load_history[0] = safety_status.cpu_load;
            } else {
                safety_clear_failure(FAILURE_TYPE_CPU_OVERLOAD);
            }
        }
    }
    
    // 检查软着陆状态
    if (monitor_status.in_soft_landing) {
        // 软着陆过程中持续降低电机输出
        // 这里需要与飞行控制模块协作
    }
}

/**
 * @brief 设置监控参数
 */
void safety_monitor_set_param(monitor_param_type_t param_type, float value) {
    if (!monitor_initialized) {
        return;
    }
    
    // 根据参数类型设置不同的值
    switch (param_type) {
        case MONITOR_PARAM_BATTERY:
            if (monitor_config.enable_battery_monitor) {
                // 可以在这里添加电池电压的额外处理
            }
            break;
            
        case MONITOR_PARAM_RC:
            if (monitor_config.enable_rc_monitor) {
                // 检查油门值是否在安全范围内
                if (value < monitor_config.thresholds.min_throttle_value || \
                    value > monitor_config.thresholds.max_throttle_value) {
                    safety_report_failure(FAILURE_TYPE_RC_LOSS, "油门值异常");
                    monitor_status.rc_warning_count++;
                } else {
                    safety_clear_failure(FAILURE_TYPE_RC_LOSS);
                    monitor_status.rc_warning_count = 0;
                }
            }
            break;
            
        case MONITOR_PARAM_IMU:
            if (monitor_config.enable_imu_monitor) {
                // 检查IMU值是否在安全范围内
                if (fabs(value) > monitor_config.thresholds.max_imu_accel_value || \
                    fabs(value) > monitor_config.thresholds.max_imu_gyro_value) {
                    safety_report_failure(FAILURE_TYPE_SENSOR_DRIFT, "IMU数据异常");
                    monitor_status.imu_warning_count++;
                } else {
                    safety_clear_failure(FAILURE_TYPE_SENSOR_DRIFT);
                    monitor_status.imu_warning_count = 0;
                }
            }
            break;
            
        case MONITOR_PARAM_CPU:
            if (monitor_config.enable_cpu_monitor) {
                // 记录CPU负载
            }
            break;
            
        // 其他参数类型的处理...
        
        default:
            break;
    }
}

/**
 * @brief 获取监控器状态
 */
bool safety_monitor_get_status(safety_monitor_status_t *status) {
    if (!monitor_initialized || status == NULL) {
        return false;
    }
    
    *status = monitor_status;
    return true;
}

/**
 * @brief 触发软着陆
 */
bool safety_monitor_trigger_soft_landing(void) {
    if (!monitor_initialized || monitor_status.in_soft_landing) {
        return false;
    }
    
    monitor_status.in_soft_landing = true;
    system_log(LOG_LEVEL_WARNING, "触发软着陆程序\n");
    
    // 这里应该发送软着陆命令到飞行控制模块
    // 实际的软着陆实现需要飞行控制模块配合
    
    // 通知通信模块发送软着陆警告
    communication_send_log(0, LOG_LEVEL_WARNING, "软着陆模式已激活\n");
    
    return true;
}

/**
 * @brief 检查参数是否在安全范围内
 */
bool safety_monitor_check_param(monitor_param_type_t param_type, float value) {
    if (!monitor_initialized) {
        return true;  // 未初始化时返回安全
    }
    
    switch (param_type) {
        case MONITOR_PARAM_BATTERY:
            return value >= monitor_config.thresholds.min_battery_voltage;
            
        case MONITOR_PARAM_RC:
            return value >= monitor_config.thresholds.min_throttle_value && \
                   value <= monitor_config.thresholds.max_throttle_value;
            
        case MONITOR_PARAM_IMU:
            return fabs(value) <= monitor_config.thresholds.max_imu_accel_value && \
                   fabs(value) <= monitor_config.thresholds.max_imu_gyro_value;
            
        case MONITOR_PARAM_CPU:
            return value <= monitor_config.thresholds.max_cpu_load;
            
        // 其他参数类型的检查...
        
        default:
            return true;
    }
}

/**
 * @brief 重置飞行时间
 */
void safety_monitor_reset_flight_time(void) {
    if (!monitor_initialized) {
        return;
    }
    
    monitor_status.flight_time_ms = 0;
}

/**
 * @brief 获取参数异常计数
 */
uint16_t safety_monitor_get_warning_count(monitor_param_type_t param_type) {
    if (!monitor_initialized) {
        return 0;
    }
    
    switch (param_type) {
        case MONITOR_PARAM_BATTERY:
            return monitor_status.battery_warning_count;
            
        case MONITOR_PARAM_RC:
            return monitor_status.rc_warning_count;
            
        case MONITOR_PARAM_IMU:
            return monitor_status.imu_warning_count;
            
        case MONITOR_PARAM_MOTOR:
            return monitor_status.motor_warning_count;
            
        default:
            return 0;
    }
}