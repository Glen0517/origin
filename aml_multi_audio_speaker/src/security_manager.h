/**
 * @file security_manager.h
 * @brief 安全管理模块头文件
 * @details 提供安全检查、安全事件记录、固件更新等功能
 * @author AML Audio Team
 * @date 2026-01-28
 */

#ifndef __SECURITY_MANAGER_H__
#define __SECURITY_MANAGER_H__

#include "common_def.h"
#include "logger.h"

// 安全级别定义
typedef enum {
    SECURITY_LEVEL_LOW = 0,
    SECURITY_LEVEL_MEDIUM = 1,
    SECURITY_LEVEL_HIGH = 2,
    SECURITY_LEVEL_MAX
} SecurityLevel_e;

// 安全事件类型定义
typedef enum {
    SECURITY_EVENT_NONE = 0,
    SECURITY_EVENT_INPUT_VALIDATION = 1,
    SECURITY_EVENT_NETWORK_ACCESS = 2,
    SECURITY_EVENT_MEMORY_ACCESS = 3,
    SECURITY_EVENT_CONFIG_CHANGE = 4,
    SECURITY_EVENT_FIRMWARE_UPDATE = 5,
    SECURITY_EVENT_MAX
} SecurityEvent_e;

// 安全记录结构体
typedef struct SecurityRecord {
    uint32_t record_id;
    SecurityEvent_e event_type;
    SecurityLevel_e security_level;
    const char *event_msg;
    const char *module_name;
    const char *source_ip;
    uint16_t source_port;
    uint64_t timestamp;
    struct SecurityRecord *next;
} SecurityRecord_t;

// 安全管理器结构体
typedef struct {
    SecurityRecord_t *security_records;
    uint32_t record_id_counter;
    uint32_t total_security_events;
    uint32_t event_counts[SECURITY_EVENT_MAX];
    uint32_t level_counts[SECURITY_LEVEL_MAX];
    bool input_validation_enabled;
    bool network_security_enabled;
    bool security_update_enabled;
    char *firmware_version;
    char *last_security_update;
} SecurityManager_t;

/**
 * @brief 初始化安全管理器
 * @details 创建并初始化安全管理器，用于安全检查和更新
 * @return 安全管理器指针，失败返回NULL
 */
SecurityManager_t *security_manager_init(void);

/**
 * @brief 记录安全事件
 * @details 记录安全相关事件并更新统计信息
 * @param manager 安全管理器指针
 * @param event_type 事件类型
 * @param security_level 安全级别
 * @param event_msg 事件消息
 * @param module_name 模块名称
 * @param source_ip 源IP地址
 * @param source_port 源端口
 */
void security_manager_record_event(SecurityManager_t *manager, 
                                 SecurityEvent_e event_type, 
                                 SecurityLevel_e security_level, 
                                 const char *event_msg, 
                                 const char *module_name, 
                                 const char *source_ip, 
                                 uint16_t source_port);

/**
 * @brief 验证输入数据
 * @details 验证输入数据的合法性，防止缓冲区溢出等安全问题
 * @param manager 安全管理器指针
 * @param data 输入数据
 * @param size 数据大小
 * @param max_size 最大允许大小
 * @param module_name 模块名称
 * @return 验证是否通过
 */
bool security_manager_validate_input(SecurityManager_t *manager, 
                                  const void *data, 
                                  size_t size, 
                                  size_t max_size, 
                                  const char *module_name);

/**
 * @brief 检查网络访问
 * @details 检查网络访问的合法性，防止未授权访问
 * @param manager 安全管理器指针
 * @param ip_address IP地址
 * @param port 端口
 * @param module_name 模块名称
 * @return 访问是否允许
 */
bool security_manager_check_network_access(SecurityManager_t *manager, 
                                        const char *ip_address, 
                                        uint16_t port, 
                                        const char *module_name);

/**
 * @brief 检查固件更新
 * @details 检查是否有可用的固件更新
 * @param manager 安全管理器指针
 * @return 是否有更新可用
 */
bool security_manager_check_update(SecurityManager_t *manager);

/**
 * @brief 应用安全更新
 * @details 应用安全更新到系统
 * @param manager 安全管理器指针
 * @param update_url 更新URL
 * @return 更新是否成功
 */
bool security_manager_apply_update(SecurityManager_t *manager, const char *update_url);

/**
 * @brief 输出安全统计信息
 * @details 输出安全事件统计和最近的安全记录
 * @param manager 安全管理器指针
 */
void security_manager_print_stats(SecurityManager_t *manager);

/**
 * @brief 反初始化安全管理器
 * @details 反初始化安全管理器，释放资源
 * @param manager 安全管理器指针
 */
void security_manager_deinit(SecurityManager_t *manager);

#endif // __SECURITY_MANAGER_H__
