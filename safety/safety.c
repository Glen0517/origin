/*
 * 安全模块实现
 */

#include "safety.h"
#include "arming_detector.h"
#include "../include/types.h"
#include "../config/config.h"
#include "../service/system.h"
#include "../driver/mpu6050.h"
#include "../driver/motor.h"
#include "../communication/communication.h"
#include <math.h>
#include <stdint.h>

// 定义ABS宏
#define ABS(x) ((x) > 0 ? (x) : -(x))
#include <stdint.h> /* 确保float32_t类型可用 */

// 安全配置和状态
static safety_config_t safety_config;
static safety_status_t safety_status;
static bool safety_initialized = false;

// 外部IMU数据用于解锁检测
static float imu_roll = 0.0f;
static float imu_pitch = 0.0f;
static float rc_throttle = 0.0f;

// 函数原型声明
static void update_safety_state(void);
static void perform_necessary_recovery(void);

// 故障描述表
static const char *failure_descriptions[] = {
    "无故障",
    "MPU6050传感器错误",
    "电机驱动错误",
    "电池电量低",
    "电池电量严重不足",
    "遥控器信号丢失",
    "传感器数据漂移",
    "IMU传感器未准备好",
    "CPU负载过高",
    "内存分配错误",
    "系统运行错误"
};

/**
 * @brief 初始化安全模块
 */
bool safety_init(safety_config_t *config) {
    // 设置默认配置
    safety_config.enable_mpu6050_check = true;
    safety_config.enable_motor_check = true;
    safety_config.enable_battery_monitor = true;
    safety_config.enable_rc_loss_detection = true;
    safety_config.enable_sensor_drift_check = true;
    safety_config.enable_cpu_monitor = true;
    safety_config.enable_memory_monitor = true;
    
    safety_config.battery_warning_level = 3500;    // 3.5V per cell
    safety_config.battery_critical_level = 3300;   // 3.3V per cell
    safety_config.rc_loss_timeout = 2000;         // 2秒
    safety_config.imu_error_timeout = 100;         // 100ms
    safety_config.cpu_load_warning = 80;           // 80%
    
    // 如果提供了配置，则覆盖默认值
    if (config) {
        safety_config = *config;
    }
    
    // 初始化安全状态
    safety_status.state = SAFETY_STATE_NORMAL;
    safety_status.failure_count = 0;
    safety_status.state_timestamp = system_get_time_ms();
    safety_status.last_heartbeat = system_get_time_ms();
    safety_status.battery_voltage = 0;
    safety_status.battery_level = 0;
    safety_status.cpu_load = 0;
    safety_status.rc_connected = false;
    
    // 清空故障列表
    for (uint8_t i = 0; i < 16; i++) {
        safety_status.active_failures[i] = FAILURE_TYPE_NONE;
    }
    
    // 初始化解锁检测模块
    arming_detector_config_t arming_config = {
        .arm_delay_ms = 1000,
        .throttle_threshold = 0.1f,
        .roll_angle_threshold = 5.0f,
        .pitch_angle_threshold = 5.0f
    };
    arming_detector_init(&arming_config);
    
    system_log(LOG_LEVEL_INFO, "安全模块初始化完成\n");
    safety_initialized = true;
    return true;
}

/**
 * @brief 更新安全状态
 */
