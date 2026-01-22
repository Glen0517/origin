/**
 * @file process_manager.c
 * @brief 进程管理器实现
 * @details 负责管理系统中的所有进程，包括进程的创建、监控和管理
 * @author AML Audio Team
 * @date 2026-01-22
 */

#include "system.h"
#include "system_priv.h"
#include "logger.h"
#include "event.h"
#include "process.h"
#include "common_def.h"

/******************************************************************************************
 * 进程管理器内部数据结构
 ******************************************************************************************/

/**
 * @brief 系统进程类型
 */
typedef enum {
    SYS_PROCESS_AUDIO = 0,       // 音频处理进程
    SYS_PROCESS_NETWORK = 1,      // 网络服务进程
    SYS_PROCESS_STORAGE = 2,      // 存储管理进程
    SYS_PROCESS_SYSTEM = 3,       // 系统管理进程
    SYS_PROCESS_MAX
} SysProcessType_t;

/**
 * @brief 系统进程信息
 */
typedef struct {
    SysProcessType_t type;        // 进程类型
    pid_t pid;                    // 进程ID
    char name[32];                // 进程名称
    bool initialized;             // 初始化标志
    bool running;                 // 运行标志
    uint32_t restart_count;       // 重启次数
    uint32_t last_restart_time;   // 最后重启时间
} SysProcessInfo_t;

/******************************************************************************************
 * 进程管理器全局变量
 ******************************************************************************************/

static SysProcessInfo_t g_sys_processes[SYS_PROCESS_MAX] = {
    {SYS_PROCESS_AUDIO, -1, "Audio Process", false, false, 0, 0},
    {SYS_PROCESS_NETWORK, -1, "Network Process", false, false, 0, 0},
    {SYS_PROCESS_STORAGE, -1, "Storage Process", false, false, 0, 0},
    {SYS_PROCESS_SYSTEM, -1, "System Process", false, false, 0, 0}
};

static bool g_process_manager_initialized = false;

/******************************************************************************************
 * 进程管理器内部函数
 ******************************************************************************************/

/**
 * @brief 创建系统进程
 * @param type 进程类型
 * @return 进程ID，失败返回-1
 */
static pid_t create_system_process(SysProcessType_t type) {
    const char *name = g_sys_processes[type].name;
    int priority = 0;
    ProcessType_t process_type = PROCESS_TYPE_SYSTEM;
    
    switch (type) {
        case SYS_PROCESS_AUDIO:
            process_type = PROCESS_TYPE_AUDIO;
            priority = 1; // 高优先级
            break;
        case SYS_PROCESS_NETWORK:
            process_type = PROCESS_TYPE_NETWORK;
            priority = 0; // 普通优先级
            break;
        case SYS_PROCESS_STORAGE:
            process_type = PROCESS_TYPE_STORAGE;
            priority = -1; // 低优先级
            break;
        case SYS_PROCESS_SYSTEM:
            process_type = PROCESS_TYPE_SYSTEM;
            priority = 1; // 高优先级
            break;
        default:
            LOG_ERROR("Invalid process type: %d", type);
            return -1;
    }
    
    pid_t pid = process_create(process_type, name, priority);
    if (pid != -1) {
        g_sys_processes[type].pid = pid;
        g_sys_processes[type].running = true;
        g_sys_processes[type].initialized = true;
        g_sys_processes[type].restart_count = 0;
        g_sys_processes[type].last_restart_time = GetTickCount();
        LOG_INFO("Created system process: %s, pid: %d, priority: %d", name, pid, priority);
    } else {
        LOG_ERROR("Failed to create system process: %s", name);
    }
    
    return pid;
}

/**
 * @brief 重启系统进程
 * @param type 进程类型
 * @return 进程ID，失败返回-1
 */
static pid_t restart_system_process(SysProcessType_t type) {
    if (g_sys_processes[type].pid != -1) {
        process_terminate(g_sys_processes[type].pid);
        g_sys_processes[type].pid = -1;
        g_sys_processes[type].running = false;
    }
    
    pid_t pid = create_system_process(type);
    if (pid != -1) {
        g_sys_processes[type].restart_count++;
        g_sys_processes[type].last_restart_time = GetTickCount();
        LOG_INFO("Restarted system process: %s, pid: %d, restart count: %d", 
                 g_sys_processes[type].name, pid, g_sys_processes[type].restart_count);
    }
    
    return pid;
}

/**
 * @brief 监控系统进程
 */
static void monitor_system_processes(void) {
    for (int i = 0; i < SYS_PROCESS_MAX; i++) {
        if (g_sys_processes[i].initialized && g_sys_processes[i].pid != -1) {
            bool running = process_is_running(g_sys_processes[i].pid);
            if (!running && g_sys_processes[i].running) {
                LOG_WARN("System process crashed: %s, pid: %d", 
                         g_sys_processes[i].name, g_sys_processes[i].pid);
                
                // 重启进程
                restart_system_process((SysProcessType_t)i);
                
                // 发送进程崩溃事件
                event_notify(EVENT_SYSTEM_PROCESS_CRASH, &i);
            }
            g_sys_processes[i].running = running;
        }
    }
}

/**
 * @brief 获取系统进程信息
 * @param type 进程类型
 * @param info 进程信息
 * @return SUCCESS/FAILURE
 */
static int get_system_process_info(SysProcessType_t type, ProcessInfo_t *info) {
    if (type >= SYS_PROCESS_MAX) {
        LOG_ERROR("Invalid process type: %d", type);
        return FAILURE;
    }
    
    if (g_sys_processes[type].pid == -1) {
        LOG_ERROR("Process not created: %s", g_sys_processes[type].name);
        return FAILURE;
    }
    
    return process_get_info(g_sys_processes[type].pid, info);
}

