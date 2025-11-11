/*
 * FreeRTOS配置文件 - 增强版
 * 包含所有必要的RTOS配置参数
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

// 时钟频率配置 - 根据实际硬件设置
#define configCPU_CLOCK_HZ                    16000000

// 优先级配置 - 增加优先级数量以支持更多任务
#define configMAX_PRIORITIES                  10

// 任务堆栈大小（字节） - 增加最小堆栈大小以确保稳定性
#define configMINIMAL_STACK_SIZE              256

// 系统节拍时钟配置 - 1ms节拍
#define configTICK_RATE_HZ                    1000

// 任务名称最大长度
#define configMAX_TASK_NAME_LEN               20

// 内存管理配置 - 增加堆大小以支持更多任务和资源
#define configTOTAL_HEAP_SIZE                 32768

// 系统时钟节拍配置 - 暂不使用低功耗模式
#define configUSE_TICKLESS_IDLE               0

// 调试支持 - 启用跟踪功能
#define configUSE_TRACE_FACILITY              1

// 空闲任务钩子函数 - 启用
#define configUSE_IDLE_HOOK                   1

// 系统节拍钩子函数 - 启用
#define configUSE_TICK_HOOK                   1

// 协程支持 - 禁用（现代应用通常不需要）
#define configUSE_CO_ROUTINES                 0

// 队列配置 - 启用队列集
#define configUSE_QUEUE_SETS                  1

// 计数信号量 - 启用
#define configUSE_COUNTING_SEMAPHORES         1

// 互斥量递归获取 - 启用
#define configUSE_RECURSIVE_MUTEXES           1

// 互斥量优先级继承 - 启用
#define configUSE_MUTEXES                     1

// 静态分配内存 - 启用
#define configSUPPORT_STATIC_ALLOCATION       1

// 动态分配内存 - 启用
#define configSUPPORT_DYNAMIC_ALLOCATION      1

// 栈溢出检查 - 使用方法2（更严格的检查）
#define configCHECK_FOR_STACK_OVERFLOW        2

// 任务通知 - 启用
#define configUSE_TASK_NOTIFICATIONS          1

// 任务通知数组大小
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 1

// 调度器锁定 - 禁用端口优化的任务选择
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0

// 应用程序钩子函数 - 启用所有必要的函数
#define INCLUDE_vTaskPrioritySet              1
#define INCLUDE_uxTaskPriorityGet             1
#define INCLUDE_vTaskDelete                   1
#define INCLUDE_vTaskCleanUpResources         1
#define INCLUDE_vTaskSuspend                  1
#define INCLUDE_vTaskDelayUntil               1
#define INCLUDE_vTaskDelay                    1
#define INCLUDE_xTaskGetSchedulerState        1
#define INCLUDE_xTaskGetCurrentTaskHandle     1
#define INCLUDE_uxTaskGetStackHighWaterMark   1
#define INCLUDE_xTaskGetIdleTaskHandle        1
#define INCLUDE_xTimerGetTimerDaemonTaskHandle 1

// 钩子函数声明
void vApplicationIdleHook(void);
void vApplicationTickHook(void);
void vApplicationMallocFailedHook(void);
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);

// 断言宏定义 - 增强的断言处理
#define configASSERT( x )                     \
    if( ( x ) == 0 ) {                        \
        taskDISABLE_INTERRUPTS();             \
        while(1) {                           \
            /* 可以添加LED闪烁或其他调试指示 */ \
        }                                    \
    }

// 时间片轮转调度 - 启用
#define configUSE_PREEMPTION                  1
#define configUSE_TIME_SLICING                1
#define configMAX_TASK_UNIQUE_NAME_LEN        configMAX_TASK_NAME_LEN

// 任务优先级继承深度
#define configMAX_SYSCALL_INTERRUPT_PRIORITY  5

// 空闲任务堆栈大小
#define configIDLE_TASK_STACK_SIZE            configMINIMAL_STACK_SIZE

#endif /* FREERTOS_CONFIG_H */