/**
 * @file main.c
 * @brief 主程序入口文件
 * @details 四轴无人机飞控系统主程序，负责系统初始化、模块配置和任务调度
 *          采用模块化设计，支持跨平台移植和可配置化部署
 * @author 系统开发团队
 * @version 2.0.0
 * @date 2024-11-10
 */

#include "include/types.h"
#include "config/config.h"
#include "hal/hal_gpio.h"
#include "hal/hal_uart.h"
#include "hal/hal_spi.h"
#include "hal/hal_i2c.h"
#include "platform/platform.h"
#include "driver/mpu6050.h"
#include "driver/motor.h"
#include "service/system.h"
#include "algorithm/attitude.h"
#include "algorithm/pid.h"
#include "communication/communication.h"
#include "logic/flight_control.h"
#include "safety/safety.h"
#include "rtos/rtos_adapter.h"
#include "rtos/tasks.h"

// 系统版本信息
#define SYSTEM_VERSION "1.0.0"
#define SYSTEM_NAME "Drone Flight Controller"

// 系统初始化函数
static bool system_init_all(void);

// 声明平台注册函数
extern bool platform_register_stm32_impl(void);

int main(void) {
    // 系统启动标志
    bool system_ready = false;
    
    // 首先注册STM32平台实现
    if (!platform_register_stm32_impl()) {
        // 在平台初始化前，无法使用系统日志，直接返回错误
        system_log(LOG_LEVEL_FATAL, "Failed to register default platform implementation!\n");
        return -1;
    }
    
    // 初始化系统服务
    if (!system_init(NULL)) {
        return -1;
    }
    
    system_log(LOG_LEVEL_INFO, "%s v%s starting up...", SYSTEM_NAME);
    
    // 初始化安全模块（会自动初始化解锁检测和安全监控器）
    safety_init(NULL);
    
    // 初始化所有硬件和模块
    system_ready = system_init_all();
    
    if (!system_ready) {
        system_log(LOG_LEVEL_ERROR, "System initialization failed!\n");
        return -1;
    }
    
    system_log(LOG_LEVEL_INFO, "System initialization completed successfully.\n");
    
    // 创建RTOS任务
    if (!create_system_tasks()) {
        system_log(LOG_LEVEL_ERROR, "Failed to create system tasks!\n");
        return -1;
    }
    
    system_log(LOG_LEVEL_INFO, "RTOS tasks created successfully.\n");
    
    // 启动RTOS调度器
    system_log(LOG_LEVEL_INFO, "Starting RTOS scheduler...\n");
    rtos_start_scheduler();
    
    // 正常情况下不会到达这里
    // 如果到达这里，表示RTOS调度器启动失败
    system_log(LOG_LEVEL_FATAL, "RTOS scheduler failed to start!\n");
    
    return -1;
}

/**
 * @brief 初始化所有系统模块
 * @return 是否初始化成功
 */
