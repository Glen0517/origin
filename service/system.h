/*
 * 系统服务接口定义
 */

#ifndef SYSTEM_H
#define SYSTEM_H

#include "../include/types.h"
#include "../platform/platform.h"

// 系统定时器配置
#define SYSTEM_TIMER_CHANNEL      TIMER_1    // 系统定时器通道
#define SYSTEM_TIMER_PRESCALER    71999      // 定时器预分频器（72MHz / 72000 = 1kHz）
#define SYSTEM_TIMER_AUTO_RELOAD  999        // 自动重载值（1kHz / 1000 = 1Hz中断）

// 系统日志级别
#define SYSTEM_LOG_LEVEL          CURRENT_LOG_LEVEL

// 系统任务优先级
typedef enum {
    TASK_PRIORITY_LOW,
    TASK_PRIORITY_MEDIUM,
    TASK_PRIORITY_HIGH,
    TASK_PRIORITY_REALTIME
} task_priority_t;

// 系统任务函数类型
typedef void (*task_func_t)(void *params);

// 系统任务结构体
typedef struct {
    const char *name;              // 任务名称
    task_func_t function;          // 任务函数
    void *params;                  // 任务参数
    uint32_t interval;             // 执行间隔(ms)
    task_priority_t priority;      // 任务优先级
    bool is_active;                // 是否激活
    uint32_t last_run_time;        // 上次运行时间
    uint32_t run_count;            // 运行计数
} system_task_t;

/**
 * @brief 初始化系统服务
 * @return 是否初始化成功
 */
bool system_init(void);

/**
 * @brief 系统延迟函数
 * @param ms 延迟时间(毫秒)
 */
void system_delay_ms(uint32_t ms);

/**
 * @brief 系统延迟函数（毫秒）
 * @param ms 延迟时间(毫秒)
 */
void system_delay(uint32_t ms);

/**
 * @brief 获取系统时间戳
 * @return 当前系统时间戳(毫秒)
 */
uint32_t system_get_time_ms(void);

/**
 * @brief 获取系统状态
 * @param status 系统状态结构体
 * @return 是否获取成功
 */
bool system_get_status(system_status_t *status);

/**
 * @brief 设置系统状态
 * @param state 系统状态
 * @return 是否设置成功
 */
bool system_set_state(system_state_t state);

/**
 * @brief 添加系统任务
 * @param task 任务结构体
 * @return 任务ID或错误码
 */
int32_t system_add_task(system_task_t *task);

/**
 * @brief 移除系统任务
 * @param task_id 任务ID
 * @return 是否移除成功
 */
bool system_remove_task(int32_t task_id);

/**
 * @brief 启动系统任务
 * @param task_id 任务ID
 * @return 是否启动成功
 */
bool system_start_task(int32_t task_id);

/**
 * @brief 停止系统任务
 * @param task_id 任务ID
 * @return 是否停止成功
 */
bool system_stop_task(int32_t task_id);

/**
 * @brief 系统任务调度器
 * @note 应在主循环中调用
 */
void system_task_scheduler(void);

/**
 * @brief 记录系统错误
 * @param error 错误代码
 * @param file 文件名称
 * @param line 行号
 */
void system_error(system_error_t error, const char *file, uint32_t line);

/**
 * @brief 系统重置
 * @param delay 延迟时间(毫秒)
 */
void system_reset(uint32_t delay);

/**
 * @brief 获取系统错误描述
 * @param error 错误代码
 * @return 错误描述字符串
 */
const char *system_get_error_string(system_error_t error);

/**
 * @brief 系统内存管理
 */
void *system_malloc(uint32_t size);
void system_free(void *ptr);
void system_memory_init(void);

/**
 * @brief 系统日志函数
 */
void system_log(log_level_t level, const char *format, ...);
void system_log_level(log_level_t level, const char *format, ...);

/**
 * @brief 系统更新函数
 */
void system_update(void);

/**
 * @brief 检查系统是否有错误
 * @return 有错误返回true，无错误返回false
 */
bool system_has_error(void);

/**
 * @brief 获取系统错误信息
 * @param error 错误信息结构体指针
 * @return 获取成功返回true，无更多错误返回false
 */
bool system_get_error(error_info_t *error);

// 硬件初始化函数声明
bool gpio_init_all(void);
bool timer_init_all(void);
bool i2c_init(uint8_t channel, uint8_t speed);
bool uart_init_all(void);

#endif // SYSTEM_H