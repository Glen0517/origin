/**
 * @file process.h
 * @brief 进程管理模块
 * @details 提供进程的创建、管理和通信功能
 * @author AML Audio Team
 * @date 2026-01-22
 */

#ifndef __PROCESS_H__
#define __PROCESS_H__

#include "common_def.h"

/**
 * @brief 进程类型
 */
typedef enum {
    PROCESS_TYPE_AUDIO = 0,      // 音频处理进程
    PROCESS_TYPE_NETWORK = 1,     // 网络服务进程
    PROCESS_TYPE_STORAGE = 2,     // 存储管理进程
    PROCESS_TYPE_SYSTEM = 3,      // 系统管理进程
    PROCESS_TYPE_MAX
} ProcessType_t;

/**
 * @brief 进程状态
 */
typedef enum {
    PROCESS_STATUS_IDLE = 0,      // 空闲
    PROCESS_STATUS_RUNNING = 1,   // 运行中
    PROCESS_STATUS_PAUSED = 2,    // 暂停
    PROCESS_STATUS_ERROR = 3,     // 错误
    PROCESS_STATUS_EXITED = 4     // 已退出
} ProcessStatus_t;

/**
 * @brief 进程信息
 */
typedef struct {
    ProcessType_t type;           // 进程类型
    pid_t pid;                    // 进程ID
    ProcessStatus_t status;       // 进程状态
    char name[32];                // 进程名称
    int priority;                 // 进程优先级
    int cpu_usage;                // CPU使用率
    int mem_usage;                // 内存使用率
    uint32_t start_time;          // 启动时间
    uint32_t uptime;              // 运行时间
} ProcessInfo_t;

/**
 * @brief 进程管理初始化
 * @return SUCCESS/FAILURE
 */
int process_init(void);

/**
 * @brief 进程管理反初始化
 * @return SUCCESS/FAILURE
 */
int process_deinit(void);

/**
 * @brief 创建进程
 * @param type 进程类型
 * @param name 进程名称
 * @param priority 进程优先级
 * @return 进程ID，失败返回-1
 */
pid_t process_create(ProcessType_t type, const char *name, int priority);

/**
 * @brief 终止进程
 * @param pid 进程ID
 * @return SUCCESS/FAILURE
 */
int process_terminate(pid_t pid);

/**
 * @brief 获取进程信息
 * @param pid 进程ID
 * @param info 进程信息
 * @return SUCCESS/FAILURE
 */
int process_get_info(pid_t pid, ProcessInfo_t *info);

/**
 * @brief 获取所有进程信息
 * @param infos 进程信息数组
 * @param max_count 最大进程数
 * @param count 实际进程数
 * @return SUCCESS/FAILURE
 */
int process_get_all_info(ProcessInfo_t *infos, int max_count, int *count);

/**
 * @brief 发送消息到进程
 * @param pid 进程ID
 * @param msg 消息内容
 * @param msg_len 消息长度
 * @return SUCCESS/FAILURE
 */
int process_send_message(pid_t pid, const void *msg, int msg_len);

/**
 * @brief 接收来自进程的消息
 * @param pid 进程ID
 * @param msg 消息缓冲区
 * @param msg_len 消息长度
 * @return 实际读取的消息长度，失败返回-1
 */
int process_receive_message(pid_t pid, void *msg, int msg_len);

/**
 * @brief 检查进程是否运行
 * @param pid 进程ID
 * @return true表示运行中，false表示未运行
 */
bool process_is_running(pid_t pid);

/**
 * @brief 设置进程优先级
 * @param pid 进程ID
 * @param priority 进程优先级
 * @return SUCCESS/FAILURE
 */
int process_set_priority(pid_t pid, int priority);

/**
 * @brief 获取进程CPU使用率
 * @param pid 进程ID
 * @return CPU使用率，失败返回-1
 */
int process_get_cpu_usage(pid_t pid);

/**
 * @brief 获取进程内存使用率
 * @param pid 进程ID
 * @return 内存使用率，失败返回-1
 */
int process_get_memory_usage(pid_t pid);

#endif /* __PROCESS_H__ */