void safety_update(void) {
    if (!safety_initialized) {
        return;
    }
    
    uint32_t current_time = system_get_time_ms();
    safety_status.state_timestamp = current_time;
    
    // 检查心跳是否超时（系统卡顿检测）
    if (current_time - safety_status.last_heartbeat > 1000) {
        safety_report_failure(FAILURE_TYPE_SYSTEM_ERROR, "系统心跳超时");
        // 心跳超时直接强制锁定
        arming_detector_force_disarm();
    }
    
    // 检查MPU6050状态
    if (safety_config.enable_mpu6050_check && !mpu6050_is_ready()) {
        safety_report_failure(FAILURE_TYPE_MPU6050_ERROR, "MPU6050未就绪");
        // MPU6050错误时强制锁定
        arming_detector_force_disarm();
    }
    
    // 更新解锁检测状态
    arming_detector_update(rc_throttle, imu_roll, imu_pitch, mpu6050_is_ready());
    
    // 更新安全监控器
    safety_monitor_update();
    
    // 检查电池电压
    if (safety_config.enable_battery_monitor) {
        if (safety_status.battery_voltage > 0) {
            // 估算电池电量百分比（基于3.2V-4.2V的锂电池范围）
            if (safety_status.battery_voltage < 3200) {
                safety_status.battery_level = 0;
            } else if (safety_status.battery_voltage > 4200) {
                safety_status.battery_level = 100;
            } else {
                safety_status.battery_level = (safety_status.battery_voltage - 3200) * 10 / 100;
            }
            
            // 检查电池警告级别
            if (safety_status.battery_voltage < safety_config.battery_critical_level) {
                safety_report_failure(FAILURE_TYPE_BATTERY_CRITICAL, "电池电量严重不足");
            } else if (safety_status.battery_voltage < safety_config.battery_warning_level) {
                safety_report_failure(FAILURE_TYPE_BATTERY_LOW, "电池电量低");
            } else {
                safety_clear_failure(FAILURE_TYPE_BATTERY_LOW);
                safety_clear_failure(FAILURE_TYPE_BATTERY_CRITICAL);
            }
        }
    }
    
    // 检查遥控器连接
    if (safety_config.enable_rc_loss_detection && !safety_status.rc_connected) {
        safety_report_failure(FAILURE_TYPE_RC_LOSS, "遥控器信号丢失");
    } else {
        safety_clear_failure(FAILURE_TYPE_RC_LOSS);
    }
    
    // 检查CPU负载
    if (safety_config.enable_cpu_monitor) {
        if (safety_status.cpu_load > safety_config.cpu_load_warning) {
            safety_report_failure(FAILURE_TYPE_CPU_OVERLOAD, "CPU负载过高");
        } else {
            safety_clear_failure(FAILURE_TYPE_CPU_OVERLOAD);
        }
    }
    
    // 根据故障数量和类型更新安全状态
    update_safety_state();
    
    // 执行必要的恢复措施
    perform_necessary_recovery();
}

/**
 * @brief 更新安全状态等级
 */
static void update_safety_state(void) {
    // 检查是否有临界故障
    bool has_critical_failure = false;
    for (uint8_t i = 0; i < safety_status.failure_count; i++) {
        if (safety_status.active_failures[i] == FAILURE_TYPE_BATTERY_CRITICAL ||
            safety_status.active_failures[i] == FAILURE_TYPE_RC_LOSS ||
            safety_status.active_failures[i] == FAILURE_TYPE_MPU6050_ERROR ||
            safety_status.active_failures[i] == FAILURE_TYPE_SYSTEM_ERROR) {
            has_critical_failure = true;
            break;
        }
    }
    
    // 更新安全状态
    if (has_critical_failure) {
        safety_status.state = SAFETY_STATE_EMERGENCY;
    } else if (safety_status.failure_count > 0) {
        safety_status.state = SAFETY_STATE_WARNING;
    } else {
        safety_status.state = SAFETY_STATE_NORMAL;
    }
}

/**
 * @brief 执行必要的恢复措施
 */
static void perform_necessary_recovery(void) {
    if (safety_status.state == SAFETY_STATE_EMERGENCY) {
        // 在紧急状态下，立即停止电机
        motor_stop_all();
        // 禁用所有电机
        for (uint8_t i = 0; i < MOTOR_CHANNEL_MAX; i++) {
            motor_disable(i);
        }
        
        // 发送紧急状态通知
        communication_send_error(COMM_CHANNEL_GCS, 0xFF, "进入紧急安全状态");
    }
}

