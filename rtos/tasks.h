/*
 * RTOS任务定义头文件
 */

#ifndef TASKS_H
#define TASKS_H

#include "rtos_adapter.h"

// 任务句柄外部声明
extern TaskHandle_t flight_control_task_handle;
extern TaskHandle_t communication_task_handle;
extern TaskHandle_t system_monitor_task_handle;
extern TaskHandle_t safety_task_handle;

/**
 * @brief 飞行控制任务
 * 高优先级任务，每1ms执行一次
 */
void flight_control_task(void *params);

/**
 * @brief 通信任务
 * 中优先级任务，处理通信相关功能
 */
void communication_task(void *params);

/**
 * @brief 系统监控任务
 * 低优先级任务，监控系统状态
 */
void system_monitor_task(void *params);

/**
 * @brief 安全任务
 * 高优先级任务，监控系统安全状态
 */
void safety_task(void *params);

/**
 * @brief 创建所有系统任务
 * @return 是否成功创建所有任务
 */
bool create_system_tasks(void);

#endif /* TASKS_H */