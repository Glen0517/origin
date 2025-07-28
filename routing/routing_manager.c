#include "routing_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <linux/slab.h>
#include "audio_processing.h"

// 全局路由管理器实例
static RoutingManager g_routing_manager;

// 修改为:
// 路由管理器实例指针
static RoutingManager* g_routing_manager = NULL;

// 静态函数声明
static int validate_route_rule(const RouteRule* rule);
static RouteRule* create_route_rule(const RouteRule* rule);
static void destroy_route_rule(RouteRule* rule);

// 添加创建函数
RoutingManager* routing_manager_create(const RoutingConfig* config)
{
    if (!config) return NULL;
    if (g_routing_manager) return NULL; // 确保单实例

    RoutingManager* rm = malloc(sizeof(RoutingManager));
    if (!rm) return NULL;

    memset(rm, 0, sizeof(RoutingManager));
    rm->config = *config;
    INIT_LIST_HEAD(&rm->routes);
    pthread_mutex_init(&rm->lock, NULL);
    rm->initialized = true;

    g_routing_manager = rm;
    return rm;
}

// 修改初始化函数
int routing_manager_init(const RoutingConfig* config)
{
    if (!config) return -EINVAL;
    if (g_routing_manager) return -EBUSY;

    // 创建并初始化路由管理器
    RoutingManager* rm = routing_manager_create(config);
    return rm ? 0 : -ENOMEM;
}

// 修改销毁函数
void routing_manager_destroy(void)
{
    if (!g_routing_manager) return;

    pthread_mutex_lock(&g_routing_manager->lock);

    // 删除所有路由规则
    RouteRule *rule, *temp;
    list_for_each_entry_safe(rule, temp, &g_routing_manager->routes, list) {
        list_del(&rule->list);
        destroy_route_rule(rule);
    }

    pthread_mutex_unlock(&g_routing_manager->lock);
    pthread_mutex_destroy(&g_routing_manager->lock);
    free(g_routing_manager);
    g_routing_manager = NULL;
}

// 修改获取实例函数
RoutingManager* routing_manager_get_instance(void)
{
    return g_routing_manager;
}

int routing_manager_add_route(const RouteRule* rule)
{
    if (!g_routing_manager.initialized || !rule) return -EINVAL;

    // 验证路由规则
    int ret = validate_route_rule(rule);
    if (ret != 0) return ret;

    pthread_mutex_lock(&g_routing_manager.lock);

    // 检查是否已存在相同的路由
    RouteRule* existing = routing_manager_find_route(rule->source.id, rule->sink.id);
    if (existing) {
        pthread_mutex_unlock(&g_routing_manager.lock);
        return -EEXIST;
    }

    // 检查路由数量是否已达上限
    unsigned int route_count = 0;
    RouteRule* r;
    list_for_each_entry(r, &g_routing_manager.routes, list) {
        route_count++;
    }

    if (route_count >= g_routing_manager.config.max_routes) {
        pthread_mutex_unlock(&g_routing_manager.lock);
        return -ENOSPC;
    }

    // 创建并添加新路由规则
    RouteRule* new_rule = create_route_rule(rule);
    if (!new_rule) {
        pthread_mutex_unlock(&g_routing_manager.lock);
        return -ENOMEM;
    }

    list_add_tail(&new_rule->list, &g_routing_manager.routes);

    // 调用回调函数通知路由已添加
    if (g_routing_manager.route_changed_cb) {
        g_routing_manager.route_changed_cb(new_rule, true);
    }

    pthread_mutex_unlock(&g_routing_manager.lock);
    return new_rule->route_id;
}

int routing_manager_remove_route(uint32_t route_id)
{
    if (!g_routing_manager.initialized) return -ENODEV;

    pthread_mutex_lock(&g_routing_manager.lock);

    RouteRule *rule, *temp;
    list_for_each_entry_safe(rule, temp, &g_routing_manager.routes, list) {
        if (rule->route_id == route_id) {
            list_del(&rule->list);

            // 调用回调函数通知路由已删除
            if (g_routing_manager.route_changed_cb) {
                g_routing_manager.route_changed_cb(rule, false);
            }

            destroy_route_rule(rule);
            pthread_mutex_unlock(&g_routing_manager.lock);
            return 0;
        }
    }

    pthread_mutex_unlock(&g_routing_manager.lock);
    return -ENOENT;
}

