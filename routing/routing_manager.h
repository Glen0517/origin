#ifndef ROUTING_MANAGER_H
#define ROUTING_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <linux/list.h>
#include "audio_types.h"

/**
 * 音频流路由类型
 */
typedef enum {
    ROUTE_TYPE_DIRECT,       // 直接路由
    ROUTE_TYPE_MIXED,        // 混合路由
    ROUTE_TYPE_PROCESSED     // 处理后路由
} RouteType;

/**
 * 音频端点类型
 */
typedef enum {
    ENDPOINT_TYPE_SOURCE,    // 源端点(输入)
    ENDPOINT_TYPE_SINK       // 宿端点(输出)
} EndpointType;

/**
 * 音频端点ID
 */
typedef struct {
    uint32_t id;             // 端点ID
    EndpointType type;       // 端点类型
    const char* name;        // 端点名称
    const char* device;      // 关联设备
} AudioEndpoint;

/**
 * 音频路由规则
 */
typedef struct {
    uint32_t route_id;       // 路由ID
    AudioEndpoint source;    // 源端点
    AudioEndpoint sink;      // 宿端点
    RouteType type;          // 路由类型
    uint32_t priority;       // 路由优先级(0-255)
    bool enabled;            // 是否启用
    void* processing_chain;  // 关联的处理链(可为NULL)
    struct list_head list;   // 链表节点
} RouteRule;

/**
 * 路由管理器配置
 */
typedef struct {
    bool enable_auto_routing;       // 是否启用自动路由
    uint32_t default_priority;      // 默认优先级
    uint32_t max_routes;            // 最大路由数
    uint32_t processing_buffer_size;// 处理缓冲区大小(帧)
} RoutingConfig;

/**
 * 路由管理器
 */
typedef struct {
    RoutingConfig config;           // 配置
    struct list_head routes;        // 路由规则列表
    pthread_mutex_t lock;           // 互斥锁
    bool initialized;               // 是否初始化
    void (*route_changed_cb)(RouteRule* rule, bool added); // 路由变更回调
} RoutingManager;

/**
 * 初始化路由管理器
 * 
 * @param config 配置参数
 * @return 成功返回0，失败返回负数错误码
 */
int routing_manager_init(const RoutingConfig* config);

/**
 * 销毁路由管理器
 */
void routing_manager_destroy(void);

/**
 * 获取路由管理器实例
 * 
 * @return 路由管理器指针
 */
RoutingManager* routing_manager_get_instance(void);

/**
 * 添加路由规则
 * 
 * @param rule 路由规则
 * @return 成功返回路由ID，失败返回负数错误码
 */
int routing_manager_add_route(const RouteRule* rule);

/**
 * 删除路由规则
 * 
 * @param route_id 路由ID
 * @return 成功返回0，失败返回负数错误码
 */
int routing_manager_remove_route(uint32_t route_id);

/**
 * 更新路由规则
 * 
 * @param rule 新的路由规则
 * @return 成功返回0，失败返回负数错误码
 */
int routing_manager_update_route(const RouteRule* rule);

/**
 * 获取所有路由规则
 * 
 * @param buffer 存储路由规则的缓冲区
 * @param max_count 缓冲区最大容量
 * @return 实际路由规则数量
 */
int routing_manager_get_routes(RouteRule* buffer, uint32_t max_count);

/**
 * 查找路由规则
 * 
 * @param source_id 源端点ID
 * @param sink_id 宿端点ID
 * @return 找到的路由规则，未找到返回NULL
 */
RouteRule* routing_manager_find_route(uint32_t source_id, uint32_t sink_id);

/**
 * 路由音频缓冲区
 * 
 * @param source 源端点
 * @param buffer 音频缓冲区
 * @param frames 帧数
 * @return 成功返回0，失败返回负数错误码
 */
int routing_manager_route_buffer(AudioEndpoint source, void* buffer, uint32_t frames);

/**
 * 设置路由变更回调函数
 * 
 * @param callback 回调函数
 */
void routing_manager_set_callback(void (*callback)(RouteRule* rule, bool added));

#endif // ROUTING_MANAGER_H