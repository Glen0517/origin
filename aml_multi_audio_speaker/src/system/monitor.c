/**
 * @file monitor.c
 * @brief 监控和诊断模块实现
 * @details 提供系统监控和诊断功能，包括进程状态实时监控
 * @author AML Audio Team
 * @date 2026-01-22
 */

#include "monitor.h"
#include "logger.h"
#include "process.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/******************************************************************************************
 * 监控和诊断内部数据结构
 ******************************************************************************************/

/**
 * @brief 监控事件节点
 */
typedef struct MonitorEventNode {
    MonitorEvent_t event;              // 事件结构体
    struct MonitorEventNode *next;     // 指向下一个节点
} MonitorEventNode_t;

/**
 * @brief 监控和诊断模块全局变量
 */
typedef struct {
    MonitorEventNode_t *event_list;     // 事件列表
    pthread_t monitor_thread;           // 监控线程
    int monitor_interval;               // 监控间隔
    bool monitoring;                    // 监控标志
    void (*event_callback)(const MonitorEvent_t *, void *); // 事件回调函数
    void *callback_arg;                 // 回调参数
    pthread_mutex_t mutex;              // 互斥锁
    bool initialized;                   // 初始化标志
} MonitorManager_t;

/******************************************************************************************
 * 监控和诊断模块全局变量
 ******************************************************************************************/

static MonitorManager_t g_monitor_manager = {
    .event_list = NULL,
    .monitor_thread = 0,
    .monitor_interval = 1000,
    .monitoring = false,
    .event_callback = NULL,
    .callback_arg = NULL,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .initialized = false
};

/******************************************************************************************
 * 监控和诊断内部函数
 ******************************************************************************************/

/**
 * @brief 添加监控事件
 * @param event 监控事件
 * @return SUCCESS/FAILURE
 */
static int add_event(const MonitorEvent_t *event) {
    pthread_mutex_lock(&g_monitor_manager.mutex);
    
    // 创建事件节点
    MonitorEventNode_t *node = (MonitorEventNode_t *)malloc(sizeof(MonitorEventNode_t));
    if (!node) {
        LOG_ERROR("Failed to allocate event node");
        pthread_mutex_unlock(&g_monitor_manager.mutex);
        return FAILURE;
    }
    
    // 复制事件信息
    node->event = *event;
    node->next = NULL;
    
    // 添加到事件列表
    if (!g_monitor_manager.event_list) {
        g_monitor_manager.event_list = node;
    } else {
        MonitorEventNode_t *current = g_monitor_manager.event_list;
        while (current->next) {
            current = current->next;
        }
        current->next = node;
    }
    
    pthread_mutex_unlock(&g_monitor_manager.mutex);
    return SUCCESS;
}

/**
 * @brief 清理事件列表
 */
static void cleanup_events(void) {
    pthread_mutex_lock(&g_monitor_manager.mutex);
    
    MonitorEventNode_t *node = g_monitor_manager.event_list;
    while (node) {
        MonitorEventNode_t *next = node->next;
        free(node);
        node = next;
    }
    
    g_monitor_manager.event_list = NULL;
    pthread_mutex_unlock(&g_monitor_manager.mutex);
}

/**
 * @brief 监控线程函数
 * @param arg 线程参数
 * @return 线程返回值
 */
static void *monitor_thread_func(void *arg) {
    LOG_INFO("Monitor thread started");
    
    while (g_monitor_manager.monitoring) {
        // 监控进程状态
        ProcessInfo_t *infos = NULL;
        int max_count = 100;
        int count = 0;
        
        infos = (ProcessInfo_t *)malloc(sizeof(ProcessInfo_t) * max_count);
        if (infos) {
            if (process_get_all_info(infos, max_count, &count) == SUCCESS) {
                for (int i = 0; i < count; i++) {
                    // 检查进程状态
                    if (infos[i].status == PROCESS_STATUS_EXITED) {
                        // 记录进程退出事件
                        MonitorEvent_t event;
                        event.type = MONITOR_EVENT_PROCESS_EXIT;
                        event.timestamp = time(NULL);
                        event.pid = infos[i].pid;
                        event.code = 0;
                        snprintf(event.message, sizeof(event.message), "Process %s exited", infos[i].name);
                        monitor_record_event(&event);
                    }
                }
            }
            free(infos);
        }
        
        // 等待监控间隔
        usleep(g_monitor_manager.monitor_interval * 1000);
    }
    
    LOG_INFO("Monitor thread exited");
    return NULL;
}

/******************************************************************************************
 * 监控和诊断对外接口
 ******************************************************************************************/

int monitor_init(void) {
    if (g_monitor_manager.initialized) {
        LOG_WARN("Monitor already initialized");
        return SUCCESS;
    }
    
    g_monitor_manager.event_list = NULL;
    g_monitor_manager.monitor_interval = 1000;
    g_monitor_manager.monitoring = false;
    g_monitor_manager.event_callback = NULL;
    g_monitor_manager.callback_arg = NULL;
    g_monitor_manager.initialized = true;
    
    LOG_INFO("Monitor initialized");
    return SUCCESS;
}

int monitor_deinit(void) {
    if (!g_monitor_manager.initialized) {
        LOG_WARN("Monitor not initialized");
        return SUCCESS;
    }
    
    // 停止监控
    monitor_stop();
    
    // 清理事件列表
    cleanup_events();
    
    g_monitor_manager.initialized = false;
    
    LOG_INFO("Monitor deinitialized");
    return SUCCESS;
}