int routing_manager_update_route(const RouteRule* rule)
{
    if (!g_routing_manager.initialized || !rule) return -EINVAL;

    int ret = validate_route_rule(rule);
    if (ret != 0) return ret;

    pthread_mutex_lock(&g_routing_manager.lock);

    RouteRule* existing = NULL;
    RouteRule* r;
    list_for_each_entry(r, &g_routing_manager.routes, list) {
        if (r->route_id == rule->route_id) {
            existing = r;
            break;
        }
    }

    if (!existing) {
        pthread_mutex_unlock(&g_routing_manager.lock);
        return -ENOENT;
    }

    // 更新路由规则
    existing->source = rule->source;
    existing->sink = rule->sink;
    existing->type = rule->type;
    existing->priority = rule->priority;
    existing->enabled = rule->enabled;

    // 如果处理链不同，更新处理链
    if (existing->processing_chain != rule->processing_chain) {
        // 销毁旧的处理链
        if (existing->processing_chain) {
            audio_processing_chain_destroy(existing->processing_chain);
        }
        // 复制新的处理链
        if (rule->processing_chain) {
            existing->processing_chain = audio_processing_chain_copy(rule->processing_chain);
        } else {
            existing->processing_chain = NULL;
        }
    }

    pthread_mutex_unlock(&g_routing_manager.lock);
    return 0;
}

int routing_manager_get_routes(RouteRule* buffer, uint32_t max_count)
{
    if (!g_routing_manager.initialized || !buffer || max_count == 0) return -EINVAL;

    pthread_mutex_lock(&g_routing_manager.lock);

    unsigned int count = 0;
    RouteRule* rule;
    list_for_each_entry(rule, &g_routing_manager.routes, list) {
        if (count < max_count) {
            buffer[count] = *rule;
            count++;
        } else {
            break;
        }
    }

    pthread_mutex_unlock(&g_routing_manager.lock);
    return count;
}

RouteRule* routing_manager_find_route(uint32_t source_id, uint32_t sink_id)
{
    if (!g_routing_manager.initialized) return NULL;

    RouteRule* rule;
    list_for_each_entry(rule, &g_routing_manager.routes, list) {
        if (rule->source.id == source_id && rule->sink.id == sink_id) {
            return rule;
        }
    }

    return NULL;
}

int routing_manager_route_buffer(AudioEndpoint source, void* buffer, uint32_t frames)
{
    if (!g_routing_manager.initialized || !buffer || frames == 0) return -EINVAL;

    pthread_mutex_lock(&g_routing_manager.lock);

    // 查找所有匹配的源端点路由
    RouteRule* rule;
    int routes_count = 0;
    void** processed_buffers = NULL;

    // 首先计算匹配的路由数量
    list_for_each_entry(rule, &g_routing_manager.routes, list) {
        if (rule->enabled && rule->source.id == source.id) {
            routes_count++;
        }
    }

    if (routes_count == 0) {
        pthread_mutex_unlock(&g_routing_manager.lock);
        return 0;
    }

    // 为每个路由分配处理缓冲区
    processed_buffers = malloc(sizeof(void*) * routes_count);
    if (!processed_buffers) {
        pthread_mutex_unlock(&g_routing_manager.lock);
        return -ENOMEM;
    }

    // 处理并路由音频缓冲区
    int index = 0;
    list_for_each_entry(rule, &g_routing_manager.routes, list) {
        if (rule->enabled && rule->source.id == source.id) {
            // 根据路由类型处理音频
            switch (rule->type) {
                case ROUTE_TYPE_DIRECT:
                    // 直接路由，不做处理
                    processed_buffers[index] = buffer;
                    break;

                case ROUTE_TYPE_MIXED:
                    // 混合路由，创建副本
                    processed_buffers[index] = malloc(frames * sizeof(int16_t) * 2); // 假设16位立体声
                    if (processed_buffers[index]) {
                        memcpy(processed_buffers[index], buffer, frames * sizeof(int16_t) * 2);
                    }
                    break;

                case ROUTE_TYPE_PROCESSED:
                    // 处理路由，应用处理链
                    if (rule->processing_chain) {
                        processed_buffers[index] = audio_processing_apply(rule->processing_chain, buffer, frames);
                    } else {
                        processed_buffers[index] = buffer;
                    }
                    break;
            }

            // 将处理后的缓冲区发送到目标端点
            if (processed_buffers[index]) {
                // 这里应该有发送到目标端点的逻辑
                // send_to_endpoint(rule->sink, processed_buffers[index], frames);
                routes_count++;
            }

            index++;
        }
    }

    // 清理临时缓冲区
    for (int i = 0; i < index; i++) {
        if (rule->type == ROUTE_TYPE_MIXED && processed_buffers[i] != buffer) {
            free(processed_buffers[i]);
        }
    }

    free(processed_buffers);
    pthread_mutex_unlock(&g_routing_manager.lock);
    return routes_count;
}

void routing_manager_set_callback(void (*callback)(RouteRule* rule, bool added))
{
    if (!g_routing_manager.initialized) return;

    pthread_mutex_lock(&g_routing_manager.lock);
    g_routing_manager.route_changed_cb = callback;
    pthread_mutex_unlock(&g_routing_manager.lock);
}

