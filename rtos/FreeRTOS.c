/*
 * FreeRTOS 模拟实现 - 增强版
 * 用于四轴无人机项目，包含完整的任务调度、同步机制和钩子函数
 */

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "../service/system.h"
#include "../include/types.h"
#include "../platform/platform.h"

#include <stdlib.h>
#include <string.h>

// 前向声明
static void vTaskSwitchContext(void);

// 任务结构体定义
typedef struct {
    TaskFunction_t pxTaskCode;        // 任务函数指针
    const char* pcName;              // 任务名称
    uint32_t usStackDepth;           // 堆栈深度
    void* pvParameters;              // 任务参数
    UBaseType_t uxPriority;          // 任务优先级
    TaskHandle_t xHandle;            // 任务句柄（指向自身）
    bool is_running;                 // 运行状态
    bool is_suspended;               // 挂起状态
    uint32_t last_run_time;          // 上次运行时间
    uint32_t run_count;              // 运行次数
    uint8_t *pxStack;                // 任务堆栈指针
    volatile uint32_t *pxTopOfStack; // 堆栈顶部指针（保存上下文）
} Task_t;

// 任务列表
#define MAX_TASKS 10
static Task_t tasks[MAX_TASKS];
static uint8_t task_count = 0;
static bool scheduler_running = false;

// 当前运行的任务指针
static Task_t *pxCurrentTCB = NULL;

// 任务创建实现 - 改进版
TaskHandle_t xTaskCreate(
    TaskFunction_t pvTaskCode,
    const char* const pcName,
    const uint32_t usStackDepth,
    void* pvParameters,
    UBaseType_t uxPriority,
    TaskHandle_t* pxCreatedTask
) {
    // 检查是否还有空间
    if (task_count >= MAX_TASKS) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 任务数量超过上限\n");
        return NULL;
    }
    
    // 参数有效性检查
    if (pvTaskCode == NULL || usStackDepth < configMINIMAL_STACK_SIZE) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 无效的任务参数\n");
        return NULL;
    }
    
    // 分配任务结构体
    Task_t* task = &tasks[task_count];
    
    // 初始化任务基本参数
    task->pxTaskCode = pvTaskCode;
    task->pcName = pcName;
    task->usStackDepth = usStackDepth;
    task->pvParameters = pvParameters;
    
    // 确保优先级在有效范围内
    if (uxPriority >= configMAX_PRIORITIES) {
        uxPriority = configMAX_PRIORITIES - 1;
    }
    task->uxPriority = uxPriority;
    
    task->xHandle = task;
    task->is_running = false;
    task->is_suspended = false;
    task->last_run_time = 0;
    task->run_count = 0;
    
    // 初始化任务堆栈和上下文
    // 分配任务堆栈内存
    task->pxStack = (uint8_t*)system_malloc(usStackDepth);
    if (task->pxStack == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 任务 %s 堆栈内存分配失败\n", pcName);
        #if (configUSE_MALLOC_FAILED_HOOK == 1)
        vApplicationMallocFailedHook();
        #endif
        return NULL;
    }
    
    system_log(LOG_LEVEL_DEBUG, "RTOS: 任务 %s 堆栈分配成功，大小: %u 字节\n", 
               pcName, usStackDepth);
    
    // 设置堆栈顶指针（从高地址开始）
    task->pxTopOfStack = (volatile uint32_t*)(task->pxStack + usStackDepth);
    
    // 模拟初始化堆栈，放入任务函数地址和参数
    // 在实际RTOS中，这里需要保存完整的CPU上下文
    *(task->pxTopOfStack - 1) = (uint32_t)pvTaskCode;  // 任务函数地址
    *(task->pxTopOfStack - 2) = (uint32_t)pvParameters; // 任务参数
    
    // 初始化堆栈底部的溢出检测标志（填充特定值）
    #if (configCHECK_FOR_STACK_OVERFLOW > 0)
    memset(task->pxStack, 0xAA, 32);  // 填充栈底作为溢出检测
    #endif
    
    // 增加任务计数
    task_count++;
    
    system_log(LOG_LEVEL_DEBUG, "RTOS: 任务创建成功 - %s (优先级: %u, 堆栈: %u字节)\n", 
               pcName, uxPriority, usStackDepth);
    
    // 如果提供了句柄指针，返回任务句柄
    if (pxCreatedTask != NULL) {
        *pxCreatedTask = task;
    }
    
    return task;
}

