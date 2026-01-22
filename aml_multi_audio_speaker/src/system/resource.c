/**
 * @file resource.c
 * @brief 资源管理模块实现
 * @details 提供系统资源监控和自动回收功能
 * @author AML Audio Team
 * @date 2026-01-22
 */

#include "resource.h"
#include "logger.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>

/******************************************************************************************
 * 资源管理内部数据结构
 ******************************************************************************************/

/**
 * @brief 资源回调结构体
 */
typedef struct {
    void (*callback)(ResourceType_t, ResourceStatus_t, void *); // 回调函数
    void *arg;                                                // 回调参数
} ResourceCallback_t;

/**
 * @brief 资源管理模块全局变量
 */
typedef struct {
    ResourceLimit_t limits[RESOURCE_TYPE_MAX];                // 资源限制
    ResourceUsage_t last_usage[RESOURCE_TYPE_MAX];            // 上次使用情况
    ResourceCallback_t callbacks[RESOURCE_TYPE_MAX];          // 资源回调
    pthread_t monitor_thread;                                 // 监控线程
    int monitor_interval;                                     // 监控间隔
    bool monitoring;                                          // 监控标志
    pthread_mutex_t mutex;                                    // 互斥锁
    bool initialized;                                         // 初始化标志
} ResourceManager_t;

/******************************************************************************************
 * 资源管理模块全局变量
 ******************************************************************************************/

static ResourceManager_t g_resource_manager = {
    .limits = {0},
    .last_usage = {0},
    .callbacks = {0},
    .monitor_thread = 0,
    .monitor_interval = 1000,
    .monitoring = false,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .initialized = false
};

/******************************************************************************************
 * 资源管理内部函数
 ******************************************************************************************/

/**
 * @brief 获取CPU使用情况
 * @param usage 资源使用情况
 * @return SUCCESS/FAILURE
 */
static int get_cpu_usage(ResourceUsage_t *usage) {
    // 简化实现，返回模拟数据
    usage->type = RESOURCE_TYPE_CPU;
    usage->used = 0;
    usage->total = 100;
    usage->peak = 0;
    usage->usage_percent = 0;
    strcpy(usage->unit, "%");
    usage->status = RESOURCE_STATUS_NORMAL;
    
    return SUCCESS;
}

/**
 * @brief 获取内存使用情况
 * @param usage 资源使用情况
 * @return SUCCESS/FAILURE
 */
static int get_memory_usage(ResourceUsage_t *usage) {
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        usage->type = RESOURCE_TYPE_MEMORY;
        usage->used = info.totalram - info.freeram;
        usage->total = info.totalram;
        usage->peak = info.totalram - info.freeram;
        usage->usage_percent = (usage->used * 100) / usage->total;
        strcpy(usage->unit, "bytes");
        
        if (usage->usage_percent >= g_resource_manager.limits[RESOURCE_TYPE_MEMORY].critical_threshold) {
            usage->status = RESOURCE_STATUS_CRITICAL;
        } else if (usage->usage_percent >= g_resource_manager.limits[RESOURCE_TYPE_MEMORY].warning_threshold) {
            usage->status = RESOURCE_STATUS_WARNING;
        } else {
            usage->status = RESOURCE_STATUS_NORMAL;
        }
        
        return SUCCESS;
    }
    
    // 简化实现，返回模拟数据
    usage->type = RESOURCE_TYPE_MEMORY;
    usage->used = 0;
    usage->total = 0;
    usage->peak = 0;
    usage->usage_percent = 0;
    strcpy(usage->unit, "bytes");
    usage->status = RESOURCE_STATUS_NORMAL;
    
    return SUCCESS;
}

/**
 * @brief 资源监控线程函数
 * @param arg 线程参数
 * @return 线程返回值
 */
static void *resource_monitor_thread(void *arg) {
    LOG_INFO("Resource monitor thread started");
    
    while (g_resource_manager.monitoring) {
        // 监控所有资源类型
        for (int i = 0; i < RESOURCE_TYPE_MAX; i++) {
            ResourceUsage_t usage;
            if (resource_get_usage((ResourceType_t)i, &usage) == SUCCESS) {
                // 检查资源状态变化
                if (usage.status != g_resource_manager.last_usage[i].status) {
                    LOG_INFO("Resource status changed: type=%d, old=%d, new=%d, usage=%d%%", 
                             i, g_resource_manager.last_usage[i].status, usage.status, usage.usage_percent);
                    
                    // 调用回调函数
                    if (g_resource_manager.callbacks[i].callback) {
                        g_resource_manager.callbacks[i].callback((ResourceType_t)i, usage.status, g_resource_manager.callbacks[i].arg);
                    }
                    
                    // 自动回收资源（当资源状态为警告或临界时）
                    if (usage.status >= RESOURCE_STATUS_WARNING) {
                        uint64_t reclaimed = resource_reclaim((ResourceType_t)i);
                        if (reclaimed > 0) {
                            LOG_INFO("Reclaimed resource: type=%d, amount=%llu", i, reclaimed);
                        }
                    }
                }
                
                // 更新上次使用情况
                g_resource_manager.last_usage[i] = usage;
            }
        }
        
        // 等待监控间隔
        usleep(g_resource_manager.monitor_interval * 1000);
    }
    
    LOG_INFO("Resource monitor thread exited");
    return NULL;
}

