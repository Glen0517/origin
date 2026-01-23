#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

// 包含必要的头文件
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// 进程名称常量
#define PROCESS_NAME_STORAGE "storage"
#define PROCESS_NAME_NETWORK "network"
#define PROCESS_NAME_AUDIO "audio"
#define PROCESS_NAME_SYSTEM "system"
#define PROCESS_NAME_REMOTE_CONTROL "remote_control"

// 类型定义
typedef int pid_t;

// 进程状态定义
typedef enum {
    PROCESS_STATUS_IDLE = 0,        // 未运行
    PROCESS_STATUS_RUNNING,         // 运行中
    PROCESS_STATUS_STARTING,        // 启动中
    PROCESS_STATUS_STOPPING,        // 停止中
    PROCESS_STATUS_CRASHED,         // 崩溃
    PROCESS_STATUS_MAX
} ProcessStatus_t;

// 进程优先级定义
typedef enum {
    PROCESS_PRIORITY_LOW = 0,       // 低优先级
    PROCESS_PRIORITY_NORMAL,        // 正常优先级
    PROCESS_PRIORITY_HIGH,          // 高优先级
    PROCESS_PRIORITY_MAX
} ProcessPriority_t;

// 进程主函数类型
typedef void (*ProcessMainFunc)(void *arg);

// 进程通信消息类型
typedef struct {
    uint32_t msg_type;              // 消息类型
    uint32_t msg_size;              // 消息大小
    void *msg_data;                 // 消息数据
} ProcessMsg_t;

// 进程资源限制结构体
typedef struct {
    int max_cpu_usage;              // 最大CPU使用率（百分比）
    int max_memory;                 // 最大内存使用量（MB）
    int max_file_descriptors;       // 最大文件描述符数量
} ProcessResourceLimits_t;

// 进程信息结构体（扩展）
typedef struct ProcessInfo_t {
    char name[64];                  // 进程名称
    ProcessMainFunc main_func;      // 进程主函数
    void *main_arg;                 // 进程主函数参数
    int (*terminate_func)(void);    // 进程终止函数
    pid_t pid;                      // 进程ID
    ProcessStatus_t status;         // 进程状态
    ProcessPriority_t priority;      // 进程优先级
    uint32_t start_time;            // 启动时间
    uint32_t restart_count;         // 重启次数
    bool auto_restart;              // 是否自动重启
    char *dependencies[10];         // 依赖的进程名称
    int dependency_count;           // 依赖进程数量
    int msg_pipe[2];                // 进程间通信管道
    ProcessResourceLimits_t limits; // 资源限制
    struct ProcessInfo_t *next;     // 指向下一个进程
} ProcessInfo_t;

// 函数声明
int process_manager_init(void);
int process_manager_deinit(void);
int process_manager_register_process(const char *name, ProcessMainFunc main_func, void *main_arg, int (*terminate_func)(void));
int process_manager_set_process_priority(const char *name, ProcessPriority_t priority);
int process_manager_set_process_auto_restart(const char *name, bool auto_restart);
int process_manager_add_process_dependency(const char *name, const char *dependency);
int process_manager_start_process(const char *name);
int process_manager_stop_process(const char *name);
int process_manager_start_all_processes(void);
int process_manager_stop_all_processes(void);
pid_t process_manager_get_process_pid(const char *name);
ProcessStatus_t process_manager_get_process_status(const char *name);
bool process_manager_is_process_running(const char *name);
int process_manager_monitor_processes(void);
int process_manager_send_message(const char *name, uint32_t msg_type, void *msg_data, size_t msg_size);
int process_manager_broadcast_message(uint32_t msg_type, void *msg_data, size_t msg_size);
int process_manager_set_process_resource_limits(const char *name, ProcessResourceLimits_t *limits);
int process_manager_get_process_resource_usage(const char *name, int *cpu_usage, int *memory_usage);
int process_manager_kill_process(const char *name, int signal);

#endif /* PROCESS_MANAGER_H */
