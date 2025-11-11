/*
 * 系统服务实现
 */

#include "system.h"
#include "../config/config.h"
#include "../include/types.h"
#include "../rtos/rtos_adapter.h"
#include "../rtos/FreeRTOSConfig.h"
#include "../platform/platform.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

// 系统错误码定义
#define SYSTEM_OK 0
#define SYSTEM_ERROR -1

// 最大任务数量
#define MAX_SYSTEM_TASKS     10

// 系统状态
static system_status_t system_status;
static system_task_t tasks[MAX_SYSTEM_TASKS];
static uint32_t tasks_count = 0;
static void* task_handles[MAX_SYSTEM_TASKS] = {NULL};
static bool system_initialized = false;
static uint32_t system_start_time = 0;

// 函数声明
const char *system_get_log_level_string(log_level_t level);

// 任务优先级处理将在调度器中直接实现

/**
 * @brief 初始化系统服务
 */
bool system_init(void) {
    // 初始化系统状态
    system_status.state = SYSTEM_STATE_INIT;
    system_status.uptime = 0;
    system_status.cpu_usage = 0;
    system_status.free_heap = 0;
    system_status.temperature = 0;
    system_status.last_error = SYSTEM_ERROR_NONE;
    system_status.error_count = 0;
    
    // 初始化任务列表
    for (uint8_t i = 0; i < MAX_SYSTEM_TASKS; i++) {
        tasks[i].name = NULL;
        tasks[i].function = NULL;
        tasks[i].params = NULL;
        tasks[i].interval = 0;
        tasks[i].priority = TASK_PRIORITY_MEDIUM;
        tasks[i].is_active = false;
        tasks[i].last_run_time = 0;
        tasks[i].run_count = 0;
        // 初始化任务状态
        task_handles[i] = NULL;
    }
    
    // 初始化平台
    platform_init_config_t platform_config = {
        .clock_source = PLATFORM_CLOCK_SOURCE_PLL,
        .target_freq_hz = SYSTEM_CLOCK_FREQ,
        .use_cache = true,
        .use_fpu = true
    };
    
    if (!platform_init(&platform_config)) {
        system_log(LOG_LEVEL_ERROR, "平台初始化失败\n");
        return false;
    }
    
    // 初始化内存管理
    system_memory_init();
    
    // 初始化RTOS
    if (!rtos_init()) {
        system_log(LOG_LEVEL_ERROR, "RTOS初始化失败\n");
        return false;
    }
    
    system_log(LOG_LEVEL_INFO, "系统服务初始化成功\n");
    
    system_initialized = true;
    return true;
}

/**
 * @brief 系统延迟函数
 */
void system_delay_ms(uint32_t ms) {
    // 使用平台抽象层的延时函数
    platform_delay_ms(ms);
}

/**
 * @brief 系统延迟函数（毫秒）
 * @param ms 延迟时间(毫秒)
 */
void system_delay(uint32_t ms) {
    // 调用system_delay_ms以保持一致性
    system_delay_ms(ms);
}

/**
 * @brief 获取系统时间戳
 */
uint32_t system_get_time_ms(void) {
    if (!system_initialized) {
        return 0;
    }
    
    // 使用平台抽象层的获取时间函数
    return platform_get_time_ms();
}

/**
 * @brief 获取系统状态
 */
bool system_get_status(system_status_t *status) {
    if (!system_initialized || status == NULL) {
        return false;
    }
    
    // 更新运行时间
    system_status.uptime = system_get_time_ms();
    
    // 复制状态
    *status = system_status;
    return true;
}

/**
 * @brief 设置系统状态
 */
bool system_set_state(system_state_t state) {
    if (!system_initialized) {
        return false;
    }
    
    system_status.state = state;
    return true;
}

/**
 * @brief 添加系统任务
 * @param task 任务结构体
 * @return 任务ID或错误码
 */
