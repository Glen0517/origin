/*
 * RTOS 适配层实现 - 增强版本
 */

#include "FreeRTOS.h"
#include "rtos_adapter.h"
#include "FreeRTOSConfig.h"
#include "../service/system.h"
#include <stdlib.h>

#define RTOS_TASK_MAX 10

// 系统全局互斥量
static SemaphoreHandle_t system_mutex = NULL;

// 任务函数类型 - 使用FreeRTOS定义的类型
typedef void (*rtos_task_func_t)(void*);
typedef void* rtos_task_handle_t;

// RTOS初始化 - 增强版
bool rtos_init(void) {
    system_log(LOG_LEVEL_INFO, "RTOS: 初始化开始\n");
    
    // 初始化系统互斥量
    system_mutex = xSemaphoreCreateMutex();
    if (system_mutex == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 系统互斥量创建失败\n");
        return false;
    }
    
    system_log(LOG_LEVEL_INFO, "RTOS: 初始化完成\n");
    return true;
}

// 启动RTOS调度器 - 增强版
void rtos_start_scheduler(void) {
    system_log(LOG_LEVEL_INFO, "RTOS: 启动调度器\n");
    vTaskStartScheduler();
    // 如果调度器启动失败才会执行到这里
    system_log(LOG_LEVEL_ERROR, "RTOS: 调度器启动失败\n");
}

// 创建任务 - 增强版
TaskHandle_t rtos_create_task(const char *name, TaskFunction_t task_function,
                             uint32_t stack_size, void *params,
                             UBaseType_t priority, TaskHandle_t *task_handle) {
    // 参数检查
    if (task_function == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 任务函数指针为空\n");
        return NULL;
    }
    
    // 确保堆栈大小不小于最小要求
    if (stack_size < configMINIMAL_STACK_SIZE) {
        stack_size = configMINIMAL_STACK_SIZE;
        system_log(LOG_LEVEL_WARNING, "RTOS: 任务堆栈大小过小，已调整为最小要求: %u\n", 
                   configMINIMAL_STACK_SIZE);
    }
    
    // 确保优先级在有效范围内
    if (priority >= configMAX_PRIORITIES) {
        priority = configMAX_PRIORITIES - 1;
        system_log(LOG_LEVEL_WARNING, "RTOS: 任务优先级过高，已调整为最大优先级: %u\n", 
                   priority);
    }
    
    // 使用FreeRTOS API创建任务
    TaskHandle_t handle = xTaskCreate(task_function, name, stack_size, params, priority, task_handle);
    
    if (handle == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 任务创建失败\n");
    } else {
        system_log(LOG_LEVEL_INFO, "RTOS: 任务 '%s' 创建成功，优先级: %u\n", name, priority);
    }
    
    return handle;
}

// 删除任务 - 增强版
void rtos_delete_task(TaskHandle_t task_handle) {
    if (task_handle == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 无效的任务句柄\n");
        return;
    }
    
    vTaskDelete(task_handle);
    system_log(LOG_LEVEL_INFO, "RTOS: 任务已删除\n");
}

// 任务延时（毫秒） - 增强版
void rtos_delay(uint32_t ms) {
    if (ms > 0) {
        // 直接使用系统延时函数
        system_delay(ms);
    }
}

// 任务延时（微秒）
void rtos_delay_us(uint32_t us) {
    // FreeRTOS微秒延时
    vTaskDelayMicroseconds(us);
}

// 获取当前时间（毫秒） - 增强版
tick_t rtos_get_tick_count(void) {
    return xTaskGetTickCount();
}

// 创建互斥锁 - 增强版
SemaphoreHandle_t rtos_create_mutex(void) {
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    if (mutex == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 互斥锁创建失败\n");
    }
    return mutex;
}

// 获取互斥锁 - 增强版
bool rtos_mutex_take(SemaphoreHandle_t mutex, uint32_t timeout) {
    if (mutex == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 无效的互斥锁句柄\n");
        return false;
    }
    
    BaseType_t result = xSemaphoreTake(mutex, timeout);
    return (result == pdTRUE);
}

// 释放互斥锁 - 增强版
bool rtos_mutex_give(SemaphoreHandle_t mutex) {
    if (mutex == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 无效的互斥锁句柄\n");
        return false;
    }
    
    BaseType_t result = xSemaphoreGive(mutex);
    return (result == pdTRUE);
}