// 任务删除实现 - 改进版
void vTaskDelete(TaskHandle_t xTaskToDelete) {
    if (xTaskToDelete == NULL) {
        system_log(LOG_LEVEL_WARNING, "RTOS: 尝试删除空任务句柄\n");
        return;
    }
    
    // 找到对应的任务并删除
    for (uint8_t i = 0; i < task_count; i++) {
        if (tasks[i].xHandle == xTaskToDelete) {
            Task_t *task = &tasks[i];
            
            system_log(LOG_LEVEL_DEBUG, "RTOS: 删除任务 - %s\n", task->pcName);
            
            // 释放任务堆栈内存
            if (task->pxStack != NULL) {
                system_free(task->pxStack);
                task->pxStack = NULL;
                task->pxTopOfStack = NULL;
            }
            
            // 如果删除的是当前运行的任务，设置当前任务为NULL
            if (pxCurrentTCB == task) {
                pxCurrentTCB = NULL;
            }
            
            // 移动后续任务，保持数组紧凑
            for (uint8_t j = i; j < task_count - 1; j++) {
                tasks[j] = tasks[j + 1];
            }
            
            // 清空最后一个元素，防止悬空指针
            memset(&tasks[task_count - 1], 0, sizeof(Task_t));
            
            // 减少任务计数
            task_count--;
            
            system_log(LOG_LEVEL_DEBUG, "RTOS: 任务删除成功，剩余任务数: %u\n", task_count);
            break;
        }
    }
}

// 任务挂起实现
void vTaskSuspend(TaskHandle_t xTaskToSuspend) {
    if (xTaskToSuspend == NULL) {
        return;
    }
    
    Task_t* task = (Task_t*)xTaskToSuspend;
    task->is_suspended = true;
}

// 任务恢复实现
void vTaskResume(TaskHandle_t xTaskToResume) {
    if (xTaskToResume == NULL) {
        return;
    }
    
    Task_t* task = (Task_t*)xTaskToResume;
    task->is_suspended = false;
}

// 延时函数实现（毫秒） - 改进版
void vTaskDelay(uint32_t xTicksToDelay) {
    if (xTicksToDelay == 0 || pxCurrentTCB == NULL) {
        return;
    }
    
    uint32_t start_time = system_get_time_ms();
    uint32_t end_time = start_time + xTicksToDelay;
    
    // 在延时期间让出CPU给其他任务
    while (system_get_time_ms() < end_time) {
        // 进行上下文切换，允许其他任务运行
        vTaskSwitchContext();
        
        // 短暂延时避免CPU占用过高
        system_delay(1);
    }
    
    system_log(LOG_LEVEL_DEBUG, "RTOS: 任务 %s 延时结束\n", pxCurrentTCB->pcName);
}

// 延时函数实现（微秒）
void vTaskDelayMicroseconds(uint32_t uxMicroSeconds) {
    // 简化实现，使用系统延时
    system_delay((uxMicroSeconds + 999) / 1000);
}

// 获取系统时钟节拍数实现
TickType_t xTaskGetTickCount(void) {
    return (TickType_t)system_get_time_ms();
}

// 互斥量结构体定义
typedef struct {
    bool is_taken;              // 互斥量是否被获取
    Task_t *owner;              // 当前持有互斥量的任务
    uint32_t recursion_count;   // 递归计数（用于递归互斥量）
    uint32_t wait_count;        // 等待任务数量
} Mutex_t;

// 创建互斥量实现 - 改进版
SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    // 分配互斥量内存
    Mutex_t *mutex = (Mutex_t*)system_malloc(sizeof(Mutex_t));
    if (mutex == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 互斥量内存分配失败\n");
        return NULL;
    }
    
    // 初始化互斥量
    mutex->is_taken = false;
    mutex->owner = NULL;
    mutex->recursion_count = 0;
    mutex->wait_count = 0;
    
    system_log(LOG_LEVEL_DEBUG, "RTOS: 互斥量创建成功\n");
    return (SemaphoreHandle_t)mutex;
}

