/**
 * @file process_manager.c
 * @brief 进程管理模块实现
 * @details 统一管理所有进程的创建、销毁和通信
 * @author AML Audio Team
 * @date 2026-01-22
 */

#include "./process_manager.h"
#include "logger.h"
#include "../lib/flac/common_def.h"

#ifdef _WIN32
// Windows 特定头文件
#include <windows.h>
#include <process.h>
#else
// Unix 特定头文件
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#endif

#include <time.h>
#include <string.h>
#include <stdlib.h>

// 包含各个模块的头文件
#include "../lib/flac/storage.h"
#include "../lib/flac/wifi_media.h"
#include "../lib/flac/audio_core.h"
#include "../lib/flac/system.h"
#include "../remote_control/remote_control.h"

/******************************************************************************************
 * 各个进程的main_func实现
 ******************************************************************************************/

/**
 * @brief 存储进程主函数
 * @param arg 进程参数
 */
static void storage_process_main(void *arg) {
    LOG_INFO("Storage process started");
    
    // 初始化存储系统
    LOG_INFO("Initializing storage system...");
    // 这里可以添加实际的存储系统初始化代码
    
    // 主循环
    while (1) {
        // 检查存储设备状态
        LOG_DEBUG("Checking storage devices...");
        // 这里可以添加实际的存储设备检查代码
        
        // 短暂休眠
        usleep(1000000); // 1秒
    }
}

/**
 * @brief 网络进程主函数
 * @param arg 进程参数
 */
static void network_process_main(void *arg) {
    LOG_INFO("Network process started");
    
    // 初始化网络系统
    LOG_INFO("Initializing network system...");
    // 这里可以添加实际的网络系统初始化代码
    
    // 主循环
    while (1) {
        // 检查网络连接状态
        LOG_DEBUG("Checking network connections...");
        // 这里可以添加实际的网络连接检查代码
        
        // 短暂休眠
        usleep(1000000); // 1秒
    }
}

/**
 * @brief 音频进程主函数
 * @param arg 进程参数
 */
static void audio_process_main(void *arg) {
    LOG_INFO("Audio process started");
    
    // 初始化音频系统
    LOG_INFO("Initializing audio system...");
    // 这里可以添加实际的音频系统初始化代码
    
    // 主循环
    while (1) {
        // 处理音频数据
        LOG_DEBUG("Processing audio data...");
        // 这里可以添加实际的音频数据处理代码
        
        // 短暂休眠
        usleep(500000); // 0.5秒
    }
}

/**
 * @brief 系统进程主函数
 * @param arg 进程参数
 */
static void system_process_main(void *arg) {
    LOG_INFO("System process started");
    
    // 初始化系统
    LOG_INFO("Initializing system...");
    // 这里可以添加实际的系统初始化代码
    
    // 主循环
    while (1) {
        // 检查系统状态
        LOG_DEBUG("Checking system status...");
        // 这里可以添加实际的系统状态检查代码
        
        // 短暂休眠
        usleep(2000000); // 2秒
    }
}

/**
 * @brief 远程控制进程主函数
 * @param arg 进程参数
 */
static void remote_control_process_main_impl(void *arg) {
    LOG_INFO("Remote control process started");
    
    // 初始化远程控制系统
    LOG_INFO("Initializing remote control system...");
    // 这里可以添加实际的远程控制系统初始化代码
    
    // 主循环
    while (1) {
        // 处理遥控器事件
        LOG_DEBUG("Processing remote control events...");
        // 这里可以添加实际的遥控器事件处理代码
        
        // 短暂休眠
        usleep(100000); // 0.1秒
    }
}

/**
 * @brief 各个进程的terminate_func实现
 ******************************************************************************************/

/**
 * @brief 存储进程终止函数
 * @return SUCCESS/FAILURE
 */
static int storage_process_terminate(void) {
    LOG_INFO("Terminating storage process...");
    // 这里可以添加实际的存储进程终止代码
    return SUCCESS;
}

/**
 * @brief 网络进程终止函数
 * @return SUCCESS/FAILURE
 */
static int network_process_terminate(void) {
    LOG_INFO("Terminating network process...");
    // 这里可以添加实际的网络进程终止代码
    return SUCCESS;
}

/**
 * @brief 音频进程终止函数
 * @return SUCCESS/FAILURE
 */
static int audio_process_terminate(void) {
    LOG_INFO("Terminating audio process...");
    // 这里可以添加实际的音频进程终止代码
    return SUCCESS;
}