// 静态辅助函数
static int validate_route_rule(const RouteRule* rule)
{
    if (!rule || !rule->source.name || !rule->sink.name) return -EINVAL;
    if (rule->priority > 255) return -EINVAL;
    if (rule->source.type != ENDPOINT_TYPE_SOURCE) return -EINVAL;
    if (rule->sink.type != ENDPOINT_TYPE_SINK) return -EINVAL;
    return 0;
}

static RouteRule* create_route_rule(const RouteRule* rule)
{
    RouteRule* new_rule = malloc(sizeof(RouteRule));
    if (!new_rule) return NULL;

    memset(new_rule, 0, sizeof(RouteRule));
    *new_rule = *rule;
    INIT_LIST_HEAD(&new_rule->list);

    // 复制字符串
    new_rule->source.name = strdup(rule->source.name);
    new_rule->source.device = rule->source.device ? strdup(rule->source.device) : NULL;
    new_rule->sink.name = strdup(rule->sink.name);
    new_rule->sink.device = rule->sink.device ? strdup(rule->sink.device) : NULL;

    // 如果有处理链，复制处理链
    if (rule->processing_chain) {
        new_rule->processing_chain = audio_processing_chain_copy(rule->processing_chain);
    }

    return new_rule;
}

static void destroy_route_rule(RouteRule* rule)
{
    if (!rule) return;

    free((void*)rule->source.name);
    free((void*)rule->source.device);
    free((void*)rule->sink.name);
    free((void*)rule->sink.device);

    // 销毁处理链
    if (rule->processing_chain) {
        audio_processing_chain_destroy(rule->processing_chain);
    }

    free(rule);
}

// 添加路由优先级枚举
typedef enum {
    ROUTE_PRIORITY_LOW = 0,
    ROUTE_PRIORITY_NORMAL = 1,
    ROUTE_PRIORITY_HIGH = 2,
    ROUTE_PRIORITY_CRITICAL = 3
} RoutePriority;

// 修改路由结构体，添加优先级字段
struct AudioRoute {
    char *name;
    char *input_device;
    char *output_device;
    AudioProcessingChain *processing_chain;
    bool active;
    RoutePriority priority;
    uint32_t route_id;
};

// 添加冲突解决策略枚举
typedef enum {
    CONFLICT_RESOLUTION_REPLACE_LOWER,
    CONFLICT_RESOLUTION_IGNORE_NEW,
    CONFLICT_RESOLUTION_MERGE,
    CONFLICT_RESOLUTION_ABORT
} ConflictResolutionPolicy;

// 修改路由管理器结构体
struct RoutingManager {
    struct pw_loop *loop;
    pthread_mutex_t mutex;
    struct AudioRoute **routes;
    uint32_t num_routes;
    uint32_t max_routes;
    uint32_t next_route_id;
    ConflictResolutionPolicy conflict_policy;
    RoutingManagerCallback *callback;
    void *user_data;
    // 保留现有结构体字段
    const RoutingConfig* config;
    bool initialized;
};

// 修改创建函数，添加冲突策略参数
RoutingManager* routing_manager_create(const RoutingConfig* config)
{
    if (!config) return NULL;

    RoutingManager *rm = malloc(sizeof(RoutingManager));
    if (!rm) return NULL;

    memset(rm, 0, sizeof(RoutingManager));
    rm->config = config;
    rm->max_routes = config->max_routes > 0 ? config->max_routes : 10;
    rm->conflict_policy = CONFLICT_RESOLUTION_REPLACE_LOWER; // 默认策略
    rm->routes = calloc(rm->max_routes, sizeof(struct AudioRoute*));
    rm->num_routes = 0;
    rm->next_route_id = 1;
    rm->callback = NULL;
    rm->user_data = NULL;
    rm->initialized = false;

    pthread_mutex_init(&rm->mutex, NULL);

    // 初始化现有代码
    if (routing_manager_init(rm) != 0) {
        free(rm->routes);
        free(rm);
        return NULL;
    }

    rm->initialized = true;
    return rm;
}

// 添加冲突检测函数
static int routing_manager_detect_conflicts(RoutingManager *manager, const struct AudioRoute *new_route, uint32_t *conflict_indices, uint32_t *num_conflicts)
{
    if (!manager || !new_route || !conflict_indices || !num_conflicts) return -1;

    *num_conflicts = 0;
    pthread_mutex_lock(&manager->mutex);

    for (uint32_t i = 0; i < manager->num_routes; i++) {
        struct AudioRoute *existing = manager->routes[i];
        if (!existing || !existing->active) continue;

        // 检测输入/输出设备冲突
        bool input_conflict = strcmp(existing->input_device, new_route->input_device) == 0;
        bool output_conflict = strcmp(existing->output_device, new_route->output_device) == 0;

        // 如果有设备冲突，记录索引
        if (input_conflict || output_conflict) {
            if (*num_conflicts < manager->max_routes) {
                conflict_indices[*num_conflicts] = i;
                (*num_conflicts)++;
            }
        }
    }

    pthread_mutex_unlock(&manager->mutex);
    return 0;
}

