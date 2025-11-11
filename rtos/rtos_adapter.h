/*
 * RTOS适配层接口定义 - 增强版
 * 将系统服务接口与FreeRTOS对接
 */

#ifndef RTOS_ADAPTER_H
#define RTOS_ADAPTER_H

#include "FreeRTOS.h"

// FreeRTOS任务函数类型
typedef void (*TaskFunction_t)(void *);

// 定义基础类型
typedef uint32_t UBaseType_t;

// 前向声明FreeRTOS类型
typedef void* TaskHandle_t;
typedef void* QueueHandle_t;
typedef void* SemaphoreHandle_t;
typedef enum { eRunning = 0, eReady, eBlocked, eSuspended, eDeleted, eInvalid } eTaskState;

// RTOS错误码
typedef enum {
    RTOS_OK = 0,
    RTOS_ERROR = -1,
    RTOS_ERR_GENERAL,
    RTOS_ERR_MEMORY,
    RTOS_ERR_TIMEOUT,
    RTOS_ERR_INVALID_PARAM
} rtos_err_t;

#define pdMS_TO_TICKS( xTimeInMs ) ( ( TickType_t ) ( ( ( TickType_t ) ( xTimeInMs ) * ( TickType_t ) configTICK_RATE_HZ ) / ( TickType_t ) 1000 ) )
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdFAIL 0

// 类型定义
typedef TickType_t tick_t;  // 使用FreeRTOS的标准类型

/**
 * @brief RTOS初始化 - 增强版
 */
bool rtos_init(void);

/**
 * @brief RTOS任务创建函数 - 增强版
 * 参数：
 * - name: 任务名称
 * - task_function: 任务函数指针
 * - stack_size: 堆栈大小（字节）
 * - params: 任务参数
 * - priority: 任务优先级
 * - task_handle: 输出参数，任务句柄
 * 返回：任务句柄，失败返回NULL
 */
TaskHandle_t rtos_create_task(const char *name, TaskFunction_t task_function, 
                             uint32_t stack_size, void *params, 
                             UBaseType_t priority, TaskHandle_t *task_handle);

/**
 * @brief RTOS任务删除函数 - 增强版
 * 参数：
 * - task_handle: 任务句柄
 */
void rtos_delete_task(TaskHandle_t task_handle);

/**
 * @brief RTOS延时函数 - 增强版
 * 参数：
 * - ms: 延时毫秒数
 */
void rtos_delay(uint32_t ms);

/**
 * @brief 任务延时（微秒） - 增强版
 * 参数：
 * - us: 延时微秒数
 */
void rtos_delay_us(uint32_t us);

/**
 * @brief 获取当前时间 - 增强版
 * 返回：系统节拍数
 */
tick_t rtos_get_tick_count(void);

/**
 * @brief RTOS互斥量获取 - 增强版
 * 参数：
 * - mutex: 互斥锁句柄
 * - timeout: 超时时间（系统节拍数）
 * 返回：成功返回true，失败返回false
 */
bool rtos_mutex_take(SemaphoreHandle_t mutex, uint32_t timeout);

/**
 * @brief RTOS互斥量释放 - 增强版
 * 参数：
 * - mutex: 互斥锁句柄
 * 返回：成功返回true，失败返回false
 */
bool rtos_mutex_give(SemaphoreHandle_t mutex);

/**
 * @brief 创建互斥锁 - 增强版
 * 返回：互斥锁句柄，失败返回NULL
 */
SemaphoreHandle_t rtos_create_mutex(void);

/**
 * @brief 删除互斥锁 - 增强版
 * 参数：
 * - mutex: 互斥锁句柄
 */
void rtos_delete_mutex(SemaphoreHandle_t mutex);

/**
 * @brief 获取系统互斥量 - 增强版
 * 参数：
 * - timeout: 超时时间（系统节拍数）
 * 返回：成功返回true，失败返回false
 */
bool rtos_get_system_mutex(uint32_t timeout);

/**
 * @brief 释放系统互斥量 - 增强版
 * 返回：成功返回true，失败返回false
 */
bool rtos_release_system_mutex(void);

/**
 * @brief 获取当前任务优先级 - 增强版
 * 参数：
 * - task_handle: 任务句柄
 * 返回：任务优先级，无效句柄返回0
 */
UBaseType_t rtos_get_task_priority(TaskHandle_t task_handle);

/**
 * @brief 设置任务优先级 - 增强版
 * 参数：
 * - task_handle: 任务句柄
 * - priority: 新优先级
 * 返回：成功返回true，失败返回false
 */
bool rtos_set_task_priority(TaskHandle_t task_handle, UBaseType_t priority);

/**
 * @brief 挂起任务 - 增强版
 * 参数：
 * - task_handle: 任务句柄
 */
void rtos_suspend_task(TaskHandle_t task_handle);

/**
 * @brief 恢复任务 - 增强版
 * 参数：
 * - task_handle: 任务句柄
 */
void rtos_resume_task(TaskHandle_t task_handle);

/**
 * @brief 获取任务状态 - 增强版
 * 参数：
 * - task_handle: 任务句柄
 * 返回：任务状态枚举值
 */
eTaskState rtos_get_task_state(TaskHandle_t task_handle);

/**
 * @brief 启动调度器 - 增强版
 */
void rtos_start_scheduler(void);

/**
 * @brief 创建队列 - 增强版
 * 参数：
 * - queue_len: 队列长度（最大项目数）
 * - item_size: 每个项目的大小（字节）
 * 返回：队列句柄，失败返回NULL
 */
QueueHandle_t rtos_create_queue(uint32_t queue_len, uint32_t item_size);

/**
 * @brief 发送队列消息 - 增强版
 * 参数：
 * - queue: 队列句柄
 * - data: 数据指针
 * - timeout: 超时时间（系统节拍数）
 * 返回：成功返回true，失败返回false
 */
bool rtos_queue_send(QueueHandle_t queue, const void* data, uint32_t timeout);

/**
 * @brief 接收队列消息 - 增强版
 * 参数：
 * - queue: 队列句柄
 * - buffer: 接收缓冲区指针
 * - timeout: 超时时间（系统节拍数）
 * 返回：成功返回true，失败返回false
 */
bool rtos_queue_receive(QueueHandle_t queue, void* buffer, uint32_t timeout);

/**
 * @brief 删除队列 - 增强版
 * 参数：
 * - queue: 队列句柄
 */
void rtos_delete_queue(QueueHandle_t queue);

// 系统任务管理函数在system.c中实现

#endif /* RTOS_ADAPTER_H */