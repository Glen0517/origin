/**
 * @file process.c
 * @brief 进程管理模块实现
 * @details 提供进程的创建、管理和通信功能
 * @author AML Audio Team
 * @date 2026-01-22
 */

#include "process.h"
#include "logger.h"
#include "event.h"
#include "common_def.h"
#include <time.h>

#ifdef _WIN32
// Windows 特定头文件
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#else
// Unix 特定头文件
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#endif

/******************************************************************************************
 * 进程管理内部数据结构
 ******************************************************************************************/

/**
 * @brief 进程管理结构体
 */
typedef struct {
    ProcessInfo_t info;           // 进程信息
#ifdef _WIN32
    HANDLE hProcess;              // 进程句柄
    HANDLE hThread;               // 线程句柄
#else
    int pipe_fd[2];               // 管道文件描述符
#endif
    struct ProcessNode *next;     // 指向下一个进程
} ProcessNode_t;

/**
 * @brief 进程管理全局变量
 */
typedef struct {
    ProcessNode_t *process_list;  // 进程列表
    int process_count;            // 进程数量
#ifdef _WIN32
    HANDLE hMutex;                // 互斥锁
#else
    pthread_mutex_t mutex;         // 互斥锁
#endif
    bool initialized;             // 初始化标志
} ProcessManager_t;

/******************************************************************************************
 * 进程管理全局变量
 ******************************************************************************************/

static ProcessManager_t g_process_manager = {
    .process_list = NULL,
    .process_count = 0,
#ifdef _WIN32
    .hMutex = NULL,
#else
    .mutex = PTHREAD_MUTEX_INITIALIZER,
#endif
    .initialized = false
};

/******************************************************************************************
 * 进程管理内部函数
 ******************************************************************************************/

/**
 * @brief 加锁
 */
static void process_lock(void) {
#ifdef _WIN32
    if (g_process_manager.hMutex) {
        WaitForSingleObject(g_process_manager.hMutex, INFINITE);
    }
#else
    MUTEX_LOCK_LOCK(g_process_manager.mutex);
#endif
}

/**
 * @brief 解锁
 */
static void process_unlock(void) {
#ifdef _WIN32
    if (g_process_manager.hMutex) {
        ReleaseMutex(g_process_manager.hMutex);
    }
#else
    MUTEX_LOCK_UNLOCK(g_process_manager.mutex);
#endif
}

/**
 * @brief 查找进程节点
 * @param pid 进程ID
 * @return 进程节点
 */