/**
 * @brief 系统进程终止函数
 * @return SUCCESS/FAILURE
 */
static int system_process_terminate(void) {
    LOG_INFO("Terminating system process...");
    // 这里可以添加实际的系统进程终止代码
    return SUCCESS;
}

/**
 * @brief 远程控制进程终止函数
 * @return SUCCESS/FAILURE
 */
static int remote_control_process_terminate(void) {
    LOG_INFO("Terminating remote control process...");
    // 这里可以添加实际的远程控制进程终止代码
    return SUCCESS;
}

/******************************************************************************************
 * 进程管理模块全局变量
 ******************************************************************************************/

static ProcessInfo_t *g_process_list = NULL;
static pthread_mutex_t g_process_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_process_manager_init = false;

/******************************************************************************************
 * 进程管理模块内部函数
 ******************************************************************************************/

/**
 * @brief 查找进程信息
 * @param name 进程名称
 * @return 进程信息指针，未找到返回NULL
 */
static ProcessInfo_t *find_process(const char *name) {
    if (!name) {
        return NULL;
    }
    
    ProcessInfo_t *current = g_process_list;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

/**
 * @brief 添加进程信息
 * @param name 进程名称
 * @param main_func 进程主函数
 * @param main_arg 进程主函数参数
 * @param terminate_func 进程终止函数
 * @return 添加结果：0表示成功，非0表示失败
 */
static int add_process(const char *name, ProcessMainFunc main_func, void *main_arg, int (*terminate_func)(void)) {
    if (!name) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    // 检查进程是否已存在
    if (find_process(name)) {
        LOG_WARN("Process already exists: %s", name);
        return -1;
    }
    
    // 创建进程信息结构体
    ProcessInfo_t *process = (ProcessInfo_t *)malloc(sizeof(ProcessInfo_t));
    if (!process) {
        LOG_ERROR("Failed to allocate memory for process info");
        return -1;
    }
    
    // 初始化进程信息
    memset(process, 0, sizeof(ProcessInfo_t));
    strncpy(process->name, name, sizeof(process->name) - 1);
    process->main_func = main_func;
    process->main_arg = main_arg;
    process->terminate_func = terminate_func;
    process->pid = -1;
    process->status = PROCESS_STATUS_IDLE;
    process->priority = PROCESS_PRIORITY_NORMAL;
    process->start_time = 0;
    process->restart_count = 0;
    process->auto_restart = true;
    process->dependency_count = 0;
    process->msg_pipe[0] = -1;
    process->msg_pipe[1] = -1;
    // 初始化资源限制
    process->limits.max_cpu_usage = 100;  // 默认无限制
    process->limits.max_memory = 0;      // 默认无限制
    process->limits.max_file_descriptors = 1024;  // 默认1024
    process->next = NULL;
    
    // 初始化依赖数组
    for (int i = 0; i < 10; i++) {
        process->dependencies[i] = NULL;
    }
    
    // 添加到进程列表
    if (!g_process_list) {
        g_process_list = process;
    } else {
        ProcessInfo_t *current = g_process_list;
        while (current->next) {
            current = current->next;
        }
        current->next = process;
    }
    
    LOG_INFO("Added process: %s", name);
    return 0;
}

/**
 * @brief 移除进程信息
 * @param name 进程名称
 * @return 移除结果：0表示成功，非0表示失败
 */
static int remove_process(const char *name) {
    if (!name) {
        return -1;
    }
    
    ProcessInfo_t *prev = NULL;
    ProcessInfo_t *current = g_process_list;
    
    while (current) {
        if (strcmp(current->name, name) == 0) {
            // 从列表中移除
            if (prev) {
                prev->next = current->next;
            } else {
                g_process_list = current->next;
            }
            
            // 关闭管道
            if (current->msg_pipe[0] != -1) {
                close(current->msg_pipe[0]);
            }
            if (current->msg_pipe[1] != -1) {
                close(current->msg_pipe[1]);
            }
            
            // 释放依赖名称
            for (int i = 0; i < current->dependency_count; i++) {
                if (current->dependencies[i]) {
                    free(current->dependencies[i]);
                }
            }
            
            // 释放内存
            free(current);
            LOG_INFO("Removed process: %s", name);
            return 0;
        }
        
        prev = current;
        current = current->next;
    }
    
    LOG_WARN("Process not found: %s", name);
    return -1;
}

/**
 * @brief 检查进程是否运行
 * @param pid 进程ID
 * @return 运行状态：true表示运行，false表示未运行
 */
static bool is_process_running(pid_t pid) {
    if (pid == -1) {
        return false;
    }
    
#ifdef _WIN32
    // Windows 平台：使用GetExitCodeProcess检查进程状态
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess == NULL) {
        return false;
    }
    
    DWORD exitCode;
    if (!GetExitCodeProcess(hProcess, &exitCode)) {
        CloseHandle(hProcess);
        return false;
    }
    
    CloseHandle(hProcess);
    return (exitCode == STILL_ACTIVE);
#else
    // Unix 平台：使用kill(0)检查进程是否存在
    return (kill(pid, 0) == 0);
#endif
}