/**
 * @brief 获取当前安全状态
 */
bool safety_get_status(safety_status_t *status) {
    if (!safety_initialized || !status) {
        return false;
    }
    
    *status = safety_status;
    return true;
}

/**
 * @brief 触发紧急停止
 */
void safety_emergency_stop(const char *reason) {
    system_log(LOG_LEVEL_FATAL, "紧急停止: %s\n", reason ? reason : "未知原因");
    
    // 立即停止所有电机
    motor_stop_all();
    for (uint8_t i = 0; i < MOTOR_CHANNEL_MAX; i++) {
        motor_disable(i);
    }
    
    // 设置安全状态为紧急
    safety_status.state = SAFETY_STATE_EMERGENCY;
    
    // 发送紧急停止消息
    communication_send_error(COMM_CHANNEL_GCS, 0xFF, reason ? reason : "紧急停止");
    communication_send_error(COMM_CHANNEL_DEBUG, 0xFF, reason ? reason : "紧急停止");
}

/**
 * @brief 记录故障
 */
void safety_report_failure(failure_type_t type, const char *message) {
    if (!safety_initialized || type == FAILURE_TYPE_NONE || safety_status.failure_count >= 16) {
        return;
    }
    
    // 检查故障是否已经记录
    for (uint8_t i = 0; i < safety_status.failure_count; i++) {
        if (safety_status.active_failures[i] == type) {
            return;  // 故障已存在，不需要重复记录
        }
    }
    
    // 记录新故障
    safety_status.active_failures[safety_status.failure_count++] = type;
    
    // 记录日志
    system_log(LOG_LEVEL_WARNING, "故障报告 #%d: %s - %s\n", 
              type, safety_get_failure_description(type), message ? message : "");
    
    // 发送错误消息
    communication_send_error(COMM_CHANNEL_DEBUG, type, message ? message : safety_get_failure_description(type));
}

/**
 * @brief 清除故障
 */
void safety_clear_failure(failure_type_t type) {
    if (!safety_initialized || type == FAILURE_TYPE_NONE) {
        return;
    }
    
    // 查找并清除故障
    for (uint8_t i = 0; i < safety_status.failure_count; i++) {
        if (safety_status.active_failures[i] == type) {
            // 移除故障（将后面的故障前移）
            for (uint8_t j = i; j < safety_status.failure_count - 1; j++) {
                safety_status.active_failures[j] = safety_status.active_failures[j + 1];
            }
            safety_status.active_failures[safety_status.failure_count - 1] = FAILURE_TYPE_NONE;
            safety_status.failure_count--;
            
            system_log(LOG_LEVEL_INFO, "故障已清除: %s\n", safety_get_failure_description(type));
            break;
        }
    }
}

/**
 * @brief 检查系统是否安全
 */
bool safety_is_system_safe(void) {
    if (!safety_initialized) {
        return false;
    }
    
    return (safety_status.state == SAFETY_STATE_NORMAL || safety_status.state == SAFETY_STATE_WARNING);
}

/**
 * @brief 检查是否可以解锁电机
 */
bool safety_can_arm(void) {
    if (!safety_initialized) {
        return false;
    }
    
    // 只有在正常状态下才能解锁
    if (safety_status.state != SAFETY_STATE_NORMAL) {
        return false;
    }
    
    // 检查是否有禁止解锁的故障
    for (uint8_t i = 0; i < safety_status.failure_count; i++) {
        if (safety_status.active_failures[i] == FAILURE_TYPE_MPU6050_ERROR ||
            safety_status.active_failures[i] == FAILURE_TYPE_MOTOR_ERROR ||
            safety_status.active_failures[i] == FAILURE_TYPE_RC_LOSS ||
            safety_status.active_failures[i] == FAILURE_TYPE_SENSOR_DRIFT ||
            safety_status.active_failures[i] == FAILURE_TYPE_IMU_NOT_READY) {
            return false;
        }
    }
    
    // 检查MPU6050是否就绪
    if (!mpu6050_is_ready()) {
        return false;
    }
    
    // 结合解锁检测模块的结果
    return arming_detector_can_arm();
}