// 添加冲突解决函数
static int routing_manager_resolve_conflicts(RoutingManager *manager, struct AudioRoute *new_route)
{
    if (!manager || !new_route) return -1;

    uint32_t *conflict_indices = calloc(manager->max_routes, sizeof(uint32_t));
    uint32_t num_conflicts = 0;

    if (routing_manager_detect_conflicts(manager, new_route, conflict_indices, &num_conflicts) != 0 || num_conflicts == 0) {
        free(conflict_indices);
        return 0; // 无冲突
    }

    int result = 0;
    pthread_mutex_lock(&manager->mutex);

    switch (manager->conflict_policy) {
        case CONFLICT_RESOLUTION_REPLACE_LOWER:
            // 替换优先级较低的冲突路由
            for (uint32_t i = 0; i < num_conflicts; i++) {
                uint32_t idx = conflict_indices[i];
                struct AudioRoute *existing = manager->routes[idx];
                if (existing->priority < new_route->priority) {
                    // 移除低优先级路由
                    routing_manager_remove_route_internal(manager, idx);
                }
            }
            break;

        case CONFLICT_RESOLUTION_IGNORE_NEW:
            // 如果有任何冲突，忽略新路由
            result = -1;
            break;

        case CONFLICT_RESOLUTION_MERGE:
            // 合并冲突路由（简单实现：保留高优先级路由的设置）
            for (uint32_t i = 0; i < num_conflicts; i++) {
                uint32_t idx = conflict_indices[i];
                struct AudioRoute *existing = manager->routes[idx];
                if (existing->priority < new_route->priority) {
                    // 用新路由替换低优先级路由
                    routing_manager_remove_route_internal(manager, idx);
                } else {
                    // 保留高优先级现有路由
                    result = -1;
                }
            }
            break;

        case CONFLICT_RESOLUTION_ABORT:
            // 遇到冲突则中止
            result = -1;
            break;
    }

    pthread_mutex_unlock(&manager->mutex);
    free(conflict_indices);
    return result;
}

// 修改添加路由函数
int routing_manager_add_route(RoutingManager *manager, const char *name, const char *input_device, const char *output_device, 
                             AudioProcessingChain *processing_chain, RoutePriority priority)
{
    if (!manager || !name || !input_device || !output_device || !processing_chain) return -1;

    // 创建新路由
    struct AudioRoute *route = calloc(1, sizeof(struct AudioRoute));
    if (!route) return -1;

    route->name = strdup(name);
    route->input_device = strdup(input_device);
    route->output_device = strdup(output_device);
    route->processing_chain = processing_chain;
    route->active = true;
    route->priority = priority;
    route->route_id = manager->next_route_id++;

    // 检查冲突
    if (routing_manager_resolve_conflicts(manager, route) != 0) {
        // 冲突无法解决，清理新路由
        free(route->name);
        free(route->input_device);
        free(route->output_device);
        free(route);
        return -1;
    }

    pthread_mutex_lock(&manager->mutex);

    // 检查是否有空间
    if (manager->num_routes >= manager->max_routes) {
        pthread_mutex_unlock(&manager->mutex);
        free(route->name);
        free(route->input_device);
        free(route->output_device);
        free(route);
        return -1;
    }

    // 添加新路由
    manager->routes[manager->num_routes++] = route;

    pthread_mutex_unlock(&manager->mutex);

    // 通知回调
    if (manager->callback) {
        manager->callback(manager, ROUTE_EVENT_ADDED, route->route_id, manager->user_data);
    }

    return 0;
}

// 添加设置冲突策略的函数
void routing_manager_set_conflict_policy(RoutingManager *manager, ConflictResolutionPolicy policy)
{
    if (manager) {
        pthread_mutex_lock(&manager->mutex);
        manager->conflict_policy = policy;
        pthread_mutex_unlock(&manager->mutex);
    }
}

// 添加按优先级获取路由的函数
struct AudioRoute* routing_manager_get_highest_priority_route(RoutingManager *manager, const char *output_device)
{
    if (!manager || !output_device) return NULL;

    struct AudioRoute *highest_route = NULL;
    int highest_priority = -1;

    pthread_mutex_lock(&manager->mutex);

    for (uint32_t i = 0; i < manager->num_routes; i++) {
        struct AudioRoute *route = manager->routes[i];
        if (route && route->active && strcmp(route->output_device, output_device) == 0 &&
            route->priority > highest_priority) {
            highest_priority = route->priority;
            highest_route = route;
        }
    }

    pthread_mutex_unlock(&manager->mutex);
    return highest_route;
}

// ... existing code ...