/**
 * @brief 创建进程（跨平台实现）
 * @param process 进程信息
 * @return 进程ID，失败返回-1
 */
static pid_t create_process(ProcessInfo_t *process) {
    if (!process) {
        return -1;
    }
    
    // 创建进程间通信管道
    if (pipe(process->msg_pipe) == -1) {
        LOG_ERROR("Failed to create message pipe");
        return -1;
    }
    
#ifdef _WIN32
    // Windows 平台：使用CreateThread创建线程模拟进程
    unsigned int thread_id;
    HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, (unsigned int (__stdcall *)(void *))process->main_func, process->main_arg, 0, &thread_id);
    if (hThread == NULL) {
        LOG_ERROR("Failed to create process (thread) on Windows");
        close(process->msg_pipe[0]);
        close(process->msg_pipe[1]);
        process->msg_pipe[0] = -1;
        process->msg_pipe[1] = -1;
        return -1;
    }
    
    // 设置线程优先级
    switch (process->priority) {
        case PROCESS_PRIORITY_HIGH:
            SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST);
            break;
        case PROCESS_PRIORITY_LOW:
            SetThreadPriority(hThread, THREAD_PRIORITY_LOWEST);
            break;
        default:
            SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);
            break;
    }
    
    // 不关闭线程句柄，以便后续可以终止线程
    return (pid_t)thread_id;
#else
    // Unix 平台：使用fork()创建进程
    pid_t pid = fork();
    if (pid == -1) {
        LOG_ERROR("Failed to fork process: %d", errno);
        close(process->msg_pipe[0]);
        close(process->msg_pipe[1]);
        process->msg_pipe[0] = -1;
        process->msg_pipe[1] = -1;
        return -1;
    } else if (pid == 0) {
        // 子进程
        close(process->msg_pipe[1]); // 关闭写端
        
        // 设置进程优先级
        if (process->priority == PROCESS_PRIORITY_HIGH) {
            nice(-10); // 提高优先级
        } else if (process->priority == PROCESS_PRIORITY_LOW) {
            nice(10); // 降低优先级
        }
        
        // 执行进程主函数
        if (process->main_func) {
            process->main_func(process->main_arg);
        }
        
        close(process->msg_pipe[0]);
        exit(0);
    } else {
        // 父进程
        close(process->msg_pipe[0]); // 关闭读端
        return pid;
    }
#endif
}

/**
 * @brief 终止进程（跨平台实现）
 * @param process 进程信息
 * @return 终止结果：0表示成功，非0表示失败
 */
static int terminate_process(ProcessInfo_t *process) {
    if (!process || process->pid == -1) {
        return SUCCESS;
    }
    
#ifdef _WIN32
    // Windows 平台：使用TerminateThread终止线程
    HANDLE hThread = OpenThread(THREAD_TERMINATE, FALSE, (DWORD)process->pid);
    if (hThread != NULL) {
        if (!TerminateThread(hThread, 0)) {
            CloseHandle(hThread);
            LOG_ERROR("Failed to terminate process (thread) on Windows");
            return FAILURE;
        }
        CloseHandle(hThread);
    }
#else
    // Unix 平台：使用SIGTERM信号终止进程
    if (kill(process->pid, SIGTERM) == -1) {
        LOG_ERROR("Failed to terminate process: %d", errno);
        return FAILURE;
    }
    
    // 等待进程退出
    waitpid(process->pid, NULL, WNOHANG);
#endif
    
    // 关闭管道
    if (process->msg_pipe[1] != -1) {
        close(process->msg_pipe[1]);
        process->msg_pipe[1] = -1;
    }
    if (process->msg_pipe[0] != -1) {
        close(process->msg_pipe[0]);
        process->msg_pipe[0] = -1;
    }
    
    process->pid = -1;
    process->status = PROCESS_STATUS_IDLE;
    return SUCCESS;
}