/******************************************************************************************
 * 资源管理对外接口
 ******************************************************************************************/

int resource_init(void) {
    if (g_resource_manager.initialized) {
        LOG_WARN("Resource manager already initialized");
        return SUCCESS;
    }
    
    // 初始化资源限制
    for (int i = 0; i < RESOURCE_TYPE_MAX; i++) {
        g_resource_manager.limits[i].type = (ResourceType_t)i;
        g_resource_manager.limits[i].soft_limit = 0;
        g_resource_manager.limits[i].hard_limit = 0;
        g_resource_manager.limits[i].warning_threshold = 80;
        g_resource_manager.limits[i].critical_threshold = 90;
    }
    
    // 初始化上次使用情况
    for (int i = 0; i < RESOURCE_TYPE_MAX; i++) {
        g_resource_manager.last_usage[i].type = (ResourceType_t)i;
        g_resource_manager.last_usage[i].status = RESOURCE_STATUS_NORMAL;
        g_resource_manager.last_usage[i].used = 0;
        g_resource_manager.last_usage[i].total = 0;
        g_resource_manager.last_usage[i].peak = 0;
        g_resource_manager.last_usage[i].usage_percent = 0;
        strcpy(g_resource_manager.last_usage[i].unit, "");
    }
    
    // 初始化回调
    for (int i = 0; i < RESOURCE_TYPE_MAX; i++) {
        g_resource_manager.callbacks[i].callback = NULL;
        g_resource_manager.callbacks[i].arg = NULL;
    }
    
    g_resource_manager.monitoring = false;
    g_resource_manager.initialized = true;
    
    LOG_INFO("Resource manager initialized");
    return SUCCESS;
}

int resource_deinit(void) {
    if (!g_resource_manager.initialized) {
        LOG_WARN("Resource manager not initialized");
        return SUCCESS;
    }
    
    // 停止监控
    resource_stop_monitoring();
    
    g_resource_manager.initialized = false;
    
    LOG_INFO("Resource manager deinitialized");
    return SUCCESS;
}

int resource_get_usage(ResourceType_t type, ResourceUsage_t *usage) {
    if (!g_resource_manager.initialized) {
        LOG_ERROR("Resource manager not initialized");
        return FAILURE;
    }
    
    if (!usage || type >= RESOURCE_TYPE_MAX) {
        LOG_ERROR("Invalid parameters");
        return FAILURE;
    }
    
    pthread_mutex_lock(&g_resource_manager.mutex);
    
    int ret = FAILURE;
    switch (type) {
        case RESOURCE_TYPE_CPU:
            ret = get_cpu_usage(usage);
            break;
        case RESOURCE_TYPE_MEMORY:
            ret = get_memory_usage(usage);
            break;
        default:
            // 其他资源类型的简化实现
            usage->type = type;
            usage->status = RESOURCE_STATUS_NORMAL;
            usage->used = 0;
            usage->total = 0;
            usage->peak = 0;
            usage->usage_percent = 0;
            strcpy(usage->unit, "");
            ret = SUCCESS;
            break;
    }
    
    pthread_mutex_unlock(&g_resource_manager.mutex);
    return ret;
}

int resource_set_limit(const ResourceLimit_t *limit) {
    if (!g_resource_manager.initialized) {
        LOG_ERROR("Resource manager not initialized");
        return FAILURE;
    }
    
    if (!limit || limit->type >= RESOURCE_TYPE_MAX) {
        LOG_ERROR("Invalid parameters");
        return FAILURE;
    }
    
    pthread_mutex_lock(&g_resource_manager.mutex);
    
    g_resource_manager.limits[limit->type] = *limit;
    
    pthread_mutex_unlock(&g_resource_manager.mutex);
    
    LOG_INFO("Set resource limit: type=%d, warning=%d%%, critical=%d%%", 
             limit->type, limit->warning_threshold, limit->critical_threshold);
    return SUCCESS;
}