// 获取互斥量实现 - 改进版
BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait) {
    if (xSemaphore == NULL) {
        return pdFALSE;
    }
    
    Mutex_t *mutex = (Mutex_t*)xSemaphore;
    uint32_t start_time = 0;
    
    // 检查是否是递归获取（同一任务再次获取）
    if (mutex->is_taken && mutex->owner == pxCurrentTCB) {
        mutex->recursion_count++;
        system_log(LOG_LEVEL_DEBUG, "RTOS: 递归获取互斥量，计数: %u\n", mutex->recursion_count);
        return pdTRUE;
    }
    
    // 记录开始时间，用于超时计算
    if (xTicksToWait > 0) {
        start_time = system_get_time_ms();
    }
    
    // 尝试获取互斥量，如果已被占用则等待
    while (mutex->is_taken) {
        // 检查是否超时
        if (xTicksToWait > 0 && 
            (system_get_time_ms() - start_time) >= xTicksToWait) {
            system_log(LOG_LEVEL_DEBUG, "RTOS: 互斥量获取超时\n");
            return pdFALSE;
        }
        
        // 增加等待计数
        mutex->wait_count++;
        
        // 让出CPU给其他任务
        vTaskSwitchContext();
        
        // 短暂延时
        system_delay(1);
        
        // 减少等待计数
        mutex->wait_count--;
    }
    
    // 获取互斥量成功
    mutex->is_taken = true;
    mutex->owner = pxCurrentTCB;
    mutex->recursion_count = 1;
    
    system_log(LOG_LEVEL_DEBUG, "RTOS: 任务 %s 获取互斥量成功\n", 
               pxCurrentTCB ? pxCurrentTCB->pcName : "<unknown>");
    return pdTRUE;
}

// 释放互斥量实现 - 改进版
BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore) {
    if (xSemaphore == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 尝试释放空互斥量\n");
        return pdFALSE;
    }
    
    Mutex_t *mutex = (Mutex_t*)xSemaphore;
    
    // 检查是否是互斥量的所有者
    if (mutex->owner != pxCurrentTCB) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 任务 %s 不是互斥量的所有者\n", 
                   pxCurrentTCB ? pxCurrentTCB->pcName : "<unknown>");
        return pdFALSE;
    }
    
    // 减少递归计数
    mutex->recursion_count--;
    
    // 如果递归计数为0，释放互斥量
    if (mutex->recursion_count == 0) {
        mutex->is_taken = false;
        mutex->owner = NULL;
        
        system_log(LOG_LEVEL_DEBUG, "RTOS: 任务 %s 释放互斥量成功\n", 
                   pxCurrentTCB->pcName);
    } else {
        system_log(LOG_LEVEL_DEBUG, "RTOS: 递归释放互斥量，剩余计数: %u\n", 
                   mutex->recursion_count);
    }
    
    return pdTRUE;
}

// 队列结构体定义
typedef struct {
    uint8_t *buffer;            // 队列缓冲区
    uint32_t item_size;         // 每个元素的大小
    uint32_t length;            // 队列最大长度
    uint32_t items;             // 当前队列中的元素数量
    uint32_t head;              // 队列头索引
    uint32_t tail;              // 队列尾索引
    SemaphoreHandle_t mutex;    // 用于保护队列访问的互斥量
} Queue_t;

// 创建队列实现 - 改进版
QueueHandle_t xQueueCreate(uint32_t uxQueueLength, uint32_t uxItemSize) {
    // 参数有效性检查
    if (uxQueueLength == 0 || uxItemSize == 0) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 队列参数无效\n");
        return NULL;
    }
    
    // 分配队列控制块
    Queue_t *queue = (Queue_t*)system_malloc(sizeof(Queue_t));
    if (queue == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 队列控制块分配失败\n");
        return NULL;
    }
    
    // 分配队列缓冲区
    queue->buffer = (uint8_t*)system_malloc(uxQueueLength * uxItemSize);
    if (queue->buffer == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 队列缓冲区分配失败\n");
        system_free(queue);
        return NULL;
    }
    
    // 初始化队列参数
    queue->item_size = uxItemSize;
    queue->length = uxQueueLength;
    queue->items = 0;
    queue->head = 0;
    queue->tail = 0;
    
    // 创建互斥量用于保护队列访问
    queue->mutex = xSemaphoreCreateMutex();
    if (queue->mutex == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS: 队列互斥量创建失败\n");
        system_free(queue->buffer);
        system_free(queue);
        return NULL;
    }
    
    system_log(LOG_LEVEL_DEBUG, "RTOS: 队列创建成功，长度: %u, 元素大小: %u字节\n", 
               uxQueueLength, uxItemSize);
    return (QueueHandle_t)queue;
}