int monitor_start(int interval) {
    if (!g_monitor_manager.initialized) {
        LOG_ERROR("Monitor not initialized");
        return FAILURE;
    }
    
    if (g_monitor_manager.monitoring) {
        LOG_WARN("Monitor already started");
        return SUCCESS;
    }
    
    g_monitor_manager.monitor_interval = interval;
    g_monitor_manager.monitoring = true;
    
    // 创建监控线程
    if (pthread_create(&g_monitor_manager.monitor_thread, NULL, monitor_thread_func, NULL) != 0) {
        LOG_ERROR("Failed to create monitor thread");
        g_monitor_manager.monitoring = false;
        return FAILURE;
    }
    
    LOG_INFO("Monitor started, interval: %dms", interval);
    return SUCCESS;
}

int monitor_stop(void) {
    if (!g_monitor_manager.initialized) {
        LOG_ERROR("Monitor not initialized");
        return FAILURE;
    }
    
    if (!g_monitor_manager.monitoring) {
        LOG_WARN("Monitor not started");
        return SUCCESS;
    }
    
    g_monitor_manager.monitoring = false;
    
    // 等待监控线程退出
    if (g_monitor_manager.monitor_thread) {
        pthread_join(g_monitor_manager.monitor_thread, NULL);
        g_monitor_manager.monitor_thread = 0;
    }
    
    LOG_INFO("Monitor stopped");
    return SUCCESS;
}

int monitor_get_system_status(SystemStatus_t *status) {
    if (!g_monitor_manager.initialized) {
        LOG_ERROR("Monitor not initialized");
        return FAILURE;
    }
    
    if (!status) {
        LOG_ERROR("Invalid parameter");
        return FAILURE;
    }
    
    // 简化实现，返回模拟数据
    status->uptime = time(NULL);
    status->process_count = 0;
    status->thread_count = 0;
    status->cpu_usage = 0;
    status->memory_usage = 0;
    status->disk_usage = 0;
    status->network_rx = 0;
    status->network_tx = 0;
    
    return SUCCESS;
}

int monitor_get_process_status(pid_t pid, ProcessInfo_t *info) {
    if (!g_monitor_manager.initialized) {
        LOG_ERROR("Monitor not initialized");
        return FAILURE;
    }
    
    if (!info) {
        LOG_ERROR("Invalid parameter");
        return FAILURE;
    }
    
    return process_get_info(pid, info);
}

int monitor_record_event(const MonitorEvent_t *event) {
    if (!g_monitor_manager.initialized) {
        LOG_ERROR("Monitor not initialized");
        return FAILURE;
    }
    
    if (!event) {
        LOG_ERROR("Invalid parameter");
        return FAILURE;
    }
    
    // 添加事件到列表
    if (add_event(event) != SUCCESS) {
        LOG_ERROR("Failed to add event");
        return FAILURE;
    }
    
    // 调用事件回调函数
    if (g_monitor_manager.event_callback) {
        g_monitor_manager.event_callback(event, g_monitor_manager.callback_arg);
    }
    
    // 记录事件日志
    LOG_INFO("Monitor event: type=%d, pid=%d, message=%s", 
             event->type, event->pid, event->message);
    
    return SUCCESS;
}

int monitor_register_callback(void (*callback)(const MonitorEvent_t *, void *), void *arg) {
    if (!g_monitor_manager.initialized) {
        LOG_ERROR("Monitor not initialized");
        return FAILURE;
    }
    
    g_monitor_manager.event_callback = callback;
    g_monitor_manager.callback_arg = arg;
    
    LOG_INFO("Registered monitor event callback");
    return SUCCESS;
}

int monitor_unregister_callback(void) {
    if (!g_monitor_manager.initialized) {
        LOG_ERROR("Monitor not initialized");
        return FAILURE;
    }
    
    g_monitor_manager.event_callback = NULL;
    g_monitor_manager.callback_arg = NULL;
    
    LOG_INFO("Unregistered monitor event callback");
    return SUCCESS;
}

int monitor_get_log(char *buffer, int max_len) {
    if (!g_monitor_manager.initialized) {
        LOG_ERROR("Monitor not initialized");
        return -1;
    }
    
    if (!buffer || max_len <= 0) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    pthread_mutex_lock(&g_monitor_manager.mutex);
    
    int offset = 0;
    MonitorEventNode_t *node = g_monitor_manager.event_list;
    
    while (node && offset < max_len - 1) {
        time_t timestamp = node->event.timestamp;
        struct tm *tm_info = localtime(&timestamp);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        
        int len = snprintf(buffer + offset, max_len - offset, 
                          "[%s] TYPE=%d, PID=%d, CODE=%d, MESSAGE=%s\n",
                          time_str, node->event.type, node->event.pid, 
                          node->event.code, node->event.message);
        
        if (len > 0) {
            offset += len;
        }
        
        node = node->next;
    }
    
    pthread_mutex_unlock(&g_monitor_manager.mutex);
    
    return offset;
}

int monitor_clear_log(void) {
    if (!g_monitor_manager.initialized) {
        LOG_ERROR("Monitor not initialized");
        return FAILURE;
    }
    
    cleanup_events();
    
    LOG_INFO("Cleared monitor log");
    return SUCCESS;
}

int monitor_trigger_diagnostic(int level, const char *output) {
    if (!g_monitor_manager.initialized) {
        LOG_ERROR("Monitor not initialized");
        return FAILURE;
    }
    
    // 简化实现，仅记录诊断事件
    MonitorEvent_t event;
    event.type = MONITOR_EVENT_SYSTEM_ERROR;
    event.timestamp = time(NULL);
    event.pid = getpid();
    event.code = level;
    snprintf(event.message, sizeof(event.message), "Triggered diagnostic level %d", level);
    monitor_record_event(&event);
    
    LOG_INFO("Triggered diagnostic: level=%d, output=%s", level, output ? output : "stdout");
    return SUCCESS;
}
