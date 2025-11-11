/*
 * RTOS功能测试文件
 * 用于验证FreeRTOS模拟实现的基本功能
 */

#include "service/system.h"
#include "rtos/rtos_adapter.h"
#include "rtos/FreeRTOSConfig.h"
#include "include/types.h"
#include <stdio.h>

// 定义portMAX_DELAY常量（如果FreeRTOSConfig.h中未定义）
#ifndef portMAX_DELAY
#define portMAX_DELAY ((TickType_t)0xffffffffUL)
#endif

// 测试任务句柄
static TaskHandle_t test_task1_handle = NULL;
static TaskHandle_t test_task2_handle = NULL;
static TaskHandle_t test_task3_handle = NULL;

// 互斥量和队列句柄
static SemaphoreHandle_t test_mutex = NULL;
static QueueHandle_t test_queue = NULL;

// 共享计数器
static volatile uint32_t shared_counter = 0;
static volatile uint32_t task1_runs = 0;
static volatile uint32_t task2_runs = 0;
static volatile uint32_t task3_runs = 0;

/**
 * @brief 高优先级测试任务
 */
void high_priority_task(void *params) {
    (void)params;  // 避免未使用参数警告
    uint32_t count = 0;
    
    while (1) {
        // 获取互斥量
        if (xSemaphoreTake(test_mutex, portMAX_DELAY) == pdTRUE) {
            // 访问共享资源
            shared_counter++;
            task1_runs++;
            
            system_log(LOG_LEVEL_DEBUG, "高优先级任务运行: count=%lu, shared_counter=%lu\n", 
                       count++, shared_counter);
            
            // 发送消息到队列
            uint32_t message = shared_counter;
            if (xQueueSend(test_queue, &message, 100) != pdTRUE) {
                system_log(LOG_LEVEL_WARNING, "高优先级任务队列发送失败\n");
            }
            
            // 释放互斥量
            xSemaphoreGive(test_mutex);
        }
        
        // 延时100ms，故意让低优先级任务有机会运行
        vTaskDelay(100);
    }
}

/**
 * @brief 中优先级测试任务
 */
void medium_priority_task(void *params) {
    (void)params;  // 避免未使用参数警告
    uint32_t count = 0;
    
    while (1) {
        // 尝试获取互斥量，带超时
        if (xSemaphoreTake(test_mutex, 50) == pdTRUE) {
            // 访问共享资源
            shared_counter++;
            task2_runs++;
            
            system_log(LOG_LEVEL_DEBUG, "中优先级任务运行: count=%lu, shared_counter=%lu\n", 
                       count++, shared_counter);
            
            // 释放互斥量
            xSemaphoreGive(test_mutex);
        } else {
            system_log(LOG_LEVEL_DEBUG, "中优先级任务获取互斥量超时\n");
        }
        
        // 延时50ms
        vTaskDelay(50);
    }
}

/**
 * @brief 低优先级测试任务
 */
void low_priority_task(void *params) {
    (void)params;  // 避免未使用参数警告
    uint32_t count = 0;
    uint32_t received_message = 0;
    
    while (1) {
        // 接收队列消息
        if (xQueueReceive(test_queue, &received_message, 200) == pdTRUE) {
            system_log(LOG_LEVEL_DEBUG, "低优先级任务收到消息: %lu\n", received_message);
        }
        
        // 尝试获取互斥量，不带超时
        if (xSemaphoreTake(test_mutex, 0) == pdTRUE) {
            // 访问共享资源
            shared_counter++;
            task3_runs++;
            
            system_log(LOG_LEVEL_DEBUG, "低优先级任务运行: count=%lu, shared_counter=%lu\n", 
                       count++, shared_counter);
            
            // 释放互斥量
            xSemaphoreGive(test_mutex);
        }
        
        // 统计运行情况
        if (count % 10 == 0) {
            system_log(LOG_LEVEL_INFO, "运行统计 - 任务1: %lu, 任务2: %lu, 任务3: %lu\n", 
                      task1_runs, task2_runs, task3_runs);
        }
        
        // 延时20ms
        vTaskDelay(20);
    }
}