/**
 * @brief 设置电池电压
 */
void safety_set_battery_voltage(uint16_t voltage) {
    safety_status.battery_voltage = voltage;
}

/**
 * @brief 设置IMU角度数据（用于解锁检测）
 */
void safety_set_imu_angles(float roll, float pitch) {
    imu_roll = roll;
    imu_pitch = pitch;
}

/**
 * @brief 设置遥控器油门值（用于解锁检测）
 */
void safety_set_rc_throttle(float throttle) {
    rc_throttle = throttle;
}

/**
 * @brief 设置遥控器连接状态
 */
void safety_set_rc_connection(bool connected) {
    safety_status.rc_connected = connected;
}

/**
 * @brief 设置CPU负载
 */
void safety_set_cpu_load(uint16_t load) {
    safety_status.cpu_load = load;
}

/**
 * @brief 检查传感器数据有效性
 */
bool safety_check_sensor_data(float accel_x, float accel_y, float accel_z,
                             float gyro_x, float gyro_y, float gyro_z) {
    // 检查加速度计数据是否在合理范围内（-16g到16g）
    if (ABS(accel_x) > 16.0f || ABS(accel_y) > 16.0f || ABS(accel_z) > 16.0f) {
        safety_report_failure(FAILURE_TYPE_SENSOR_DRIFT, "加速度计数据异常");
        return false;
    }
    
    // 检查陀螺仪数据是否在合理范围内（-5000dps到5000dps）
    if (ABS(gyro_x) > 5000.0f || ABS(gyro_y) > 5000.0f || ABS(gyro_z) > 5000.0f) {
        safety_report_failure(FAILURE_TYPE_SENSOR_DRIFT, "陀螺仪数据异常");
        return false;
    }
    
    // 检查是否有NaN或无穷大值
    if (isnan(accel_x) || isnan(accel_y) || isnan(accel_z) ||
        isnan(gyro_x) || isnan(gyro_y) || isnan(gyro_z)) {
        safety_report_failure(FAILURE_TYPE_SENSOR_DRIFT, "传感器数据包含NaN");
        return false;
    }
    
    // 清除传感器漂移故障（如果之前存在）
    safety_clear_failure(FAILURE_TYPE_SENSOR_DRIFT);
    return true;
}

/**
 * @brief 执行故障恢复措施
 */
void safety_perform_recovery(failure_type_t type) {
    switch (type) {
        case FAILURE_TYPE_MPU6050_ERROR:
            // 尝试重新初始化MPU6050
    mpu6050_init(0); // 添加参数
    break;
            
        case FAILURE_TYPE_MOTOR_ERROR:
            // 停止并禁用所有电机
            motor_stop_all();
            // 禁用所有电机
            for (uint8_t i = 0; i < MOTOR_CHANNEL_MAX; i++) {
                motor_disable(i);
            }
            break;
            
        case FAILURE_TYPE_RC_LOSS:
            // 进入失控保护模式
            motor_stop_all();
            break;
            
        case FAILURE_TYPE_SENSOR_DRIFT:
            // 重新校准传感器
            // 尝试重新校准IMU
            mpu6050_calib_data_t calib_data = {0};
            mpu6050_calibrate(&calib_data, 100);
            break;
            
        default:
            break;
    }
}

/**
 * @brief 获取故障描述
 */
const char *safety_get_failure_description(failure_type_t type) {
    if (type < sizeof(failure_descriptions) / sizeof(failure_descriptions[0])) {
        return failure_descriptions[type];
    }
    return "未知故障";
}

/**
 * @brief 心跳函数
 */
void safety_heartbeat(void) {
    safety_status.last_heartbeat = system_get_time_ms();
}