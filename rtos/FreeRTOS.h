/*
 * FreeRTOS 模拟实现
 * 简化版本，用于四轴无人机项目
 */

#ifndef FREERTOS_H
#define FREERTOS_H

#include "../include/types.h"

// FreeRTOS基础类型定义
typedef uint32_t TickType_t;
typedef int32_t BaseType_t;
typedef uint32_t UBaseType_t;

// 任务句柄类型
typedef void* TaskHandle_t;

// 队列句柄类型
typedef void* QueueHandle_t;

// 互斥量句柄类型
typedef void* SemaphoreHandle_t;

// 基本任务函数类型
typedef void (*TaskFunction_t)(void*);

// 任务创建返回值
#define pdPASS 1
#define pdFAIL 0

// 任务优先级
#define tskIDLE_PRIORITY    0
#define tskMAX_PRIORITIES   5

// 队列相关宏定义
#define pdTRUE              1
#define pdFALSE             0

// 任务创建函数
extern TaskHandle_t xTaskCreate(
    TaskFunction_t pvTaskCode,
    const char* const pcName,
    const uint32_t usStackDepth,
    void* pvParameters,
    UBaseType_t uxPriority,
    TaskHandle_t* pxCreatedTask
);

// 任务删除函数
extern void vTaskDelete(TaskHandle_t xTaskToDelete);

// 任务挂起函数
extern void vTaskSuspend(TaskHandle_t xTaskToSuspend);

// 任务恢复函数
extern void vTaskResume(TaskHandle_t xTaskToResume);

// 延时函数（毫秒）
extern void vTaskDelay(uint32_t xTicksToDelay);

// 延时函数（微秒）
extern void vTaskDelayMicroseconds(uint32_t uxMicroSeconds);

// 获取系统时钟节拍数
extern TickType_t xTaskGetTickCount(void);

// 创建互斥量
extern SemaphoreHandle_t xSemaphoreCreateMutex(void);

// 获取互斥量
extern BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait);

// 释放互斥量
extern BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore);

// 创建队列
extern QueueHandle_t xQueueCreate(uint32_t uxQueueLength, uint32_t uxItemSize);

// 发送队列消息
extern BaseType_t xQueueSend(QueueHandle_t xQueue, const void* pvItemToQueue, TickType_t xTicksToWait);

// 接收队列消息
extern BaseType_t xQueueReceive(QueueHandle_t xQueue, void* pvBuffer, TickType_t xTicksToWait);

// 启动调度器
extern void vTaskStartScheduler(void);

// 停止调度器
extern void vTaskEndScheduler(void);

#endif // FREERTOS_H