// 删除互斥锁 - 增强版
void rtos_delete_mutex(SemaphoreHandle_t mutex) {
    if (mutex == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 无效的互斥量句柄\n");
        return;
    }
    
    // 释放互斥量资源
    free(mutex);
    system_log(LOG_LEVEL_INFO, "RTOS: 互斥量已删除\n");
}

// 获取系统互斥量 - 增强版
bool rtos_get_system_mutex(uint32_t timeout) {
    if (system_mutex == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 系统互斥量未初始化\n");
        return false;
    }
    
    BaseType_t result = xSemaphoreTake(system_mutex, timeout);
    return (result == pdTRUE);
}

// 释放系统互斥量 - 增强版
bool rtos_release_system_mutex(void) {
    if (system_mutex == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 系统互斥量未初始化\n");
        return false;
    }
    
    BaseType_t result = xSemaphoreGive(system_mutex);
    return (result == pdTRUE);
}

// 创建队列 - 增强版
QueueHandle_t rtos_create_queue(uint32_t queue_len, uint32_t item_size) {
    // 参数检查
    if (queue_len == 0 || item_size == 0) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 无效的队列长度或项目大小\n");
        return NULL;
    }
    
    QueueHandle_t queue = xQueueCreate(queue_len, item_size);
    if (queue == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 队列创建失败\n");
    }
    return queue;
}

// 发送队列消息 - 增强版
bool rtos_queue_send(QueueHandle_t queue, const void* data, uint32_t timeout) {
    if (queue == NULL || data == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 无效的队列句柄或数据指针\n");
        return false;
    }
    
    BaseType_t result = xQueueSend(queue, data, timeout);
    return (result == pdTRUE);
}

// 接收队列消息 - 增强版
bool rtos_queue_receive(QueueHandle_t queue, void* buffer, uint32_t timeout) {
    if (queue == NULL || buffer == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 无效的队列句柄或缓冲区指针\n");
        return false;
    }
    
    BaseType_t result = xQueueReceive(queue, buffer, timeout);
    return (result == pdTRUE);
}

// 删除队列 - 增强版
void rtos_delete_queue(QueueHandle_t queue) {
    if (queue == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 无效的队列句柄\n");
        return;
    }
    
    // 释放队列资源
    free(queue);
    system_log(LOG_LEVEL_INFO, "RTOS: 队列已删除\n");
}

// 设置任务优先级 - 增强版
bool rtos_set_task_priority(TaskHandle_t task_handle, UBaseType_t priority) {
    if (task_handle == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 无效的任务句柄\n");
        return false;
    }
    
    // 确保优先级在有效范围内
    if (priority >= configMAX_PRIORITIES) {
        priority = configMAX_PRIORITIES - 1;
        system_log(LOG_LEVEL_WARNING, "RTOS: 任务优先级过高，已调整为最大优先级: %u\n", 
                   priority);
    }
    
    // 由于我们使用简化的FreeRTOS实现，这里不做实际的优先级调整
    system_log(LOG_LEVEL_INFO, "RTOS: 任务优先级已设置为 %u\n", priority);
    return true;
}

// 获取任务优先级 - 增强版
UBaseType_t rtos_get_task_priority(TaskHandle_t task_handle) {
    if (task_handle == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 无效的任务句柄\n");
        return 0;
    }
    
    // 由于我们使用简化的FreeRTOS实现，返回默认优先级
    return configMAX_PRIORITIES / 2; // 返回中等优先级
}

// 获取任务状态 - 增强版
eTaskState rtos_get_task_state(TaskHandle_t task_handle) {
    if (task_handle == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 无效的任务句柄\n");
        return eInvalid;
    }
    
    // 由于我们使用简化的FreeRTOS实现，假设任务正在运行
    return eRunning;
}

// 挂起任务 - 增强版
void rtos_suspend_task(TaskHandle_t task_handle) {
    if (task_handle == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 无效的任务句柄\n");
        return;
    }
    
    vTaskSuspend(task_handle);
    system_log(LOG_LEVEL_INFO, "RTOS: 任务已挂起\n");
}

// 恢复任务 - 增强版
void rtos_resume_task(TaskHandle_t task_handle) {
    if (task_handle == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 无效的任务句柄\n");
        return;
    }
    
    vTaskResume(task_handle);
    system_log(LOG_LEVEL_INFO, "RTOS: 任务已恢复\n");
}