int32_t system_add_task(system_task_t *task) {
    // 检查系统是否已初始化
    if (!system_initialized) {
        system_log(LOG_LEVEL_ERROR, "系统未初始化\n");
        return SYSTEM_ERROR;
    }

    // 检查参数是否有效
    if (task == NULL || task->function == NULL) {
        system_log(LOG_LEVEL_ERROR, "无效的任务参数\n");
        return SYSTEM_ERROR;
    }

    // 检查任务数量是否超过上限
    if (tasks_count >= MAX_SYSTEM_TASKS) {
        system_log(LOG_LEVEL_ERROR, "任务数量已达上限\n");
        return SYSTEM_ERROR;
    }

    // 复制任务信息
    uint32_t task_id = tasks_count;
    tasks[task_id].name = task->name;
    tasks[task_id].function = task->function;
    tasks[task_id].params = task->params;
    tasks[task_id].interval = task->interval;
    tasks[task_id].priority = task->priority;
    tasks[task_id].is_active = true; // 初始为活动状态
    tasks[task_id].last_run_time = system_get_time_ms();
    tasks[task_id].run_count = 0;
    
    // 调用RTOS适配器创建真正的任务
    void *handle = rtos_create_task(task->name, 
                                   (task_func_t)task->function, 
                                   configMINIMAL_STACK_SIZE, 
                                   task->params, 
                                   task->priority, 
                                   NULL);
    
    if (handle == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS任务创建失败: %s\n", task->name);
        // 如果RTOS任务创建失败，清除任务信息
        memset(&tasks[task_id], 0, sizeof(system_task_t));
        return SYSTEM_ERROR;
    }
    
    // 保存任务句柄
    task_handles[task_id] = handle;
    tasks_count++;
    
    system_log(LOG_LEVEL_INFO, "任务已添加: %d, 名称: %s, 优先级: %d\n", 
               task_id, task->name, task->priority);
    return (int32_t)task_id; // 返回任务ID
}

/**
 * @brief 移除系统任务
 * @param task_id 任务ID
 * @return 是否移除成功
 */
bool system_remove_task(int32_t task_id) {
    // 检查参数有效性
    if (!system_initialized || task_id < 0 || task_id >= (int32_t)tasks_count) {
        system_log(LOG_LEVEL_ERROR, "无效的任务ID: %d\n", task_id);
        return false;
    }

    // 检查任务是否存在
    if (tasks[task_id].function == NULL) {
        system_log(LOG_LEVEL_WARNING, "任务不存在: %d\n", task_id);
        return false;
    }

    const char *task_name = tasks[task_id].name;
    
    // 调用RTOS适配器删除任务
    if (task_handles[task_id] != NULL) {
        rtos_delete_task(task_handles[task_id]);
        task_handles[task_id] = NULL;
    }
    
    // 清除任务信息
    memset(&tasks[task_id], 0, sizeof(system_task_t));
    
    // 如果是最后一个任务，可以减少tasks_count
    if (task_id == (int32_t)(tasks_count - 1)) {
        tasks_count--;
    }
    
    system_log(LOG_LEVEL_INFO, "任务已移除: %d, 名称: %s\n", task_id, task_name);
    return true;
}

/**
 * @brief 启动任务
 * @param task_id 任务ID
 * @return 是否启动成功
 */
bool system_start_task(int32_t task_id) {
    if (!system_initialized || task_id < 0 || task_id >= (int32_t)tasks_count) {
        return false;
    }
    
    // 使用RTOS恢复任务
    if (tasks[task_id].is_active && task_handles[task_id]) {
        rtos_resume_task(task_handles[task_id]);
    }
    
    tasks[task_id].is_active = true;
    return true;
}

/**
 * @brief 停止任务
 * @param task_id 任务ID
 * @return 是否停止成功
 */
bool system_stop_task(int32_t task_id) {
    if (!system_initialized || task_id < 0 || task_id >= (int32_t)tasks_count) {
        return false;
    }
    
    // 使用RTOS挂起任务
    if (tasks[task_id].is_active && task_handles[task_id]) {
        rtos_suspend_task(task_handles[task_id]);
    }
    
    tasks[task_id].is_active = false;
    return true;
}

/**
 * @brief 系统任务调度器
 * 在RTOS环境中，此函数不执行实际调度，调度由RTOS内核管理
 */
void system_task_scheduler(void) {
    // RTOS模式下此函数为空，调度由RTOS内核自动完成
    // 这里可以保留一些统计或监控功能
    system_update();
}

/**
 * @brief 记录系统错误
 */