static bool system_init_all(void) {
    bool success = true;
    
    system_log(LOG_LEVEL_INFO, "初始化硬件模块...\n");
    
    // 初始化GPIO
    if (!gpio_init_all()) {
        system_log(LOG_LEVEL_ERROR, "GPIO初始化失败!\n");
        safety_report_failure(FAILURE_TYPE_SYSTEM_ERROR, "GPIO初始化失败");
        success = false;
    }
    
    // 初始化I2C（用于MPU6050）
    if (!i2c_init(I2C_CHANNEL_1, 400000)) {
        system_log(LOG_LEVEL_ERROR, "I2C初始化失败!\n");
        safety_report_failure(FAILURE_TYPE_SYSTEM_ERROR, "I2C初始化失败");
        success = false;
    }
    
    // 初始化SPI（如果需要）
    // if (!spi_init(SPI_CHANNEL_1, SPI_MODE_0, SPI_DATA_SIZE_8BIT, 1000000)) {
    //     system_log(LOG_LEVEL_ERROR, "SPI初始化失败!\n");
    //     safety_report_failure(FAILURE_TYPE_SYSTEM_ERROR, "SPI初始化失败");
    //     success = false;
    // }
    
    // 初始化UART
    if (!uart_init_all()) {
        system_log(LOG_LEVEL_ERROR, "UART初始化失败!\n");
        safety_report_failure(FAILURE_TYPE_SYSTEM_ERROR, "UART初始化失败");
        success = false;
    }
    
    system_log(LOG_LEVEL_INFO, "初始化驱动模块...\n");
    
    // 初始化MPU6050传感器
    mpu6050_config_t mpu6050_config = {
        .i2c_channel = I2C_CHANNEL_1,
        .device_address = MPU6050_ADDR_AD0_LOW,
        .gyro_range = MPU6050_GYRO_RANGE_2000DPS,
        .accel_range = MPU6050_ACCEL_RANGE_8G,
        .dlpf_bandwidth = MPU6050_DLPF_BW_41HZ,
        .sample_rate_divider = 1
    };
    
    if (!mpu6050_init(&mpu6050_config)) {
        system_log(LOG_LEVEL_ERROR, "MPU6050初始化失败!\n");
        safety_report_failure(FAILURE_TYPE_MPU6050_ERROR, "MPU6050初始化失败");
        success = false;
    } else {
        // 校准MPU6050
        system_log(LOG_LEVEL_INFO, "校准MPU6050传感器...\n");
        mpu6050_calib_data_t calib_data;
        mpu6050_calibrate(&calib_data, 1000);
    }
    
    // 初始化电机驱动
    if (!motor_init_all()) {
        system_log(LOG_LEVEL_ERROR, "电机驱动初始化失败!\n");
        safety_report_failure(FAILURE_TYPE_MOTOR_ERROR, "电机驱动初始化失败");
        success = false;
    }
    
    system_log(LOG_LEVEL_INFO, "初始化通信模块...\n");
    
    // 初始化通信模块
    if (!communication_init_all()) {
        system_log(LOG_LEVEL_ERROR, "通信模块初始化失败!\n");
        safety_report_failure(FAILURE_TYPE_SYSTEM_ERROR, "通信模块初始化失败");
        success = false;
    }
    
    system_log(LOG_LEVEL_INFO, "初始化算法模块...\n");
    
    // 初始化姿态解算算法
    attitude_config_t attitude_config = {
        .algorithm = ATTITUDE_ALGORITHM_COMPLEMENTARY,
        .complementary_gain = 0.98f
    };
    if (!attitude_init(&attitude_config)) {
        system_log(LOG_LEVEL_ERROR, "姿态解算算法初始化失败!\n");
        safety_report_failure(FAILURE_TYPE_SYSTEM_ERROR, "姿态解算算法初始化失败");
        success = false;
    }
    
    // 初始化PID控制器
    bool pid_init_result = true;
    
    // 横滚通道PID配置
    pid_config_t roll_pid_config = {
        .mode = PID_MODE_POSITION,
        .kp = 5.0f,
        .ki = 0.05f,
        .kd = 2.0f,
        .output_max = 400.0f,
        .output_min = -400.0f,
        .integrator_max = 50.0f,
        .differentiator_max = 100.0f,
        .anti_windup = true,
        .deadband = 0.1f
    };
    
    // 俯仰通道PID配置
    pid_config_t pitch_pid_config = {
        .mode = PID_MODE_POSITION,
        .kp = 5.0f,
        .ki = 0.05f,
        .kd = 2.0f,
        .output_max = 400.0f,
        .output_min = -400.0f,
        .integrator_max = 50.0f,
        .differentiator_max = 100.0f,
        .anti_windup = true,
        .deadband = 0.1f
    };
    
    // 偏航通道PID配置
    pid_config_t yaw_pid_config = {
        .mode = PID_MODE_POSITION,
        .kp = 3.0f,
        .ki = 0.03f,
        .kd = 0.5f,
        .output_max = 300.0f,
        .output_min = -300.0f,
        .integrator_max = 30.0f,
        .differentiator_max = 50.0f,
        .anti_windup = true,
        .deadband = 0.1f
    };
    
    // 初始化各通道PID
    if (!pid_init(&roll_pid_config) || 
        !pid_init(&pitch_pid_config) || 
        !pid_init(&yaw_pid_config)) {
        pid_init_result = false;
    }
    
    if (!pid_init_result) {
        system_log(LOG_LEVEL_ERROR, "PID控制器初始化失败!\n");
        safety_report_failure(FAILURE_TYPE_SYSTEM_ERROR, "PID控制器初始化失败");
        success = false;
    }
    
    system_log(LOG_LEVEL_INFO, "初始化飞行控制...\n");
    
    // 初始化飞行控制
    flight_control_config_t fc_config = {
        .use_gyro_calibration = true,
        .use_accel_calibration = true,
        .use_magnetometer = false,
        .min_throttle = 0.1f,
        .max_throttle = 1.0f,
        .hover_throttle = 0.5f,
        .yaw_boost = 1.0f,
        .stabilization_enabled = true
    };
    if (!flight_control_init(&fc_config)) {
        system_log(LOG_LEVEL_ERROR, "飞行控制初始化失败!\n");
        safety_report_failure(FAILURE_TYPE_SYSTEM_ERROR, "飞行控制初始化失败");
        success = false;
    }
    
    // 检查系统是否安全
    if (!safety_is_system_safe()) {
        system_log(LOG_LEVEL_WARNING, "系统初始化完成但处于非安全状态!\n");
    }
    
    return success;
}