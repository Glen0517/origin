/*
 * RTOS任务定义与实现
 */

#include "tasks.h"
#include "../service/system.h"
#include "../hal/hal_gpio.h"
#include "../hal/hal_uart.h"
#include "../hal/hal_spi.h"
#include "../hal/hal_i2c.h"
#include "../hal/hal_timer.h"
#include "../driver/mpu6050.h"
#include "../driver/motor.h"
#include "../algorithm/attitude.h"
#include "../algorithm/pid.h"
#include "../communication/communication.h"
#include "../logic/flight_control.h"
#include "../safety/safety.h"
#include "../include/types.h"

// 任务句柄
TaskHandle_t flight_control_task_handle = NULL;
TaskHandle_t communication_task_handle = NULL;
TaskHandle_t system_monitor_task_handle = NULL;
TaskHandle_t safety_task_handle = NULL;

/**
 * @brief 飞行控制任务
 * 高优先级任务，每1ms执行一次
 */
void flight_control_task(void *params) {
    while (1) {
        // 只有在系统安全时才运行飞行控制
        if (safety_is_system_safe()) {
            flight_control_loop(1.0f);  // 传入1ms的时间间隔
        } else {
            // 系统不安全，确保电机停止
            motor_stop_all();
        }
        
        // 1ms延时，确保任务以1kHz频率运行
        rtos_delay(1);
    }
}

/**
 * @brief 通信任务
 * 中优先级任务，处理通信相关功能
 */
void communication_task(void *params) {
    uint32_t last_heartbeat_time = 0;
    
    while (1) {
        // 处理所有通信通道
        for (uint8_t i = 0; i < COMM_CHANNEL_MAX; i++) {
            if (communication_has_data(i)) {
                message_t message;
                if (communication_receive(i, &message) > 0) {
                    communication_process_message(i, &message);
                }
            }
        }
        
        // 每100ms发送心跳包
        uint32_t current_time = system_get_time_ms();
        if (current_time - last_heartbeat_time > 100) {
            communication_send_heartbeat(COMM_CHANNEL_GCS);
            last_heartbeat_time = current_time;
        }
        
        // 通信任务可以稍微低频率运行
        rtos_delay(10);
    }
}

/**
 * @brief 系统监控任务
 * 低优先级任务，处理日志、状态报告等
 */
void system_monitor_task(void *params) {
    uint32_t last_log_time = 0;
    uint32_t loop_count = 0;
    
    while (1) {
        loop_count++;
        
        // 更新系统状态
        system_update();
        
        // 每1秒记录系统状态
        uint32_t current_time = system_get_time_ms();
        if (current_time - last_log_time > 1000) {
            system_status_t status;
            system_get_status(&status);
            communication_send_status(COMM_CHANNEL_LOGGING, &status);
            
            // 显示系统信息
            system_log(LOG_LEVEL_INFO, "Loop: %lu, CPU: %d%%, Heap: %d/%d bytes\n",
                      loop_count, status.cpu_usage, status.used_heap, status.total_heap);
            
            // 获取并显示安全状态
            safety_status_t safety_stat;
            if (safety_get_status(&safety_stat)) {
                system_log(LOG_LEVEL_INFO, "安全状态: %d, 故障数: %d, 电池电压: %dmV\n",
                          safety_stat.state, safety_stat.failure_count, safety_stat.battery_voltage);
            }
            
            last_log_time = current_time;
        }
        
        // 检查错误
        if (system_has_error()) {
            error_info_t error;
            while (system_get_error(&error)) {
                system_log(LOG_LEVEL_ERROR, "Error #%d: %s\n", error.code, error.message);
                communication_send_error(COMM_CHANNEL_DEBUG, error.code, error.message);
            }
        }
        
        // 监控任务可以以较低频率运行
        rtos_delay(50);
    }
}

/**
 * @brief 安全监控任务
 * 高优先级任务，持续监控系统安全状态
 */
void safety_task(void *params) {
    while (1) {
        // 更新安全状态
        safety_update();
        
        // 发送安全心跳
        safety_heartbeat();
        
        // 安全任务需要高频运行
        rtos_delay(5);
    }
}

/**
 * @brief 创建所有系统任务
 */
bool create_system_tasks(void) {
    // 飞行控制任务 - 高优先级
    system_task_t flight_task = {
        .name = "FlightControl",
        .function = (task_func_t)flight_control_task,
        .params = NULL,
        .interval = 1,  // 1ms
        .priority = TASK_PRIORITY_HIGH
    };
    
    if (system_add_task(&flight_task) < 0) {
        system_log(LOG_LEVEL_ERROR, "创建飞行控制任务失败\n");
        return false;
    }
    
    // 安全监控任务 - 最高优先级
    system_task_t safety_monitor_task = {
        .name = "SafetyMonitor",
        .function = (task_func_t)safety_task,
        .params = NULL,
        .interval = 5,  // 5ms
        .priority = TASK_PRIORITY_REALTIME
    };
    
    if (system_add_task(&safety_monitor_task) < 0) {
        system_log(LOG_LEVEL_ERROR, "创建安全任务失败\n");
        return false;
    }
    
    // 通信任务 - 中优先级
    system_task_t comm_task = {
        .name = "Communication",
        .function = (task_func_t)communication_task,
        .params = NULL,
        .interval = 10,  // 10ms
        .priority = TASK_PRIORITY_MEDIUM
    };
    
    if (system_add_task(&comm_task) < 0) {
        system_log(LOG_LEVEL_ERROR, "创建通信任务失败\n");
        return false;
    }
    
    // 系统监控任务 - 低优先级
    system_task_t monitor_task = {
        .name = "SystemMonitor",
        .function = (task_func_t)system_monitor_task,
        .params = NULL,
        .interval = 50,  // 50ms
        .priority = TASK_PRIORITY_LOW
    };
    
    if (system_add_task(&monitor_task) < 0) {
        system_log(LOG_LEVEL_ERROR, "创建系统监控任务失败\n");
        return false;
    }
    
    // 启动所有任务
    system_start_task(0);
    system_start_task(1);
    system_start_task(2);
    system_start_task(3);
    
    return true;
}