// 发送队列消息实现 - 改进版
BaseType_t xQueueSend(QueueHandle_t xQueue, const void* pvItemToQueue, TickType_t xTicksToWait) {
    if (xQueue == NULL || pvItemToQueue == NULL) {
        return pdFALSE;
    }
    
    Queue_t *queue = (Queue_t*)xQueue;
    uint32_t start_time = 0;
    
    // 记录开始时间，用于超时计算
    if (xTicksToWait > 0) {
        start_time = system_get_time_ms();
    }
    
    // 尝试发送消息，如果队列满则等待
    while (1) {
        // 获取互斥量访问队列
        if (xSemaphoreTake(queue->mutex, 0) == pdTRUE) {
            // 检查队列是否有空间
            if (queue->items < queue->length) {
                // 复制数据到队列
                memcpy(queue->buffer + (queue->tail * queue->item_size), 
                       pvItemToQueue, queue->item_size);
                
                // 更新队列尾索引
                queue->tail = (queue->tail + 1) % queue->length;
                queue->items++;
                
                // 释放互斥量
                xSemaphoreGive(queue->mutex);
                
                system_log(LOG_LEVEL_DEBUG, "RTOS: 队列消息发送成功，队列中元素: %u/%u\n", 
                           queue->items, queue->length);
                return pdTRUE;
            }
            
            // 释放互斥量
            xSemaphoreGive(queue->mutex);
        }
        
        // 检查是否超时
        if (xTicksToWait > 0 && 
            (system_get_time_ms() - start_time) >= xTicksToWait) {
            system_log(LOG_LEVEL_DEBUG, "RTOS: 队列发送超时\n");
            return pdFALSE;
        }
        
        // 让出CPU给其他任务
        vTaskSwitchContext();
        
        // 短暂延时
        system_delay(1);
    }
}

// 接收队列消息实现 - 改进版
BaseType_t xQueueReceive(QueueHandle_t xQueue, void* pvBuffer, TickType_t xTicksToWait) {
    if (xQueue == NULL || pvBuffer == NULL) {
        return pdFALSE;
    }
    
    Queue_t *queue = (Queue_t*)xQueue;
    uint32_t start_time = 0;
    
    // 记录开始时间，用于超时计算
    if (xTicksToWait > 0) {
        start_time = system_get_time_ms();
    }
    
    // 尝试接收消息，如果队列为空则等待
    while (1) {
        // 获取互斥量访问队列
        if (xSemaphoreTake(queue->mutex, 0) == pdTRUE) {
            // 检查队列是否有消息
            if (queue->items > 0) {
                // 从队列复制数据
                memcpy(pvBuffer, 
                       queue->buffer + (queue->head * queue->item_size), 
                       queue->item_size);
                
                // 更新队列头索引
                queue->head = (queue->head + 1) % queue->length;
                queue->items--;
                
                // 释放互斥量
                xSemaphoreGive(queue->mutex);
                
                system_log(LOG_LEVEL_DEBUG, "RTOS: 队列消息接收成功，队列中元素: %u/%u\n", 
                           queue->items, queue->length);
                return pdTRUE;
            }
            
            // 释放互斥量
            xSemaphoreGive(queue->mutex);
        }
        
        // 检查是否超时
        if (xTicksToWait > 0 && 
            (system_get_time_ms() - start_time) >= xTicksToWait) {
            system_log(LOG_LEVEL_DEBUG, "RTOS: 队列接收超时\n");
            return pdFALSE;
        }
        
        // 让出CPU给其他任务
        vTaskSwitchContext();
        
        // 短暂延时
        system_delay(1);
    }
}

// 钩子函数实现
#if (configUSE_IDLE_HOOK == 1)
void vApplicationIdleHook(void) {
    // 空闲任务钩子函数，可以在这里执行低优先级的后台任务
    system_log(LOG_LEVEL_DEBUG, "RTOS: 空闲任务运行中\n");
    // 短暂休眠，降低CPU使用率
    system_delay(5);
}
#endif

#if (configUSE_TICK_HOOK == 1)
void vApplicationTickHook(void) {
    // 系统节拍钩子函数，每过一个系统节拍就会调用
    static uint32_t tick_count = 0;
    tick_count++;
    if (tick_count % 1000 == 0) {
        system_log(LOG_LEVEL_INFO, "RTOS: 系统运行时间 %u 秒\n", tick_count / 1000);
    }
}
#endif

#if (configCHECK_FOR_STACK_OVERFLOW > 0)
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
    // 堆栈溢出检测钩子函数
    system_log(LOG_LEVEL_ERROR, "RTOS: 任务 %s 堆栈溢出!\n", pcTaskName);
    // 这里可以添加堆栈溢出的处理逻辑，如重启任务或系统
}
#endif

#if (configUSE_MALLOC_FAILED_HOOK == 1)
static void vApplicationMallocFailedHook(void) {
    // 内存分配失败钩子函数
    system_log(LOG_LEVEL_ERROR, "RTOS: 内存分配失败!\n");
    // 这里可以添加内存分配失败的处理逻辑
}
#endif

