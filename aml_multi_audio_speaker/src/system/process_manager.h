#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

// 包含必要的头文件
#include <stdint.h>
#include <stdbool.h>

// 进程名称常量
#define PROCESS_NAME_STORAGE "storage"
#define PROCESS_NAME_NETWORK "network"
#define PROCESS_NAME_AUDIO "audio"
#define PROCESS_NAME_SYSTEM "system"
#define PROCESS_NAME_REMOTE_CONTROL "remote_control"

// 类型定义
typedef int pid_t;

// 函数声明
int process_manager_init(void);
int process_manager_deinit(void);
int process_manager_register_process(const char *name, pid_t (*create_func)(void), int (*terminate_func)(void));
int process_manager_start_process(const char *name);
int process_manager_stop_process(const char *name);
int process_manager_start_all_processes(void);
int process_manager_stop_all_processes(void);
pid_t process_manager_get_process_pid(const char *name);
bool process_manager_is_process_running(const char *name);
int process_manager_monitor_processes(void);

#endif /* PROCESS_MANAGER_H */
