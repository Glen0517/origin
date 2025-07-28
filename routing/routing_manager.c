#include "routing_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <linux/slab.h>
#include "audio_processing.h"

// 全局路由管理器实例
static RoutingManager g_routing_manager;

// 静态函数声明
static int validate_route_rule(const RouteRule* rule);
static RouteRule* create_route_rule(const RouteRule* rule);
static void destroy_route_rule(RouteRule* rule);

int routing_manager_init(const RoutingConfig* config)
{
    if (!config) return -EINVAL;
    if (g_routing_manager.initialized) return -EBUSY;

    // 初始化路由管理器
    memset(&g_routing_manager, 0, sizeof(RoutingManager));
    g_routing_manager.config = *config;
    INIT_LIST_HEAD(&g_routing_manager.routes);
    pthread_mutex_init(&g_routing_manager.lock, NULL);
    g_routing_manager.initialized = true;

    return 0;
}

void routing_manager_destroy(void)
{
    if (!g_routing_manager.initialized) return;

    pthread_mutex_lock(&g_routing_manager.lock);

    // 删除所有路由规则
    RouteRule *rule, *temp;
    list_for_each_entry_safe(rule, temp, &g_routing_manager.routes, list) {
        list_del(&rule->list);
        destroy_route_rule(rule);
    }

    pthread_mutex_unlock(&g_routing_manager.lock);
    pthread_mutex_destroy(&g_routing_manager.lock);
    memset(&g_routing_manager, 0, sizeof(RoutingManager));
}

RoutingManager* routing_manager_get_instance(void)
{
    if (!g_routing_manager.initialized) return NULL;
    return &g_routing_manager;
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