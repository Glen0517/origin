/**
 * @file monitor.h
 * @brief 监控和诊断模块头文件
 * @details 提供系统监控和诊断功能，包括进程状态实时监控
 * @author AML Audio Team
 * @date 2026-01-22
 */

#ifndef __MONITOR_H__
#define __MONITOR_H__

#include "common_def.h"
#include "process.h"

/******************************************************************************************
 * 监控和诊断数据类型
 ******************************************************************************************/

/**
 * @brief 监控事件类型
 */
typedef enum {
    MONITOR_EVENT_PROCESS_START = 0,    // 进程启动
    MONITOR_EVENT_PROCESS_EXIT,         // 进程退出
    MONITOR_EVENT_PROCESS_CRASH,        // 进程崩溃
    MONITOR_EVENT_RESOURCE_WARNING,     // 资源警告
    MONITOR_EVENT_RESOURCE_CRITICAL,    // 资源临界
    MONITOR_EVENT_SYSTEM_ERROR,         // 系统错误
    MONITOR_EVENT_MAX
} MonitorEventType_t;

/**
 * @brief 监控事件结构体
 */
typedef struct {
    MonitorEventType_t type;            // 事件类型
    uint64_t timestamp;                 // 事件时间戳
    pid_t pid;                          // 进程ID
    int code;                           // 事件代码
    char message[256];                  // 事件消息
} MonitorEvent_t;

/**
 * @brief 系统状态结构体
 */
typedef struct {
    uint64_t uptime;                    // 系统运行时间
    int process_count;                  // 进程数量
    int thread_count;                   // 线程数量
    int cpu_usage;                      // CPU使用率
    int memory_usage;                   // 内存使用率
    int disk_usage;                     // 磁盘使用率
    int network_rx;                     // 网络接收速率
    int network_tx;                     // 网络发送速率
} SystemStatus_t;

/******************************************************************************************
 * 监控和诊断对外接口
 ******************************************************************************************/

/**
 * @brief 初始化监控和诊断模块
 * @return SUCCESS/FAILURE
 */
int monitor_init(void);

/**
 * @brief 反初始化监控和诊断模块
 * @return SUCCESS/FAILURE
 */
int monitor_deinit(void);

/**
 * @brief 开始监控
 * @param interval 监控间隔（毫秒）
 * @return SUCCESS/FAILURE
 */
int monitor_start(int interval);

/**
 * @brief 停止监控
 * @return SUCCESS/FAILURE
 */
int monitor_stop(void);

/**
 * @brief 获取系统状态
 * @param status 系统状态
 * @return SUCCESS/FAILURE
 */
int monitor_get_system_status(SystemStatus_t *status);

/**
 * @brief 获取进程状态
 * @param pid 进程ID
 * @param info 进程信息
 * @return SUCCESS/FAILURE
 */
int monitor_get_process_status(pid_t pid, ProcessInfo_t *info);

/**
 * @brief 记录监控事件
 * @param event 监控事件
 * @return SUCCESS/FAILURE
 */
int monitor_record_event(const MonitorEvent_t *event);

/**
 * @brief 注册事件回调函数
 * @param callback 回调函数
 * @param arg 回调参数
 * @return SUCCESS/FAILURE
 */
int monitor_register_callback(void (*callback)(const MonitorEvent_t *, void *), void *arg);

/**
 * @brief 取消注册事件回调函数
 * @return SUCCESS/FAILURE
 */
int monitor_unregister_callback(void);

/**
 * @brief 获取监控日志
 * @param buffer 日志缓冲区
 * @param max_len 缓冲区最大长度
 * @return 日志长度
 */
int monitor_get_log(char *buffer, int max_len);

/**
 * @brief 清空监控日志
 * @return SUCCESS/FAILURE
 */
int monitor_clear_log(void);

/**
 * @brief 触发诊断
 * @param level 诊断级别
 * @param output 诊断输出路径
 * @return SUCCESS/FAILURE
 */
int monitor_trigger_diagnostic(int level, const char *output);

#endif /* __MONITOR_H__ */