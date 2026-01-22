/**
 * @file system_process.c
 * @brief 系统管理进程实现
 * @details 负责系统级管理功能，包括进程监控、资源管理等
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
 * 系统管理进程消息类型
 ******************************************************************************************/

/**
 * @brief 系统管理进程消息类型
 */
typedef enum {
    SYSTEM_MSG_INIT = 0,           // 初始化系统管理
    SYSTEM_MSG_DEINIT,              // 反初始化系统管理
    SYSTEM_MSG_MONITOR_PROCESS,     // 监控进程状态
    SYSTEM_MSG_MANAGE_RESOURCES,    // 管理系统资源
    SYSTEM_MSG_HANDLE_CRASH,        // 处理进程崩溃
    SYSTEM_MSG_RESTART_PROCESS,     // 重启进程
    SYSTEM_MSG_GET_SYSTEM_INFO,     // 获取系统信息
    SYSTEM_MSG_MAX
} SystemMsgType_t;

/**
 * @brief 系统管理进程消息
 */
typedef struct {
    SystemMsgType_t type;           // 消息类型
    union {
        struct {
            pid_t pid;              // 进程ID
        } monitor_process;
        struct {
            pid_t pid;              // 进程ID
        } restart_process;
    } data;
} SystemMsg_t;

/******************************************************************************************
 * 系统管理进程全局变量
 ******************************************************************************************/

static bool g_system_process_running = false;
static pid_t g_system_process_pid = -1;
static int g_system_process_pipe[2] = {-1, -1};

/******************************************************************************************
 * 系统管理进程内部函数
 ******************************************************************************************/

/**
 * @brief 系统管理进程主函数
 * @param arg 进程参数
 * @return 进程返回值
 */
static void *system_process_main(void *arg) {
    LOG_INFO("System process started, pid: %d", getpid());
    
    // 初始化系统管理
    if (system_init() != 0) {
        LOG_ERROR("System init failed");
        return NULL;
    }
    
    // 主循环
    while (g_system_process_running) {
        // 接收消息
        SystemMsg_t msg;
        int ret = read(g_system_process_pipe[0], &msg, sizeof(SystemMsg_t));
        if (ret <= 0) {
            // 模拟进程监控
            sleep(1);
            continue;
        }
        
        // 处理消息
        switch (msg.type) {
            case SYSTEM_MSG_INIT:
                system_deinit();
                system_init();
                break;
            case SYSTEM_MSG_DEINIT:
                system_deinit();
                break;
            case SYSTEM_MSG_MONITOR_PROCESS:
                // 监控指定进程
                break;
            case SYSTEM_MSG_MANAGE_RESOURCES:
                // 管理系统资源
                break;
            case SYSTEM_MSG_HANDLE_CRASH:
                // 处理进程崩溃
                break;
            case SYSTEM_MSG_RESTART_PROCESS:
                // 重启指定进程
                break;
            case SYSTEM_MSG_GET_SYSTEM_INFO:
                // 获取系统信息
                break;
            default:
                LOG_ERROR("Invalid system message type: %d", msg.type);
                break;
        }
    }
    
    // 反初始化系统管理
    system_deinit();
    
    LOG_INFO("System process exited");
    return NULL;
}

/**
 * @brief 创建系统管理进程
 * @return 进程ID，失败返回-1
 */
static pid_t create_system_process(void) {
    // 创建管道
    if (pipe(g_system_process_pipe) == -1) {
        LOG_ERROR("Failed to create pipe: %d", errno);
        return -1;
    }
    
    // 创建进程
    pid_t pid = fork();
    if (pid == -1) {
        LOG_ERROR("Failed to fork system process: %d", errno);
        close(g_system_process_pipe[0]);
        close(g_system_process_pipe[1]);
        g_system_process_pipe[0] = -1;
        g_system_process_pipe[1] = -1;
        return -1;
    } else if (pid == 0) {
        // 子进程
        close(g_system_process_pipe[1]); // 关闭写端
        g_system_process_running = true;
        system_process_main(NULL);
        close(g_system_process_pipe[0]);
        exit(0);
    } else {
        // 父进程
        close(g_system_process_pipe[0]); // 关闭读端
        g_system_process_pid = pid;
        LOG_INFO("Created system process: %d", pid);
    }
    
    return pid;
}

