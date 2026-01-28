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
    SECURITY_LEVEL_CRITICAL = 3,
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
    SECURITY_EVENT_CRYPTO_OPERATION = 6,
    SECURITY_EVENT_AUDIT_LOG = 7,
    SECURITY_EVENT_ACCESS_CONTROL = 8,
    SECURITY_EVENT_MAX
} SecurityEvent_e;

// 加密算法类型
typedef enum {
    CRYPTO_ALG_AES_128 = 0,
    CRYPTO_ALG_AES_256 = 1,
    CRYPTO_ALG_RSA_2048 = 2,
    CRYPTO_ALG_RSA_4096 = 3,
    CRYPTO_ALG_SHA_256 = 4,
    CRYPTO_ALG_SHA_512 = 5,
    CRYPTO_ALG_MAX
} CryptoAlgorithm_e;

// 加密操作类型
typedef enum {
    CRYPTO_OP_ENCRYPT = 0,
    CRYPTO_OP_DECRYPT = 1,
    CRYPTO_OP_SIGN = 2,
    CRYPTO_OP_VERIFY = 3,
    CRYPTO_OP_HASH = 4,
    CRYPTO_OP_MAX
} CryptoOperation_e;

// 安全审计级别
typedef enum {
    AUDIT_LEVEL_NONE = 0,
    AUDIT_LEVEL_BASIC = 1,
    AUDIT_LEVEL_DETAILED = 2,
    AUDIT_LEVEL_FULL = 3
} AuditLevel_e;

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
    bool crypto_enabled;
    bool audit_enabled;
    AuditLevel_e audit_level;
    char *firmware_version;
    char *last_security_update;
    char *encryption_key;
    uint32_t key_size;
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
 * @brief 加密数据
 * @details 使用指定算法加密数据
 * @param manager 安全管理器指针
 * @param algorithm 加密算法
 * @param input 输入数据
 * @param input_len 输入数据长度
 * @param output 输出数据
 * @param output_len 输出数据长度
 * @param module_name 模块名称
 * @return 加密是否成功
 */
bool security_manager_encrypt(SecurityManager_t *manager, CryptoAlgorithm_e algorithm, const uint8_t *input, size_t input_len, uint8_t *output, size_t *output_len, const char *module_name);

/**
 * @brief 解密数据
 * @details 使用指定算法解密数据
 * @param manager 安全管理器指针
 * @param algorithm 加密算法
 * @param input 输入数据
 * @param input_len 输入数据长度
 * @param output 输出数据
 * @param output_len 输出数据长度
 * @param module_name 模块名称
 * @return 解密是否成功
 */
bool security_manager_decrypt(SecurityManager_t *manager, CryptoAlgorithm_e algorithm, const uint8_t *input, size_t input_len, uint8_t *output, size_t *output_len, const char *module_name);

/**
 * @brief 计算数据哈希
 * @details 使用指定算法计算数据哈希值
 * @param manager 安全管理器指针
 * @param algorithm 哈希算法
 * @param input 输入数据
 * @param input_len 输入数据长度
 * @param output 输出哈希值
 * @param output_len 输出哈希值长度
 * @param module_name 模块名称
 * @return 计算是否成功
 */
bool security_manager_hash(SecurityManager_t *manager, CryptoAlgorithm_e algorithm, const uint8_t *input, size_t input_len, uint8_t *output, size_t *output_len, const char *module_name);

/**
 * @brief 验证数据签名
 * @details 验证数据签名的有效性
 * @param manager 安全管理器指针
 * @param algorithm 签名算法
 * @param data 原始数据
 * @param data_len 原始数据长度
 * @param signature 签名数据
 * @param signature_len 签名数据长度
 * @param module_name 模块名称
 * @return 验证是否成功
 */
bool security_manager_verify_signature(SecurityManager_t *manager, CryptoAlgorithm_e algorithm, const uint8_t *data, size_t data_len, const uint8_t *signature, size_t signature_len, const char *module_name);

/**
 * @brief 配置安全审计
 * @details 配置安全审计级别和功能
 * @param manager 安全管理器指针
 * @param enabled 是否启用
 * @param level 审计级别
 * @return 配置是否成功
 */
bool security_manager_config_audit(SecurityManager_t *manager, bool enabled, AuditLevel_e level);

/**
 * @brief 记录安全审计事件
 * @details 记录详细的安全审计事件
 * @param manager 安全管理器指针
 * @param event_type 事件类型
 * @param security_level 安全级别
 * @param event_msg 事件消息
 * @param module_name 模块名称
 * @param source_ip 源IP地址
 * @param source_port 源端口
 * @param details 详细信息
 * @return 记录是否成功
 */
bool security_manager_audit_event(SecurityManager_t *manager, SecurityEvent_e event_type, SecurityLevel_e security_level, const char *event_msg, const char *module_name, const char *source_ip, uint16_t source_port, const char *details);

/**
 * @brief 导出安全审计日志
 * @details 导出安全审计日志到文件
 * @param manager 安全管理器指针
 * @param file_path 文件路径
 * @return 导出是否成功
 */
bool security_manager_export_audit_log(SecurityManager_t *manager, const char *file_path);

/**
 * @brief 设置加密密钥
 * @details 设置用于加密的密钥
 * @param manager 安全管理器指针
 * @param key 加密密钥
 * @param key_size 密钥大小
 * @return 设置是否成功
 */
bool security_manager_set_encryption_key(SecurityManager_t *manager, const char *key, uint32_t key_size);

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