// 初始化任务堆栈（增强版）
// prvInitialiseNewTask 函数已被集成到 xTaskCreate 中，不再需要单独定义

// 任务上下文切换（增强版）
static void vTaskSwitchContext(void) {
    Task_t *pxNextTCB = NULL;
    UBaseType_t uxHighestPriority = tskIDLE_PRIORITY - 1;
    
    /* 禁用中断，保护任务切换过程 */
    platform_disable_interrupts();
    
    system_log(LOG_LEVEL_DEBUG, "RTOS: 执行上下文切换\n");
    
    // 查找最高优先级且可运行的任务
    for (uint8_t i = 0; i < task_count; i++) {
        if (!tasks[i].is_suspended && tasks[i].uxPriority > uxHighestPriority) {
            uxHighestPriority = tasks[i].uxPriority;
            pxNextTCB = &tasks[i];
        }
    }
    
    // 如果找到新任务且不是当前任务，则切换
    if (pxNextTCB != NULL && pxNextTCB != pxCurrentTCB) {
        Task_t *pxOldTCB = pxCurrentTCB;
        pxCurrentTCB = pxNextTCB;
        
        system_log(LOG_LEVEL_DEBUG, "RTOS: 切换到任务 %s (优先级: %u)\n", 
                   pxNextTCB->pcName, pxNextTCB->uxPriority);
        
        // 这里可以添加任务切换时的钩子函数调用
        // vTaskSwitchHook();
    }
}

// 任务调度函数 - 增强版
static void task_scheduler(void) {
    uint32_t current_time;
    uint32_t last_switch_time = system_get_time_ms();
    uint32_t last_tick_time = system_get_time_ms();
    
    scheduler_running = true;
    
    system_log(LOG_LEVEL_INFO, "RTOS: 调度器启动，任务数量: %u\n", task_count);
    
    // 选择第一个可运行的任务作为初始任务
    if (task_count > 0) {
        for (uint8_t i = 0; i < task_count; i++) {
            if (!tasks[i].is_suspended) {
                pxCurrentTCB = &tasks[i];
                system_log(LOG_LEVEL_DEBUG, "RTOS: 初始任务: %s\n", pxCurrentTCB->pcName);
                break;
            }
        }
    }
    
    while (scheduler_running) {
        current_time = system_get_time_ms();
        
        // 系统节拍处理
        if (current_time - last_tick_time >= (1000 / configTICK_RATE_HZ)) {
            last_tick_time = current_time;
            // 调用系统节拍钩子函数
            #if (configUSE_TICK_HOOK == 1)
            vApplicationTickHook();
            #endif
        }
        
        // 时间片轮转调度：如果启用了时间片且当前任务运行超过时间片，进行上下文切换
        if (configUSE_TIME_SLICING && 
            pxCurrentTCB && 
            (current_time - last_switch_time >= (1000 / configTICK_RATE_HZ))) {
            
            system_log(LOG_LEVEL_DEBUG, "RTOS: 时间片结束，执行上下文切换\n");
            vTaskSwitchContext();
            last_switch_time = current_time;
        }
        
        // 如果有当前任务且未挂起，则运行任务
        if (pxCurrentTCB && !pxCurrentTCB->is_suspended) {
            // 堆栈溢出检测
            #if (configCHECK_FOR_STACK_OVERFLOW > 1)
            // 检查堆栈使用情况，这里简化为记录堆栈使用统计
            system_log(LOG_LEVEL_DEBUG, "RTOS: 任务 %s 堆栈使用检查\n", pxCurrentTCB->pcName);
            #endif
            
            pxCurrentTCB->is_running = true;
            pxCurrentTCB->pxTaskCode(pxCurrentTCB->pvParameters);
            pxCurrentTCB->is_running = false;
            pxCurrentTCB->last_run_time = current_time;
            pxCurrentTCB->run_count++;
        } else {
            // 调用空闲任务钩子函数
            #if (configUSE_IDLE_HOOK == 1)
            vApplicationIdleHook();
            #endif
        }
        
        // 短暂延时，防止CPU占用过高
        system_delay(1);
    }
    
    system_log(LOG_LEVEL_INFO, "RTOS: 调度器停止\n");
}

// 启动调度器实现
void vTaskStartScheduler(void) {
    scheduler_running = true;
    
    // 运行调度器
    task_scheduler();
}

// 停止调度器实现
void vTaskEndScheduler(void) {
    scheduler_running = false;
}