/**
 * @brief 检查依赖的进程是否都已启动
 * @param process 进程信息
 * @return 检查结果：true表示所有依赖都已启动，false表示有依赖未启动
 */
static bool check_dependencies(ProcessInfo_t *process) {
    if (!process || process->dependency_count == 0) {
        return true;
    }
    
    for (int i = 0; i < process->dependency_count; i++) {
        if (!process->dependencies[i]) {
            continue;
        }
        
        ProcessInfo_t *dep_process = find_process(process->dependencies[i]);
        if (!dep_process || dep_process->status != PROCESS_STATUS_RUNNING) {
            LOG_WARN("Dependency process %s not running for %s", process->dependencies[i], process->name);
            return false;
        }
    }
    
    return true;
}

/**
 * @brief 按优先级排序进程列表
 * @return 排序后的进程列表
 */
static ProcessInfo_t *sort_processes_by_priority(void) {
    // 简单的冒泡排序
    ProcessInfo_t *sorted = NULL;
    ProcessInfo_t *current = g_process_list;
    
    while (current) {
        ProcessInfo_t *next = current->next;
        
        // 插入到排序后的列表中
        if (!sorted) {
            sorted = current;
            current->next = NULL;
        } else {
            ProcessInfo_t *prev = NULL;
            ProcessInfo_t *sorted_current = sorted;
            
            while (sorted_current && sorted_current->priority >= current->priority) {
                prev = sorted_current;
                sorted_current = sorted_current->next;
            }
            
            if (!prev) {
                current->next = sorted;
                sorted = current;
            } else {
                prev->next = current;
                current->next = sorted_current;
            }
        }
        
        current = next;
    }
    
    return sorted;
}

/******************************************************************************************
 * 进程管理模块对外接口
 ******************************************************************************************/

/**
 * @brief 初始化进程管理模块
 * @return 初始化结果：0表示成功，非0表示失败
 */
int process_manager_init(void) {
    if (g_process_manager_init) {
        LOG_INFO("Process manager already initialized");
        return SUCCESS;
    }
    
    // 初始化进程列表
    g_process_list = NULL;
    pthread_mutex_init(&g_process_list_mutex, NULL);
    
    // 注册系统进程
    process_manager_register_process("storage", storage_process_main, NULL, storage_process_terminate);
    process_manager_register_process("network", network_process_main, NULL, network_process_terminate);
    process_manager_register_process("audio", audio_process_main, NULL, audio_process_terminate);
    process_manager_register_process("system", system_process_main, NULL, system_process_terminate);
    process_manager_register_process("remote_control", remote_control_process_main_impl, NULL, remote_control_process_terminate);
    
    g_process_manager_init = true;
    LOG_INFO("Process manager init success");
    
    return SUCCESS;
}

/**
 * @brief 反初始化进程管理模块
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int process_manager_deinit(void) {
    if (!g_process_manager_init) {
        LOG_INFO("Process manager not initialized");
        return SUCCESS;
    }
    
    // 停止所有进程
    process_manager_stop_all_processes();
    
    // 清空进程列表
    ProcessInfo_t *current = g_process_list;
    while (current) {
        ProcessInfo_t *next = current->next;
        
        // 关闭管道
        if (current->msg_pipe[0] != -1) {
            close(current->msg_pipe[0]);
        }
        if (current->msg_pipe[1] != -1) {
            close(current->msg_pipe[1]);
        }
        
        // 释放依赖名称
        for (int i = 0; i < current->dependency_count; i++) {
            if (current->dependencies[i]) {
                free(current->dependencies[i]);
            }
        }
        
        free(current);
        current = next;
    }
    g_process_list = NULL;
    
    // 销毁互斥锁
    pthread_mutex_destroy(&g_process_list_mutex);
    
    g_process_manager_init = false;
    LOG_INFO("Process manager deinitialized");
    
    return SUCCESS;
}

/**
 * @brief 注册进程
 * @param name 进程名称
 * @param main_func 进程主函数
 * @param main_arg 进程主函数参数
 * @param terminate_func 进程终止函数
 * @return 注册结果：0表示成功，非0表示失败
 */
