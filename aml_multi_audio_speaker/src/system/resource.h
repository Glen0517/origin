/**
 * @file resource.h
 * @brief 资源管理模块头文件
 * @details 提供系统资源监控和自动回收功能
 * @author AML Audio Team
 * @date 2026-01-22
 */

#ifndef __RESOURCE_H__
#define __RESOURCE_H__

#include "common_def.h"

/******************************************************************************************
 * 资源管理数据类型
 ******************************************************************************************/

/**
 * @brief 资源类型
 */
typedef enum {
    RESOURCE_TYPE_CPU = 0,          // CPU资源
    RESOURCE_TYPE_MEMORY,           // 内存资源
    RESOURCE_TYPE_DISK,             // 磁盘资源
    RESOURCE_TYPE_NETWORK,          // 网络资源
    RESOURCE_TYPE_FILE_DESCRIPTOR,  // 文件描述符
    RESOURCE_TYPE_THREAD,           // 线程资源
    RESOURCE_TYPE_PROCESS,          // 进程资源
    RESOURCE_TYPE_MAX
} ResourceType_t;

/**
 * @brief 资源状态
 */
typedef enum {
    RESOURCE_STATUS_NORMAL = 0,     // 正常状态
    RESOURCE_STATUS_WARNING,         // 警告状态
    RESOURCE_STATUS_CRITICAL,        // 临界状态
    RESOURCE_STATUS_ERROR,           // 错误状态
    RESOURCE_STATUS_MAX
} ResourceStatus_t;

/**
 * @brief 资源使用情况
 */
typedef struct {
    ResourceType_t type;            // 资源类型
    ResourceStatus_t status;         // 资源状态
    uint64_t used;                  // 已使用量
    uint64_t total;                 // 总量
    uint64_t peak;                  // 峰值使用量
    uint32_t usage_percent;          // 使用百分比
    char unit[16];                  // 单位
} ResourceUsage_t;

/**
 * @brief 资源限制
 */
typedef struct {
    ResourceType_t type;            // 资源类型
    uint64_t soft_limit;            // 软限制
    uint64_t hard_limit;            // 硬限制
    uint32_t warning_threshold;     // 警告阈值（百分比）
    uint32_t critical_threshold;    // 临界阈值（百分比）
} ResourceLimit_t;

/******************************************************************************************
 * 资源管理对外接口
 ******************************************************************************************/

/**
 * @brief 初始化资源管理模块
 * @return SUCCESS/FAILURE
 */
int resource_init(void);

/**
 * @brief 反初始化资源管理模块
 * @return SUCCESS/FAILURE
 */
int resource_deinit(void);

/**
 * @brief 获取资源使用情况
 * @param type 资源类型
 * @param usage 资源使用情况
 * @return SUCCESS/FAILURE
 */
int resource_get_usage(ResourceType_t type, ResourceUsage_t *usage);

/**
 * @brief 设置资源限制
 * @param limit 资源限制
 * @return SUCCESS/FAILURE
 */
int resource_set_limit(const ResourceLimit_t *limit);

/**
 * @brief 获取资源限制
 * @param type 资源类型
 * @param limit 资源限制
 * @return SUCCESS/FAILURE
 */
int resource_get_limit(ResourceType_t type, ResourceLimit_t *limit);

/**
 * @brief 监控资源使用情况
 * @param interval 监控间隔（毫秒）
 * @return SUCCESS/FAILURE
 */
int resource_start_monitoring(int interval);

/**
 * @brief 停止资源监控
 * @return SUCCESS/FAILURE
 */
int resource_stop_monitoring(void);

/**
 * @brief 手动触发资源回收
 * @param type 资源类型
 * @return 回收的资源量
 */
uint64_t resource_reclaim(ResourceType_t type);

/**
 * @brief 获取资源使用统计
 * @param type 资源类型
 * @param stats 统计信息缓冲区
 * @param max_len 缓冲区最大长度
 * @return SUCCESS/FAILURE
 */
int resource_get_stats(ResourceType_t type, char *stats, int max_len);

/**
 * @brief 注册资源使用回调函数
 * @param type 资源类型
 * @param callback 回调函数
 * @param arg 回调参数
 * @return SUCCESS/FAILURE
 */
int resource_register_callback(ResourceType_t type, void (*callback)(ResourceType_t, ResourceStatus_t, void *), void *arg);

/**
 * @brief 取消注册资源使用回调函数
 * @param type 资源类型
 * @return SUCCESS/FAILURE
 */
int resource_unregister_callback(ResourceType_t type);

#endif /* __RESOURCE_H__ */