static ProcessNode_t *process_find_node(pid_t pid) {
    ProcessNode_t *node = g_process_manager.process_list;
    while (node) {
        if (node->info.pid == pid) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

/**
 * @brief 添加进程节点
 * @param node 进程节点
 */
static void process_add_node(ProcessNode_t *node) {
    if (!g_process_manager.process_list) {
        g_process_manager.process_list = node;
    } else {
        ProcessNode_t *current = g_process_manager.process_list;
        while (current->next) {
            current = current->next;
        }
        current->next = node;
    }
    g_process_manager.process_count++;
}

/**
 * @brief 移除进程节点
 * @param pid 进程ID
 */
static void process_remove_node(pid_t pid) {
    ProcessNode_t *node = g_process_manager.process_list;
    ProcessNode_t *prev = NULL;
    
    while (node) {
        if (node->info.pid == pid) {
            if (prev) {
                prev->next = node->next;
            } else {
                g_process_manager.process_list = node->next;
            }
            
#ifdef _WIN32
            if (node->hProcess) {
                CloseHandle(node->hProcess);
            }
            if (node->hThread) {
                CloseHandle(node->hThread);
            }
#else
            if (node->pipe_fd[0] != -1) {
                close(node->pipe_fd[0]);
            }
            if (node->pipe_fd[1] != -1) {
                close(node->pipe_fd[1]);
            }
#endif
            
            free(node);
            g_process_manager.process_count--;
            break;
        }
        prev = node;
        node = node->next;
    }
}

/**
 * @brief 更新进程状态
 */
static void process_update_status(void) {
    ProcessNode_t *node = g_process_manager.process_list;
    while (node) {
#ifdef _WIN32
        DWORD exit_code;
        if (GetExitCodeProcess(node->hProcess, &exit_code)) {
            if (exit_code == STILL_ACTIVE) {
                node->info.status = PROCESS_STATUS_RUNNING;
                
                // 更新进程运行时间
                DWORD uptime = GetTickCount() - node->info.start_time;
                node->info.uptime = uptime / 1000;
                
                // 更新CPU和内存使用率（简化实现）
                node->info.cpu_usage = 0;
                node->info.mem_usage = 0;
            } else {
                node->info.status = PROCESS_STATUS_EXITED;
            }
        }
#else
        // Unix 系统实现
        int status;
        pid_t result = waitpid(node->info.pid, &status, WNOHANG);
        if (result == 0) {
            node->info.status = PROCESS_STATUS_RUNNING;
            
            // 更新进程运行时间
            node->info.uptime = time(NULL) - node->info.start_time;
            
            // 更新CPU和内存使用率（简化实现）
            node->info.cpu_usage = 0;
            node->info.mem_usage = 0;
        } else if (result == node->info.pid) {
            node->info.status = PROCESS_STATUS_EXITED;
        }
#endif
        node = node->next;
    }
}

/******************************************************************************************
 * 进程管理对外接口
 ******************************************************************************************/

int process_init(void) {
    if (g_process_manager.initialized) {
        LOG_WARN("Process manager already initialized");
        return SUCCESS;
    }
    
#ifdef _WIN32
    // 创建互斥锁
    g_process_manager.hMutex = CreateMutex(NULL, FALSE, NULL);
    if (!g_process_manager.hMutex) {
        LOG_ERROR("Failed to create mutex: %d", GetLastError());
        return FAILURE;
    }
#endif
    
    g_process_manager.process_list = NULL;
    g_process_manager.process_count = 0;
    g_process_manager.initialized = true;
    
    LOG_INFO("Process manager initialized");
    return SUCCESS;
}

int process_deinit(void) {
    if (!g_process_manager.initialized) {
        LOG_WARN("Process manager not initialized");
        return SUCCESS;
    }
    
    process_lock();
    
    // 终止所有进程
    ProcessNode_t *node = g_process_manager.process_list;
    while (node) {
        ProcessNode_t *next = node->next;
        
        if (node->info.status == PROCESS_STATUS_RUNNING) {
            process_terminate(node->info.pid);
        }
        
#ifdef _WIN32
        if (node->hProcess) {
            CloseHandle(node->hProcess);
        }
        if (node->hThread) {
            CloseHandle(node->hThread);
        }
#else
        if (node->pipe_fd[0] != -1) {
            close(node->pipe_fd[0]);
        }
        if (node->pipe_fd[1] != -1) {
            close(node->pipe_fd[1]);
        }
#endif
        
        free(node);
        node = next;
    }
    
    g_process_manager.process_list = NULL;
    g_process_manager.process_count = 0;
    
#ifdef _WIN32
    if (g_process_manager.hMutex) {
        CloseHandle(g_process_manager.hMutex);
        g_process_manager.hMutex = NULL;
    }
#else
    MUTEX_LOCK_DESTROY(g_process_manager.mutex);
#endif
    
    g_process_manager.initialized = false;
    process_unlock();
    
    LOG_INFO("Process manager deinitialized");
    return SUCCESS;
}

pid_t process_create(ProcessType_t type, const char *name, int priority) {
    if (!g_process_manager.initialized) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    if (type >= PROCESS_TYPE_MAX) {
        LOG_ERROR("Invalid process type: %d", type);
        return -1;
    }
    
    if (!name) {
        LOG_ERROR("Invalid process name");
        return -1;
    }
    
    process_lock();
    
    // 创建进程节点
    ProcessNode_t *node = (ProcessNode_t *)malloc(sizeof(ProcessNode_t));
    if (!node) {
        LOG_ERROR("Failed to allocate process node");
        process_unlock();
        return -1;
    }
    
    // 初始化进程节点
    memset(node, 0, sizeof(ProcessNode_t));
    node->info.type = type;
    strncpy(node->info.name, name, sizeof(node->info.name) - 1);
    node->info.priority = priority;
    node->info.start_time = GetTickCount();
    node->info.uptime = 0;
    node->info.cpu_usage = 0;
    node->info.mem_usage = 0;
    node->info.status = PROCESS_STATUS_IDLE;
    node->next = NULL;
    
#ifdef _WIN32
    // Windows 进程创建（简化实现）
    // 注意：在实际应用中，这里应该创建真正的进程
    // 由于是示例，我们只创建一个模拟的进程
    node->hProcess = NULL;
    node->hThread = NULL;
    node->info.pid = GetCurrentProcessId(); // 使用当前进程ID作为模拟
    node->info.status = PROCESS_STATUS_RUNNING;
#else
    // Unix 进程创建
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        LOG_ERROR("Failed to create pipe: %d", errno);
        free(node);
        process_unlock();
        return -1;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        LOG_ERROR("Failed to fork process: %d", errno);
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        free(node);
        process_unlock();
        return -1;
    } else if (pid == 0) {
        // 子进程
        close(pipe_fd[0]); // 关闭读端
        
        // 子进程逻辑
        LOG_INFO("Child process created: %s, pid: %d", name, getpid());
        
        // 模拟子进程工作
        while (1) {
            sleep(1);
        }
        
        exit(0);
    } else {
        // 父进程
        close(pipe_fd[1]); // 关闭写端
        node->pipe_fd[0] = pipe_fd[0];
        node->pipe_fd[1] = -1;
        node->info.pid = pid;
        node->info.status = PROCESS_STATUS_RUNNING;
    }
#endif
    
    // 添加进程节点到列表
    process_add_node(node);
    
    process_unlock();
    
    LOG_INFO("Process created: %s, type: %d, pid: %d", name, type, node->info.pid);
    return node->info.pid;
}

int process_terminate(pid_t pid) {
    if (!g_process_manager.initialized) {
        LOG_ERROR("Process manager not initialized");
        return FAILURE;
    }
    
    process_lock();
    
    ProcessNode_t *node = process_find_node(pid);
    if (!node) {
        LOG_ERROR("Process not found: %d", pid);
        process_unlock();
        return FAILURE;
    }
    
#ifdef _WIN32
    // Windows 进程终止
    if (node->hProcess) {
        if (!TerminateProcess(node->hProcess, 0)) {
            LOG_ERROR("Failed to terminate process: %d", GetLastError());
            process_unlock();
            return FAILURE;
        }
    }
#else
    // Unix 进程终止
    if (kill(pid, SIGTERM) == -1) {
        LOG_ERROR("Failed to terminate process: %d", errno);
        process_unlock();
        return FAILURE;
    }
    
    // 等待进程退出
    waitpid(pid, NULL, 0);
#endif
    
    node->info.status = PROCESS_STATUS_EXITED;
    
    // 从列表中移除进程节点
    process_remove_node(pid);
    
    process_unlock();
    
    LOG_INFO("Process terminated: %d", pid);
    return SUCCESS;
}

int process_get_info(pid_t pid, ProcessInfo_t *info) {
    if (!g_process_manager.initialized) {
        LOG_ERROR("Process manager not initialized");
        return FAILURE;
    }
    
    if (!info) {
        LOG_ERROR("Invalid process info pointer");
        return FAILURE;
    }
    
    process_lock();
    
    // 更新进程状态
    process_update_status();
    
    // 查找进程
    ProcessNode_t *node = process_find_node(pid);
    if (!node) {
        LOG_ERROR("Process not found: %d", pid);
        process_unlock();
        return FAILURE;
    }
    
    // 复制进程信息
    *info = node->info;
    
    process_unlock();
    return SUCCESS;
}

int process_get_all_info(ProcessInfo_t *infos, int max_count, int *count) {
    if (!g_process_manager.initialized) {
        LOG_ERROR("Process manager not initialized");
        return FAILURE;
    }
    
    if (!infos || !count) {
        LOG_ERROR("Invalid parameters");
        return FAILURE;
    }
    
    process_lock();
    
    // 更新进程状态
    process_update_status();
    
    // 复制进程信息
    int i = 0;
    ProcessNode_t *node = g_process_manager.process_list;
    while (node && i < max_count) {
        infos[i] = node->info;
        i++;
        node = node->next;
    }
    
    *count = i;
    
    process_unlock();
    return SUCCESS;
}

int process_send_message(pid_t pid, const void *msg, int msg_len) {
    if (!g_process_manager.initialized) {
        LOG_ERROR("Process manager not initialized");
        return FAILURE;
    }
    
    if (!msg || msg_len <= 0) {
        LOG_ERROR("Invalid message");
        return FAILURE;
    }
    
    process_lock();
    
    ProcessNode_t *node = process_find_node(pid);
    if (!node) {
        LOG_ERROR("Process not found: %d", pid);
        process_unlock();
        return FAILURE;
    }
    
    if (node->info.status != PROCESS_STATUS_RUNNING) {
        LOG_ERROR("Process not running: %d", pid);
        process_unlock();
        return FAILURE;
    }
    
#ifdef _WIN32
    // Windows 进程间通信（简化实现）
    LOG_INFO("Sending message to process: %d, length: %d", pid, msg_len);
#else
    // Unix 进程间通信
    if (write(node->pipe_fd[0], msg, msg_len) != msg_len) {
        LOG_ERROR("Failed to send message: %d", errno);
        process_unlock();
        return FAILURE;
    }
#endif
    
    process_unlock();
    return SUCCESS;
}

int process_receive_message(pid_t pid, void *msg, int msg_len) {
    if (!g_process_manager.initialized) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    if (!msg || msg_len <= 0) {
        LOG_ERROR("Invalid message buffer");
        return -1;
    }
    
    process_lock();
    
    ProcessNode_t *node = process_find_node(pid);
    if (!node) {
        LOG_ERROR("Process not found: %d", pid);
        process_unlock();
        return -1;
    }
    
    if (node->info.status != PROCESS_STATUS_RUNNING) {
        LOG_ERROR("Process not running: %d", pid);
        process_unlock();
        return -1;
    }
    
    int read_len = 0;
#ifdef _WIN32
    // Windows 进程间通信（简化实现）
    LOG_INFO("Receiving message from process: %d, length: %d", pid, msg_len);
    read_len = msg_len;
#else
    // Unix 进程间通信
    read_len = read(node->pipe_fd[0], msg, msg_len);
    if (read_len == -1) {
        LOG_ERROR("Failed to receive message: %d", errno);
        process_unlock();
        return -1;
    }
#endif
    
    process_unlock();
    return read_len;
}

bool process_is_running(pid_t pid) {
    if (!g_process_manager.initialized) {
        return false;
    }
    
    process_lock();
    
    ProcessNode_t *node = process_find_node(pid);
    if (!node) {
        process_unlock();
        return false;
    }
    
    // 更新进程状态
    process_update_status();
    
    bool running = (node->info.status == PROCESS_STATUS_RUNNING);
    
    process_unlock();
    return running;
}

int process_set_priority(pid_t pid, int priority) {
    if (!g_process_manager.initialized) {
        LOG_ERROR("Process manager not initialized");
        return FAILURE;
    }
    
    process_lock();
    
    ProcessNode_t *node = process_find_node(pid);
    if (!node) {
        LOG_ERROR("Process not found: %d", pid);
        process_unlock();
        return FAILURE;
    }
    
    node->info.priority = priority;
    
#ifdef _WIN32
    // Windows 进程优先级设置
    if (node->hProcess) {
        DWORD win_priority = NORMAL_PRIORITY_CLASS;
        if (priority > 0) {
            win_priority = HIGH_PRIORITY_CLASS;
        } else if (priority < 0) {
            win_priority = BELOW_NORMAL_PRIORITY_CLASS;
        }
        
        if (!SetPriorityClass(node->hProcess, win_priority)) {
            LOG_ERROR("Failed to set process priority: %d", GetLastError());
            process_unlock();
            return FAILURE;
        }
    }
#else
    // Unix 进程优先级设置
    int nice_value = 0;
    if (priority > 0) {
        nice_value = -5;
    } else if (priority < 0) {
        nice_value = 5;
    }
    
    if (setpriority(PRIO_PROCESS, pid, nice_value) == -1) {
        LOG_ERROR("Failed to set process priority: %d", errno);
        process_unlock();
        return FAILURE;
    }
#endif
    
    process_unlock();
    
    LOG_INFO("Process priority set: %d, priority: %d", pid, priority);
    return SUCCESS;
}

int process_get_cpu_usage(pid_t pid) {
    if (!g_process_manager.initialized) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    process_lock();
    
    ProcessNode_t *node = process_find_node(pid);
    if (!node) {
        LOG_ERROR("Process not found: %d", pid);
        process_unlock();
        return -1;
    }
    
    // 更新进程状态
    process_update_status();
    
    int cpu_usage = node->info.cpu_usage;
    
    process_unlock();
    return cpu_usage;
}

int process_get_memory_usage(pid_t pid) {
    if (!g_process_manager.initialized) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    process_lock();
    
    ProcessNode_t *node = process_find_node(pid);
    if (!node) {
        LOG_ERROR("Process not found: %d", pid);
        process_unlock();
        return -1;
    }
    
    // 更新进程状态
    process_update_status();
    
    int mem_usage = node->info.mem_usage;
    
    process_unlock();
    return mem_usage;
}