/**
 * @brief 终止系统管理进程
 * @return SUCCESS/FAILURE
 */
static int terminate_system_process(void) {
    if (g_system_process_pid == -1) {
        return SUCCESS;
    }
    
    // 发送终止消息
    SystemMsg_t msg;
    msg.type = SYSTEM_MSG_DEINIT;
    write(g_system_process_pipe[1], &msg, sizeof(SystemMsg_t));
    
    // 终止进程
    if (kill(g_system_process_pid, SIGTERM) == -1) {
        LOG_ERROR("Failed to terminate system process: %d", errno);
        return FAILURE;
    }
    
    // 等待进程退出
    waitpid(g_system_process_pid, NULL, 0);
    
    // 关闭管道
    close(g_system_process_pipe[1]);
    g_system_process_pipe[0] = -1;
    g_system_process_pipe[1] = -1;
    g_system_process_pid = -1;
    
    LOG_INFO("Terminated system process: %d", g_system_process_pid);
    return SUCCESS;
}

/******************************************************************************************
 * 系统管理进程对外接口
 ******************************************************************************************/

/**
 * @brief 初始化系统管理进程
 * @return SUCCESS/FAILURE
 */
int system_process_init(void) {
    // 创建系统管理进程
    pid_t pid = create_system_process();
    if (pid == -1) {
        LOG_ERROR("Failed to create system process");
        return FAILURE;
    }
    
    LOG_INFO("System process initialized");
    return SUCCESS;
}

/**
 * @brief 反初始化系统管理进程
 * @return SUCCESS/FAILURE
 */
int system_process_deinit(void) {
    int ret = terminate_system_process();
    if (ret == SUCCESS) {
        LOG_INFO("System process deinitialized");
    }
    return ret;
}

/**
 * @brief 监控进程状态
 * @param pid 进程ID
 * @return SUCCESS/FAILURE
 */
int system_process_monitor(pid_t pid) {
    if (g_system_process_pid == -1) {
        LOG_ERROR("System process not initialized");
        return FAILURE;
    }
    
    SystemMsg_t msg;
    msg.type = SYSTEM_MSG_MONITOR_PROCESS;
    msg.data.monitor_process.pid = pid;
    
    if (write(g_system_process_pipe[1], &msg, sizeof(SystemMsg_t)) != sizeof(SystemMsg_t)) {
        LOG_ERROR("Failed to send monitor process message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 重启进程
 * @param pid 进程ID
 * @return SUCCESS/FAILURE
 */
int system_process_restart(pid_t pid) {
    if (g_system_process_pid == -1) {
        LOG_ERROR("System process not initialized");
        return FAILURE;
    }
    
    SystemMsg_t msg;
    msg.type = SYSTEM_MSG_RESTART_PROCESS;
    msg.data.restart_process.pid = pid;
    
    if (write(g_system_process_pipe[1], &msg, sizeof(SystemMsg_t)) != sizeof(SystemMsg_t)) {
        LOG_ERROR("Failed to send restart process message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 获取系统管理进程ID
 * @return 进程ID，失败返回-1
 */
pid_t system_process_get_pid(void) {
    return g_system_process_pid;
}

/**
 * @brief 检查系统管理进程是否运行
 * @return true表示运行，false表示未运行
 */
bool system_process_is_running(void) {
    if (g_system_process_pid == -1) {
        return false;
    }
    
    int status;
    pid_t result = waitpid(g_system_process_pid, &status, WNOHANG);
    return result == 0;
}