/**
 * @brief 运行RTOS测试
 */
bool run_rtos_tests(void) {
    system_log(LOG_LEVEL_INFO, "开始RTOS功能测试...\n");
    
    // 创建互斥量
    test_mutex = xSemaphoreCreateMutex();
    if (test_mutex == NULL) {
        system_log(LOG_LEVEL_ERROR, "互斥量创建失败\n");
        return false;
    }
    system_log(LOG_LEVEL_INFO, "互斥量创建成功\n");
    
    // 创建队列
    test_queue = xQueueCreate(5, sizeof(uint32_t));
    if (test_queue == NULL) {
        system_log(LOG_LEVEL_ERROR, "队列创建失败\n");
        return false;
    }
    system_log(LOG_LEVEL_INFO, "队列创建成功\n");
    
    // 创建高优先级任务
    test_task1_handle = rtos_create_task(
        "HighPriorityTask",
        high_priority_task,
        configMINIMAL_STACK_SIZE,
        NULL,
        TASK_PRIORITY_HIGH,
        NULL
    );
    
    // 创建中优先级任务
    test_task2_handle = rtos_create_task(
        "MediumPriorityTask",
        medium_priority_task,
        configMINIMAL_STACK_SIZE,
        NULL,
        TASK_PRIORITY_MEDIUM,
        NULL
    );
    
    // 创建低优先级任务
    test_task3_handle = rtos_create_task(
        "LowPriorityTask",
        low_priority_task,
        configMINIMAL_STACK_SIZE,
        NULL,
        TASK_PRIORITY_LOW,
        NULL
    );
    
    if (test_task1_handle == NULL || test_task2_handle == NULL || test_task3_handle == NULL) {
        system_log(LOG_LEVEL_ERROR, "任务创建失败\n");
        return false;
    }
    
    system_log(LOG_LEVEL_INFO, "所有测试任务创建成功，开始运行...\n");
    system_log(LOG_LEVEL_INFO, "测试将运行30秒，请观察任务调度情况...\n");
    
    // 等待30秒后停止测试
    system_delay(30000);
    
    // 删除测试任务
    if (test_task1_handle != NULL) {
        vTaskDelete(test_task1_handle);
    }
    if (test_task2_handle != NULL) {
        vTaskDelete(test_task2_handle);
    }
    if (test_task3_handle != NULL) {
        vTaskDelete(test_task3_handle);
    }
    
    // 删除互斥量和队列
    // 在实际FreeRTOS中，需要调用相应的删除函数
    // 这里我们只设置为NULL
    test_mutex = NULL;
    test_queue = NULL;
    
    system_log(LOG_LEVEL_INFO, "RTOS测试完成！\n");
    system_log(LOG_LEVEL_INFO, "最终运行统计 - 任务1: %lu, 任务2: %lu, 任务3: %lu, 共享计数器: %lu\n", 
              task1_runs, task2_runs, task3_runs, shared_counter);
    
    // 验证测试结果
    if (task1_runs > 0 && task2_runs > 0 && task3_runs > 0) {
        system_log(LOG_LEVEL_INFO, "测试通过：所有任务都成功运行！\n");
        return true;
    } else {
        system_log(LOG_LEVEL_ERROR, "测试失败：某些任务没有运行！\n");
        return false;
    }
}

/**
 * @brief 主测试函数
 */
int main_test(void) {
    // 初始化系统
    if (!system_init()) {
        printf("系统初始化失败！\n");
        return -1;
    }
    
    printf("系统初始化成功！\n");
    
    // 运行RTOS测试
    bool test_result = run_rtos_tests();
    
    printf("RTOS测试%s！\n", test_result ? "通过" : "失败");
    
    return test_result ? 0 : -1;
}