int process_manager_register_process(const char *name, ProcessMainFunc main_func, void *main_arg, int (*terminate_func)(void)) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    int ret = add_process(name, main_func, main_arg, terminate_func);
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    
    return ret;
}

/**
 * @brief 设置进程优先级
 * @param name 进程名称
 * @param priority 进程优先级
 * @return 设置结果：0表示成功，非0表示失败
 */
int process_manager_set_process_priority(const char *name, ProcessPriority_t priority) {
    if (!g_process_manager_init || priority >= PROCESS_PRIORITY_MAX) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    if (!process) {
        LOG_ERROR("Process not found: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    process->priority = priority;
    LOG_INFO("Set process %s priority to %d", name, priority);
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    return 0;
}

/**
 * @brief 设置进程自动重启
 * @param name 进程名称
 * @param auto_restart 是否自动重启
 * @return 设置结果：0表示成功，非0表示失败
 */
int process_manager_set_process_auto_restart(const char *name, bool auto_restart) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    if (!process) {
        LOG_ERROR("Process not found: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    process->auto_restart = auto_restart;
    LOG_INFO("Set process %s auto_restart to %s", name, auto_restart ? "true" : "false");
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    return 0;
}

/**
 * @brief 添加进程依赖
 * @param name 进程名称
 * @param dependency 依赖的进程名称
 * @return 添加结果：0表示成功，非0表示失败
 */
int process_manager_add_process_dependency(const char *name, const char *dependency) {
    if (!g_process_manager_init || !name || !dependency) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    if (!process) {
        LOG_ERROR("Process not found: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    if (process->dependency_count >= 10) {
        LOG_ERROR("Too many dependencies for process: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    // 检查依赖的进程是否存在
    if (!find_process(dependency)) {
        LOG_ERROR("Dependency process not found: %s", dependency);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    // 添加依赖
    process->dependencies[process->dependency_count] = strdup(dependency);
    if (!process->dependencies[process->dependency_count]) {
        LOG_ERROR("Failed to allocate memory for dependency");
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    process->dependency_count++;
    LOG_INFO("Added dependency %s to process %s", dependency, name);
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    return 0;
}

/**
 * @brief 启动进程
 * @param name 进程名称
 * @return 启动结果：0表示成功，非0表示失败
 */
int process_manager_start_process(const char *name) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    if (!process) {
        LOG_ERROR("Process not found: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    if (process->status == PROCESS_STATUS_RUNNING) {
        LOG_WARN("Process already running: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return 0;
    }
    
    // 检查依赖的进程是否都已启动
    if (!check_dependencies(process)) {
        LOG_ERROR("Dependencies not met for process: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    process->status = PROCESS_STATUS_STARTING;
    
    // 使用统一的进程创建接口
    int ret = 0;
    if (process->main_func) {
        // 使用统一的进程创建接口
        pid_t pid = create_process(process);
        if (pid == -1) {
            LOG_ERROR("Failed to create process: %s", name);
            process->status = PROCESS_STATUS_IDLE;
            MUTEX_LOCK_UNLOCK(g_process_list_mutex);
            return -1;
        }
        
        process->pid = pid;
        process->status = PROCESS_STATUS_RUNNING;
        process->start_time = time(NULL);
        process->restart_count++;
        ret = SUCCESS;
        LOG_INFO("Started process: %s, pid: %d", name, pid);
    } else {
        LOG_ERROR("Process has no main function: %s", name);
        process->status = PROCESS_STATUS_IDLE;
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    if (ret != SUCCESS) {
        LOG_ERROR("Failed to start process: %s", name);
        process->status = PROCESS_STATUS_IDLE;
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    // 如果是使用模块接口启动的进程，设置虚拟进程ID
    if (process->pid == -1) {
        process->pid = 1; // 虚拟进程ID
    }
    
    process->status = PROCESS_STATUS_RUNNING;
    process->start_time = time(NULL);
    process->restart_count++;
    LOG_INFO("Started process: %s, pid: %d", name, process->pid);
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    return 0;
}

/**
 * @brief 停止进程
 * @param name 进程名称
 * @return 停止结果：0表示成功，非0表示失败
 */
int process_manager_stop_process(const char *name) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    if (!process) {
        LOG_ERROR("Process not found: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    if (process->status == PROCESS_STATUS_IDLE) {
        LOG_WARN("Process not running: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return 0;
    }
    
    process->status = PROCESS_STATUS_STOPPING;
    
    // 使用统一的进程终止接口
    int ret = 0;
    if (process->terminate_func) {
        ret = process->terminate_func();
    } else {
        // 使用统一的进程终止接口
        ret = terminate_process(process);
    }
    
    if (ret != SUCCESS) {
        LOG_ERROR("Failed to stop process: %s", name);
        process->status = PROCESS_STATUS_CRASHED;
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    // 重置进程信息
    process->pid = -1;
    process->status = PROCESS_STATUS_IDLE;
    LOG_INFO("Stopped process: %s", name);
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    return 0;
}

/**
 * @brief 启动所有进程
 * @return 启动结果：0表示成功，非0表示失败
 */
int process_manager_start_all_processes(void) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    // 按优先级排序进程列表
    ProcessInfo_t *sorted_list = sort_processes_by_priority();
    
    ProcessInfo_t *current = sorted_list;
    int failed = 0;
    
    while (current) {
        if (current->status == PROCESS_STATUS_IDLE) {
            // 检查依赖的进程是否都已启动
            if (!check_dependencies(current)) {
                LOG_WARN("Dependencies not met for process: %s, skipping", current->name);
                current = current->next;
                continue;
            }
            
            current->status = PROCESS_STATUS_STARTING;
            
            // 使用统一的进程创建接口
            int ret = 0;
            if (current->main_func) {
                // 使用统一的进程创建接口
                pid_t pid = create_process(current);
                if (pid == -1) {
                    LOG_ERROR("Failed to create process: %s", current->name);
                    current->status = PROCESS_STATUS_IDLE;
                    failed++;
                    current = current->next;
                    continue;
                }
                
                current->pid = pid;
                current->status = PROCESS_STATUS_RUNNING;
                current->start_time = time(NULL);
                current->restart_count++;
                ret = SUCCESS;
                LOG_INFO("Started process: %s, pid: %d", current->name, pid);
            } else {
                LOG_ERROR("Process has no main function: %s", current->name);
                current->status = PROCESS_STATUS_IDLE;
                failed++;
                current = current->next;
                continue;
            }
            
            if (ret != SUCCESS) {
                LOG_ERROR("Failed to start process: %s", current->name);
                current->status = PROCESS_STATUS_IDLE;
                failed++;
            } else {
                // 如果是使用模块接口启动的进程，设置虚拟进程ID
                if (current->pid == -1) {
                    current->pid = 1; // 虚拟进程ID
                }
                
                current->status = PROCESS_STATUS_RUNNING;
                current->start_time = time(NULL);
                current->restart_count++;
                LOG_INFO("Started process: %s, pid: %d", current->name, current->pid);
            }
        }
        current = current->next;
    }
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    
    if (failed > 0) {
        LOG_ERROR("Failed to start %d processes", failed);
        return -1;
    }
    
    LOG_INFO("Started all processes");
    return 0;
}

/**
 * @brief 停止所有进程
 * @return 停止结果：0表示成功，非0表示失败
 */
int process_manager_stop_all_processes(void) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *current = g_process_list;
    int failed = 0;
    
    while (current) {
        if (current->status == PROCESS_STATUS_RUNNING) {
            current->status = PROCESS_STATUS_STOPPING;
            
            // 使用统一的进程终止接口
            int ret = 0;
            if (current->terminate_func) {
                ret = current->terminate_func();
            } else {
                // 使用统一的进程终止接口
                ret = terminate_process(current);
            }
            
            if (ret != SUCCESS) {
                LOG_ERROR("Failed to stop process: %s", current->name);
                current->status = PROCESS_STATUS_CRASHED;
                failed++;
            } else {
                current->pid = -1;
                current->status = PROCESS_STATUS_IDLE;
                LOG_INFO("Stopped process: %s", current->name);
            }
        }
        current = current->next;
    }
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    
    if (failed > 0) {
        LOG_ERROR("Failed to stop %d processes", failed);
        return -1;
    }
    
    LOG_INFO("Stopped all processes");
    return 0;
}

/**
 * @brief 获取进程ID
 * @param name 进程名称
 * @return 进程ID，失败返回-1
 */
pid_t process_manager_get_process_pid(const char *name) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    pid_t pid = process ? process->pid : -1;
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    
    return pid;
}

/**
 * @brief 获取进程状态
 * @param name 进程名称
 * @return 进程状态，失败返回PROCESS_STATUS_IDLE
 */
ProcessStatus_t process_manager_get_process_status(const char *name) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return PROCESS_STATUS_IDLE;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    ProcessStatus_t status = PROCESS_STATUS_IDLE;
    
    if (process) {
        status = process->status;
    }
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    
    return status;
}

/**
 * @brief 检查进程是否运行
 * @param name 进程名称
 * @return 运行状态：true表示运行，false表示未运行
 */
bool process_manager_is_process_running(const char *name) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return false;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    bool running = false;
    
    if (process) {
        if (process->status == PROCESS_STATUS_RUNNING) {
            // 验证进程是否真的在运行
            running = is_process_running(process->pid);
            if (!running) {
                // 更新状态
                process->status = PROCESS_STATUS_CRASHED;
                LOG_WARN("Process %s has crashed, updated status", name);
            }
        }
    }
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    
    return running;
}

/**
 * @brief 监控进程状态
 * @return 监控结果：0表示成功，非0表示失败
 */
int process_manager_monitor_processes(void) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *current = g_process_list;
    int restarted = 0;
    
    while (current) {
        if (current->status == PROCESS_STATUS_RUNNING) {
            // 验证进程是否真的在运行
            if (!is_process_running(current->pid)) {
                // 更新状态为崩溃
                current->status = PROCESS_STATUS_CRASHED;
                LOG_WARN("Process %s has crashed", current->name);
                
                // 如果设置了自动重启，尝试重启进程
                if (current->auto_restart) {
                    LOG_INFO("Attempting to restart crashed process: %s", current->name);
                    
                    // 重置进程信息
                    current->pid = -1;
                    current->status = PROCESS_STATUS_STARTING;
                    
                    // 使用统一的进程创建接口
                    int ret = 0;
                    if (current->main_func) {
                        // 使用统一的进程创建接口
                        pid_t pid = create_process(current);
                        if (pid == -1) {
                            LOG_ERROR("Failed to restart process: %s", current->name);
                            current->status = PROCESS_STATUS_CRASHED;
                            current = current->next;
                            continue;
                        }
                        
                        current->pid = pid;
                        current->status = PROCESS_STATUS_RUNNING;
                        current->start_time = time(NULL);
                        current->restart_count++;
                        ret = SUCCESS;
                        LOG_INFO("Restarted process: %s, pid: %d", current->name, pid);
                    } else {
                        LOG_ERROR("Process has no main function: %s", current->name);
                        current->status = PROCESS_STATUS_CRASHED;
                        current = current->next;
                        continue;
                    }
                    
                    if (ret != SUCCESS) {
                        LOG_ERROR("Failed to restart process: %s", current->name);
                        current->status = PROCESS_STATUS_CRASHED;
                    } else {
                        LOG_INFO("Restarted process: %s, pid: %d", current->name, current->pid);
                        restarted++;
                    }
                }
            } else {
                LOG_DEBUG("Monitoring process: %s (pid: %d, status: running)", current->name, current->pid);
            }
        }
        current = current->next;
    }
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    
    if (restarted > 0) {
        LOG_INFO("Restarted %d crashed processes", restarted);
    }
    
    return 0;
}

/**
 * @brief 发送消息到进程
 * @param name 进程名称
 * @param msg_type 消息类型
 * @param msg_data 消息数据
 * @param msg_size 消息大小
 * @return 发送结果：0表示成功，非0表示失败
 */
int process_manager_send_message(const char *name, uint32_t msg_type, void *msg_data, size_t msg_size) {
    if (!g_process_manager_init || !name) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    if (!process || process->status != PROCESS_STATUS_RUNNING || process->msg_pipe[1] == -1) {
        LOG_ERROR("Process not running or no message pipe: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    // 构建消息
    ProcessMsg_t msg;
    msg.msg_type = msg_type;
    msg.msg_size = msg_size;
    msg.msg_data = msg_data;
    
    // 发送消息
    if (write(process->msg_pipe[1], &msg, sizeof(ProcessMsg_t)) != sizeof(ProcessMsg_t)) {
        LOG_ERROR("Failed to send message to process: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    // 如果有消息数据，发送数据
    if (msg_data && msg_size > 0) {
        if (write(process->msg_pipe[1], msg_data, msg_size) != (ssize_t)msg_size) {
            LOG_ERROR("Failed to send message data to process: %s", name);
            MUTEX_LOCK_UNLOCK(g_process_list_mutex);
            return -1;
        }
    }
    
    LOG_DEBUG("Sent message type %d to process: %s", msg_type, name);
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    return 0;
}

/**
 * @brief 广播消息到所有进程
 * @param msg_type 消息类型
 * @param msg_data 消息数据
 * @param msg_size 消息大小
 * @return 广播结果：0表示成功，非0表示失败
 */
int process_manager_broadcast_message(uint32_t msg_type, void *msg_data, size_t msg_size) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *current = g_process_list;
    int failed = 0;
    
    while (current) {
        if (current->status == PROCESS_STATUS_RUNNING && current->msg_pipe[1] != -1) {
            // 构建消息
            ProcessMsg_t msg;
            msg.msg_type = msg_type;
            msg.msg_size = msg_size;
            msg.msg_data = msg_data;
            
            // 发送消息
            if (write(current->msg_pipe[1], &msg, sizeof(ProcessMsg_t)) != sizeof(ProcessMsg_t)) {
                LOG_ERROR("Failed to send broadcast message to process: %s", current->name);
                failed++;
                current = current->next;
                continue;
            }
            
            // 如果有消息数据，发送数据
            if (msg_data && msg_size > 0) {
                if (write(current->msg_pipe[1], msg_data, msg_size) != (ssize_t)msg_size) {
                    LOG_ERROR("Failed to send broadcast message data to process: %s", current->name);
                    failed++;
                    current = current->next;
                    continue;
                }
            }
            
            LOG_DEBUG("Sent broadcast message type %d to process: %s", msg_type, current->name);
        }
        current = current->next;
    }
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    
    if (failed > 0) {
        LOG_ERROR("Failed to send broadcast message to %d processes", failed);
        return -1;
    }
    
    return 0;
}

/**
 * @brief 设置进程资源限制
 * @param name 进程名称
 * @param limits 资源限制
 * @return 设置结果：0表示成功，非0表示失败
 */
int process_manager_set_process_resource_limits(const char *name, ProcessResourceLimits_t *limits) {
    if (!g_process_manager_init || !name || !limits) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    if (!process) {
        LOG_ERROR("Process not found: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    // 设置资源限制
    process->limits = *limits;
    LOG_INFO("Set resource limits for process %s:", name);
    LOG_INFO("  Max CPU usage: %d%%", limits->max_cpu_usage);
    LOG_INFO("  Max memory: %d MB", limits->max_memory);
    LOG_INFO("  Max file descriptors: %d", limits->max_file_descriptors);
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    return 0;
}

/**
 * @brief 获取进程资源使用情况
 * @param name 进程名称
 * @param cpu_usage CPU使用率（输出）
 * @param memory_usage 内存使用量（输出，MB）
 * @return 获取结果：0表示成功，非0表示失败
 */
int process_manager_get_process_resource_usage(const char *name, int *cpu_usage, int *memory_usage) {
    if (!g_process_manager_init || !name) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    if (!process || process->status != PROCESS_STATUS_RUNNING) {
        LOG_ERROR("Process not found or not running: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    // 这里只是一个示例实现，实际应该根据操作系统获取真实的资源使用情况
    if (cpu_usage) {
        *cpu_usage = 0; // 示例值
    }
    if (memory_usage) {
        *memory_usage = 0; // 示例值
    }
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    return 0;
}

/**
 * @brief 终止进程（发送信号）
 * @param name 进程名称
 * @param signal 信号值
 * @return 终止结果：0表示成功，非0表示失败
 */
int process_manager_kill_process(const char *name, int signal) {
    if (!g_process_manager_init || !name) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    if (!process || process->status != PROCESS_STATUS_RUNNING) {
        LOG_ERROR("Process not found or not running: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
#ifdef _WIN32
    // Windows平台：使用TerminateThread终止线程
    HANDLE hThread = OpenThread(THREAD_TERMINATE, FALSE, (DWORD)process->pid);
    if (hThread != NULL) {
        if (!TerminateThread(hThread, signal)) {
            CloseHandle(hThread);
            LOG_ERROR("Failed to terminate process (thread) on Windows");
            MUTEX_LOCK_UNLOCK(g_process_list_mutex);
            return -1;
        }
        CloseHandle(hThread);
    }
#else
    // Unix平台：使用kill发送信号
    if (kill(process->pid, signal) == -1) {
        LOG_ERROR("Failed to send signal %d to process: %s", signal, name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
#endif
    
    LOG_INFO("Sent signal %d to process: %s", signal, name);
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    return 0;
}
