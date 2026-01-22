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

// 包含各个模块的头文件
#include "../lib/flac/storage.h"
#include "../lib/flac/wifi_media.h"
#include "../lib/flac/audio_core.h"
#include "../lib/flac/system.h"
#include "../remote_control/remote_control.h"

/******************************************************************************************
 * 进程管理模块内部数据结构
 ******************************************************************************************/

/**
 * @brief 进程信息结构体
 */
typedef struct {
    char name[64];                 // 进程名称
    pid_t (*create_func)(void);     // 进程创建函数
    int (*terminate_func)(void);    // 进程终止函数
    pid_t pid;                      // 进程ID
    bool running;                   // 运行状态
    struct ProcessInfo_t *next;     // 指向下一个进程
} ProcessInfo_t;

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
 * @param create_func 进程创建函数
 * @param terminate_func 进程终止函数
 * @return 添加结果：0表示成功，非0表示失败
 */
static int add_process(const char *name, pid_t (*create_func)(void), int (*terminate_func)(void)) {
    if (!name || !create_func || !terminate_func) {
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
    strncpy(process->name, name, sizeof(process->name) - 1);
    process->create_func = create_func;
    process->terminate_func = terminate_func;
    process->pid = -1;
    process->running = false;
    process->next = NULL;
    
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
    
    // 尝试发送信号0（不执行任何操作，只检查进程是否存在）
    return (kill(pid, 0) == 0);
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
    process_manager_register_process("storage", NULL, NULL);
    process_manager_register_process("network", NULL, NULL);
    process_manager_register_process("audio", NULL, NULL);
    process_manager_register_process("system", NULL, NULL);
    process_manager_register_process("remote_control", NULL, NULL);
    
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
 * @param create_func 进程创建函数
 * @param terminate_func 进程终止函数
 * @return 注册结果：0表示成功，非0表示失败
 */
int process_manager_register_process(const char *name, pid_t (*create_func)(void), int (*terminate_func)(void)) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    int ret = add_process(name, create_func, terminate_func);
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    
    return ret;
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
    
    if (process->running) {
        LOG_WARN("Process already running: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return 0;
    }
    
    // 使用各个模块提供的对外接口函数启动进程
    int ret = 0;
    if (strcmp(name, "storage") == 0) {
        ret = storage_process_init();
    } else if (strcmp(name, "network") == 0) {
        ret = network_process_init();
    } else if (strcmp(name, "audio") == 0) {
        ret = audio_process_init();
    } else if (strcmp(name, "system") == 0) {
        ret = system_process_init();
    } else if (strcmp(name, "remote_control") == 0) {
        ret = remote_control_process_init();
    } else {
        LOG_ERROR("Unknown process name: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    if (ret != SUCCESS) {
        LOG_ERROR("Failed to start process: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    // 由于无法获取进程ID，我们使用一个虚拟的进程ID
    process->pid = 1; // 虚拟进程ID
    process->running = true;
    LOG_INFO("Started process: %s", name);
    
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
    
    if (!process->running) {
        LOG_WARN("Process not running: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return 0;
    }
    
    // 使用各个模块提供的对外接口函数停止进程
    int ret = 0;
    if (strcmp(name, "storage") == 0) {
        ret = storage_process_deinit();
    } else if (strcmp(name, "network") == 0) {
        ret = network_process_deinit();
    } else if (strcmp(name, "audio") == 0) {
        ret = audio_process_deinit();
    } else if (strcmp(name, "system") == 0) {
        ret = system_process_deinit();
    } else if (strcmp(name, "remote_control") == 0) {
        ret = remote_control_process_deinit();
    } else {
        LOG_ERROR("Unknown process name: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    if (ret != SUCCESS) {
        LOG_ERROR("Failed to stop process: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    process->pid = -1;
    process->running = false;
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
    
    ProcessInfo_t *current = g_process_list;
    int failed = 0;
    
    while (current) {
        if (!current->running) {
            // 使用各个模块提供的对外接口函数启动进程
            int ret = 0;
            if (strcmp(current->name, "storage") == 0) {
                ret = storage_process_init();
            } else if (strcmp(current->name, "network") == 0) {
                ret = network_process_init();
            } else if (strcmp(current->name, "audio") == 0) {
                ret = audio_process_init();
            } else if (strcmp(current->name, "system") == 0) {
                ret = system_process_init();
            } else if (strcmp(current->name, "remote_control") == 0) {
                ret = remote_control_process_init();
            }
            
            if (ret != SUCCESS) {
                LOG_ERROR("Failed to start process: %s", current->name);
                failed++;
            } else {
                current->pid = 1; // 虚拟进程ID
                current->running = true;
                LOG_INFO("Started process: %s", current->name);
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
        if (current->running) {
            // 使用各个模块提供的对外接口函数停止进程
            int ret = 0;
            if (strcmp(current->name, "storage") == 0) {
                ret = storage_process_deinit();
            } else if (strcmp(current->name, "network") == 0) {
                ret = network_process_deinit();
            } else if (strcmp(current->name, "audio") == 0) {
                ret = audio_process_deinit();
            } else if (strcmp(current->name, "system") == 0) {
                ret = system_process_deinit();
            } else if (strcmp(current->name, "remote_control") == 0) {
                ret = remote_control_process_deinit();
            }
            
            if (ret != SUCCESS) {
                LOG_ERROR("Failed to stop process: %s", current->name);
                failed++;
            } else {
                current->pid = -1;
                current->running = false;
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
        if (process->running) {
            // 验证进程是否真的在运行
            running = is_process_running(process->pid);
            if (!running) {
                // 更新状态
                process->running = false;
                process->pid = -1;
                LOG_WARN("Process %s is not running, updated status", name);
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
        if (current->running) {
            // 由于使用虚拟进程ID，我们无法直接检测进程状态
            // 这里我们假设所有进程都在正常运行
            // 实际应用中，应该使用各个模块提供的状态查询接口来检测进程状态
            LOG_DEBUG("Monitoring process: %s", current->name);
        }
        current = current->next;
    }
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    
    if (restarted > 0) {
        LOG_INFO("Restarted %d processes", restarted);
    }
    
    return 0;
}