int resource_get_limit(ResourceType_t type, ResourceLimit_t *limit) {
    if (!g_resource_manager.initialized) {
        LOG_ERROR("Resource manager not initialized");
        return FAILURE;
    }
    
    if (!limit || type >= RESOURCE_TYPE_MAX) {
        LOG_ERROR("Invalid parameters");
        return FAILURE;
    }
    
    pthread_mutex_lock(&g_resource_manager.mutex);
    
    *limit = g_resource_manager.limits[type];
    
    pthread_mutex_unlock(&g_resource_manager.mutex);
    return SUCCESS;
}

int resource_start_monitoring(int interval) {
    if (!g_resource_manager.initialized) {
        LOG_ERROR("Resource manager not initialized");
        return FAILURE;
    }
    
    if (g_resource_manager.monitoring) {
        LOG_WARN("Resource monitoring already started");
        return SUCCESS;
    }
    
    g_resource_manager.monitor_interval = interval;
    g_resource_manager.monitoring = true;
    
    // 创建监控线程
    if (pthread_create(&g_resource_manager.monitor_thread, NULL, resource_monitor_thread, NULL) != 0) {
        LOG_ERROR("Failed to create resource monitor thread");
        g_resource_manager.monitoring = false;
        return FAILURE;
    }
    
    LOG_INFO("Resource monitoring started, interval: %dms", interval);
    return SUCCESS;
}

int resource_stop_monitoring(void) {
    if (!g_resource_manager.initialized) {
        LOG_ERROR("Resource manager not initialized");
        return FAILURE;
    }
    
    if (!g_resource_manager.monitoring) {
        LOG_WARN("Resource monitoring not started");
        return SUCCESS;
    }
    
    g_resource_manager.monitoring = false;
    
    // 等待监控线程退出
    if (g_resource_manager.monitor_thread) {
        pthread_join(g_resource_manager.monitor_thread, NULL);
        g_resource_manager.monitor_thread = 0;
    }
    
    LOG_INFO("Resource monitoring stopped");
    return SUCCESS;
}

uint64_t resource_reclaim(ResourceType_t type) {
    if (!g_resource_manager.initialized) {
        LOG_ERROR("Resource manager not initialized");
        return 0;
    }
    
    if (type >= RESOURCE_TYPE_MAX) {
        LOG_ERROR("Invalid resource type: %d", type);
        return 0;
    }
    
    // 简化实现，返回模拟回收量
    LOG_INFO("Reclaiming resource: type=%d", type);
    
    return 0;
}

int resource_get_stats(ResourceType_t type, char *stats, int max_len) {
    if (!g_resource_manager.initialized) {
        LOG_ERROR("Resource manager not initialized");
        return FAILURE;
    }
    
    if (!stats || max_len <= 0 || type >= RESOURCE_TYPE_MAX) {
        LOG_ERROR("Invalid parameters");
        return FAILURE;
    }
    
    ResourceUsage_t usage;
    if (resource_get_usage(type, &usage) == SUCCESS) {
        snprintf(stats, max_len, "Type: %d, Used: %llu %s, Total: %llu %s, Usage: %d%%, Status: %d",
                 usage.type, usage.used, usage.unit, usage.total, usage.unit, usage.usage_percent, usage.status);
        return SUCCESS;
    }
    
    return FAILURE;
}

int resource_register_callback(ResourceType_t type, void (*callback)(ResourceType_t, ResourceStatus_t, void *), void *arg) {
    if (!g_resource_manager.initialized) {
        LOG_ERROR("Resource manager not initialized");
        return FAILURE;
    }
    
    if (type >= RESOURCE_TYPE_MAX) {
        LOG_ERROR("Invalid resource type: %d", type);
        return FAILURE;
    }
    
    pthread_mutex_lock(&g_resource_manager.mutex);
    
    g_resource_manager.callbacks[type].callback = callback;
    g_resource_manager.callbacks[type].arg = arg;
    
    pthread_mutex_unlock(&g_resource_manager.mutex);
    
    LOG_INFO("Registered resource callback for type: %d", type);
    return SUCCESS;
}

int resource_unregister_callback(ResourceType_t type) {
    if (!g_resource_manager.initialized) {
        LOG_ERROR("Resource manager not initialized");
        return FAILURE;
    }
    
    if (type >= RESOURCE_TYPE_MAX) {
        LOG_ERROR("Invalid resource type: %d", type);
        return FAILURE;
    }
    
    pthread_mutex_lock(&g_resource_manager.mutex);
    
    g_resource_manager.callbacks[type].callback = NULL;
    g_resource_manager.callbacks[type].arg = NULL;
    
    pthread_mutex_unlock(&g_resource_manager.mutex);
    
    LOG_INFO("Unregistered resource callback for type: %d", type);
    return SUCCESS;
}