/******************************************************************************************
 * 进程管理器对外接口
 ******************************************************************************************/

/**
 * @brief 初始化进程管理器
 * @return SUCCESS/FAILURE
 */
int process_manager_init(void) {
    if (g_process_manager_initialized) {
        LOG_WARN("Process manager already initialized");
        return SUCCESS;
    }
    
    // 初始化进程管理模块
    if (process_init() != SUCCESS) {
        LOG_ERROR("Failed to initialize process module");
        return FAILURE;
    }
    
    // 创建系统进程
    for (int i = 0; i < SYS_PROCESS_MAX; i++) {
        create_system_process((SysProcessType_t)i);
    }
    
    g_process_manager_initialized = true;
    LOG_INFO("Process manager initialized successfully");
    return SUCCESS;
}

/**
 * @brief 反初始化进程管理器
 * @return SUCCESS/FAILURE
 */
int process_manager_deinit(void) {
    if (!g_process_manager_initialized) {
        LOG_WARN("Process manager not initialized");
        return SUCCESS;
    }
    
    // 终止所有系统进程
    for (int i = 0; i < SYS_PROCESS_MAX; i++) {
        if (g_sys_processes[i].pid != -1) {
            process_terminate(g_sys_processes[i].pid);
            g_sys_processes[i].pid = -1;
            g_sys_processes[i].running = false;
            g_sys_processes[i].initialized = false;
        }
    }
    
    // 反初始化进程管理模块
    if (process_deinit() != SUCCESS) {
        LOG_ERROR("Failed to deinitialize process module");
        return FAILURE;
    }
    
    g_process_manager_initialized = false;
    LOG_INFO("Process manager deinitialized successfully");
    return SUCCESS;
}

/**
 * @brief 获取系统进程ID
 * @param type 进程类型
 * @return 进程ID，失败返回-1
 */
pid_t process_manager_get_process_id(SysProcessType_t type) {
    if (type >= SYS_PROCESS_MAX) {
        LOG_ERROR("Invalid process type: %d", type);
        return -1;
    }
    
    return g_sys_processes[type].pid;
}

/**
 * @brief 重启系统进程
 * @param type 进程类型
 * @return SUCCESS/FAILURE
 */
int process_manager_restart_process(SysProcessType_t type) {
    if (type >= SYS_PROCESS_MAX) {
        LOG_ERROR("Invalid process type: %d", type);
        return FAILURE;
    }
    
    pid_t pid = restart_system_process(type);
    return pid != -1 ? SUCCESS : FAILURE;
}

/**
 * @brief 监控系统进程状态
 * @return SUCCESS/FAILURE
 */
int process_manager_monitor(void) {
    if (!g_process_manager_initialized) {
        LOG_ERROR("Process manager not initialized");
        return FAILURE;
    }
    
    monitor_system_processes();
    return SUCCESS;
}

/**
 * @brief 获取系统进程状态
 * @param type 进程类型
 * @param running 运行状态
 * @return SUCCESS/FAILURE
 */
int process_manager_get_process_status(SysProcessType_t type, bool *running) {
    if (type >= SYS_PROCESS_MAX) {
        LOG_ERROR("Invalid process type: %d", type);
        return FAILURE;
    }
    
    if (running) {
        *running = g_sys_processes[type].running;
    }
    
    return SUCCESS;
}

/**
 * @brief 获取系统进程信息
 * @param type 进程类型
 * @param info 进程信息
 * @return SUCCESS/FAILURE
 */
int process_manager_get_process_info(SysProcessType_t type, ProcessInfo_t *info) {
    return get_system_process_info(type, info);
}

/**
 * @brief 获取所有系统进程信息
 * @param infos 进程信息数组
 * @param max_count 最大进程数
 * @param count 实际进程数
 * @return SUCCESS/FAILURE
 */
int process_manager_get_all_process_info(ProcessInfo_t *infos, int max_count, int *count) {
    if (!infos || !count) {
        LOG_ERROR("Invalid parameters");
        return FAILURE;
    }
    
    int actual_count = 0;
    for (int i = 0; i < SYS_PROCESS_MAX && i < max_count; i++) {
        if (g_sys_processes[i].pid != -1) {
            if (process_get_info(g_sys_processes[i].pid, &infos[actual_count]) == SUCCESS) {
                actual_count++;
            }
        }
    }
    
    *count = actual_count;
    return SUCCESS;
}

/**
 * @brief 发送消息到系统进程
 * @param type 进程类型
 * @param msg 消息内容
 * @param msg_len 消息长度
 * @return SUCCESS/FAILURE
 */
int process_manager_send_message(SysProcessType_t type, const void *msg, int msg_len) {
    if (type >= SYS_PROCESS_MAX) {
        LOG_ERROR("Invalid process type: %d", type);
        return FAILURE;
    }
    
    if (g_sys_processes[type].pid == -1) {
        LOG_ERROR("Process not created: %s", g_sys_processes[type].name);
        return FAILURE;
    }
    
    return process_send_message(g_sys_processes[type].pid, msg, msg_len);
}

/**
 * @brief 接收来自系统进程的消息
 * @param type 进程类型
 * @param msg 消息缓冲区
 * @param msg_len 消息长度
 * @return 实际读取的消息长度，失败返回-1
 */
int process_manager_receive_message(SysProcessType_t type, void *msg, int msg_len) {
    if (type >= SYS_PROCESS_MAX) {
        LOG_ERROR("Invalid process type: %d", type);
        return -1;
    }
    
    if (g_sys_processes[type].pid == -1) {
        LOG_ERROR("Process not created: %s", g_sys_processes[type].name);
        return -1;
    }
    
    return process_receive_message(g_sys_processes[type].pid, msg, msg_len);
}