void system_error(system_error_t error, const char *file, uint32_t line) {
    system_status.last_error = error;
    system_status.error_count++;
    
    // 输出错误日志
    system_log(LOG_LEVEL_ERROR, "ERROR: %s (%u) at %s:%u\n",
              system_get_error_string(error), 
              error, file, line);
    
    // 严重错误可以考虑重置系统
    if (error == SYSTEM_ERROR_SENSOR_ERROR || error == SYSTEM_ERROR_MEMORY_ALLOCATION) {
        system_log(LOG_LEVEL_FATAL, "Critical error detected, system reset in 1 second...\n");
        system_delay_ms(1000);
        system_reset(0);
    }
}

/**
 * @brief 系统重置
 */
void system_reset(uint32_t delay) {
    system_log(LOG_LEVEL_INFO, "系统将在%u毫秒后重置...\n", delay);
    
    if (delay > 0) {
        platform_delay_ms(delay);
    }
    
    // 使用平台抽象层的重置函数
    platform_reset(false);
}

/**
 * @brief 获取系统错误描述
 */
const char *system_get_error_string(system_error_t error) {
    static const char *error_strings[] = {
        "No error",
        "System uninitialized",
        "Null pointer",
        "Invalid parameter",
        "Memory error",
        "Task limit reached",
        "Task not found",
        "Resource busy",
        "Timeout",
        "Hardware error"
    };
    
    if (error >= sizeof(error_strings) / sizeof(error_strings[0])) {
        return "Unknown error";
    }
    
    return error_strings[error];
}

/**
 * @brief 系统内存管理
 */
void system_memory_init(void) {
    // 在嵌入式系统中，这里可能需要初始化自定义的内存分配器
    // 在PC仿真环境中，使用标准库的malloc/free即可
}

void *system_malloc(uint32_t size) {
    if (!system_initialized || size == 0) {
        return NULL;
    }
    
    void *ptr = malloc(size);
    if (ptr == NULL) {
        system_error(SYSTEM_ERROR_MEMORY_ALLOCATION, __FILE__, __LINE__);
    }
    
    return ptr;
}

void system_free(void *ptr) {
    if (!system_initialized || ptr == NULL) {
        return;
    }
    
    free(ptr);
}

/**
 * @brief 系统日志函数
 */
void system_log(log_level_t level, const char *format, ...) {
    if (!system_initialized || format == NULL) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    system_log_level(level, format, args);
    va_end(args);
}

void system_log_level(log_level_t level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    // 确保日志级别有效
    if (level < LOG_LEVEL_DEBUG || level > LOG_LEVEL_ERROR) {
        level = LOG_LEVEL_INFO;
    }
    
    // 在实际应用中，这里会根据配置输出到不同的目标（串口、文件等）
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    
    // 简单的控制台输出实现
    const char *level_str = system_get_log_level_string(level);
    printf("[%s] %s", level_str, buffer);
    
    va_end(args);
}

/**
 * @brief 更新系统状态
 */
void system_update(void) {
    // 更新系统运行时间
    system_status.uptime = system_get_time_ms() - system_start_time;
    
    // 模拟CPU使用率计算
    system_status.cpu_usage = 50;  // 实际应用中应该基于实际运行时间计算
    
    // 模拟内存使用情况
    system_status.free_heap = 8 * 1024;  // 8KB空闲内存
    system_status.used_heap = 2 * 1024;  // 2KB已用内存
    system_status.total_heap = 10 * 1024;  // 10KB总内存
}

/**
 * @brief 检查系统是否有错误
 */
bool system_has_error(void) {
    // 简单实现：检查是否有错误记录
    return system_status.error_count > 0;
}

/**
 * @brief 获取系统错误信息
 */
bool system_get_error(error_info_t *error) {
    // 如果提供了错误指针，设置错误信息并返回true
    if (error != NULL) {
        error->code = system_status.last_error;
        error->message = system_get_error_string(system_status.last_error);
    }
    return true;
}

/**
 * @brief 获取日志级别字符串
 */
const char *system_get_log_level_string(log_level_t level) {
    // 根据日志级别返回对应的字符串
    switch (level) {
        case LOG_LEVEL_DEBUG:
            return "DEBUG";
        case LOG_LEVEL_INFO:
            return "INFO";
        case LOG_LEVEL_WARNING:
            return "WARNING";
        case LOG_LEVEL_ERROR:
            return "ERROR";
        case LOG_LEVEL_FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
    }
}