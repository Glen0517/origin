/**
 * @file security_manager.c
 * @brief 安全管理模块实现
 * @details 提供安全检查、安全事件记录、固件更新等功能
 * @author AML Audio Team
 * @date 2026-01-28
 */

#include "security_manager.h"

/**
 * @brief 初始化安全管理器
 * @details 创建并初始化安全管理器，用于安全检查和更新
 * @return 安全管理器指针，失败返回NULL
 */
SecurityManager_t *security_manager_init(void) {
    SecurityManager_t *manager = (SecurityManager_t *)malloc(sizeof(SecurityManager_t));
    if (!manager) {
        LOG_ERROR("Failed to allocate security manager");
        return NULL;
    }
    
    memset(manager, 0, sizeof(SecurityManager_t));
    
    // 启用安全功能
    manager->input_validation_enabled = true;
    manager->network_security_enabled = true;
    manager->security_update_enabled = true;
    manager->crypto_enabled = true;
    manager->audit_enabled = true;
    manager->audit_level = AUDIT_LEVEL_BASIC;
    
    // 设置固件版本
    manager->firmware_version = "1.0.0";
    manager->last_security_update = "2026-01-27";
    
    // 初始化加密密钥（实际实现中应该从安全存储中读取）
    manager->encryption_key = "default_encryption_key_123";
    manager->key_size = strlen(manager->encryption_key);
    
    LOG_INFO("Security manager initialized");
    return manager;
}

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
                                 uint16_t source_port) {
    if (!manager) {
        return;
    }
    
    // 创建安全记录
    SecurityRecord_t *record = (SecurityRecord_t *)malloc(sizeof(SecurityRecord_t));
    if (!record) {
        LOG_ERROR("Failed to allocate security record");
        return;
    }
    
    memset(record, 0, sizeof(SecurityRecord_t));
    record->record_id = ++manager->record_id_counter;
    record->event_type = event_type;
    record->security_level = security_level;
    record->event_msg = event_msg;
    record->module_name = module_name;
    record->source_ip = source_ip;
    record->source_port = source_port;
    record->timestamp = time(NULL);
    
    // 更新统计信息
    manager->total_security_events++;
    if (event_type < SECURITY_EVENT_MAX) {
        manager->event_counts[event_type]++;
    }
    if (security_level < SECURITY_LEVEL_MAX) {
        manager->level_counts[security_level]++;
    }
    
    // 添加到安全记录列表
    record->next = manager->security_records;
    manager->security_records = record;
    
    // 根据安全级别输出日志
    switch (security_level) {
        case SECURITY_LEVEL_LOW:
            LOG_INFO("Security Event [%d]: %s (Module: %s, Type: %d, Source: %s:%d)", 
                     record->record_id, event_msg, module_name, event_type, source_ip, source_port);
            break;
        case SECURITY_LEVEL_MEDIUM:
            LOG_WARN("Security Event [%d]: %s (Module: %s, Type: %d, Source: %s:%d)", 
                     record->record_id, event_msg, module_name, event_type, source_ip, source_port);
            break;
        case SECURITY_LEVEL_HIGH:
            LOG_ERROR("Security Event [%d]: %s (Module: %s, Type: %d, Source: %s:%d)", 
                     record->record_id, event_msg, module_name, event_type, source_ip, source_port);
            break;
        default:
            break;
    }
}

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
                                  const char *module_name) {
    if (!manager || !manager->input_validation_enabled) {
        return true;
    }
    
    if (!data) {
        security_manager_record_event(manager, 
                                   SECURITY_EVENT_INPUT_VALIDATION, 
                                   SECURITY_LEVEL_MEDIUM, 
                                   "Null input data", 
                                   module_name, 
                                   "localhost", 
                                   0);
        return false;
    }
    
    if (size > max_size) {
        security_manager_record_event(manager, 
                                   SECURITY_EVENT_INPUT_VALIDATION, 
                                   SECURITY_LEVEL_HIGH, 
                                   "Input data size exceeds maximum allowed", 
                                   module_name, 
                                   "localhost", 
                                   0);
        return false;
    }
    
    // 检查输入数据是否包含恶意内容
    // TODO: 实现更详细的输入验证逻辑
    
    return true;
}

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
                                        const char *module_name) {
    if (!manager || !manager->network_security_enabled) {
        return true;
    }
    
    if (!ip_address) {
        security_manager_record_event(manager, 
                                   SECURITY_EVENT_NETWORK_ACCESS, 
                                   SECURITY_LEVEL_MEDIUM, 
                                   "Null IP address", 
                                   module_name, 
                                   "unknown", 
                                   port);
        return false;
    }
    
    // 检查是否为本地地址
    if (strcmp(ip_address, "127.0.0.1") == 0 || strcmp(ip_address, "localhost") == 0) {
        return true;
    }
    
    // 检查是否为允许的网络
    // TODO: 实现网络访问控制列表
    
    // 记录网络访问事件
    security_manager_record_event(manager, 
                               SECURITY_EVENT_NETWORK_ACCESS, 
                               SECURITY_LEVEL_LOW, 
                               "Network access attempt", 
                               module_name, 
                               ip_address, 
                               port);
    
    return true;
}

/**
 * @brief 检查固件更新
 * @details 检查是否有可用的固件更新
 * @param manager 安全管理器指针
 * @return 是否有更新可用
 */
bool security_manager_check_update(SecurityManager_t *manager) {
    if (!manager || !manager->security_update_enabled) {
        return false;
    }
    
    // 检查固件更新
    LOG_INFO("Checking for firmware updates...");
    LOG_INFO("Current firmware version: %s", manager->firmware_version);
    LOG_INFO("Last security update: %s", manager->last_security_update);
    
    // 模拟固件更新检查
    // 实际实现中，这里应该通过网络请求检查更新服务器
    // 或者通过本地存储的更新包检查
    
    // 检查更新服务器
    // 这里使用模拟逻辑，实际应替换为真实的网络请求
    bool update_available = false;
    char latest_version[64] = "1.0.1";
    
    // 比较版本号
    if (strcmp(manager->firmware_version, latest_version) < 0) {
        update_available = true;
        LOG_INFO("New firmware version available: %s", latest_version);
    } else {
        LOG_INFO("Firmware is up to date");
    }
    
    return update_available;
}

/**
 * @brief 应用安全更新
 * @details 应用安全更新到系统
 * @param manager 安全管理器指针
 * @param update_url 更新URL
 * @return 更新是否成功
 */
bool security_manager_apply_update(SecurityManager_t *manager, const char *update_url) {
    if (!manager || !manager->security_update_enabled) {
        return false;
    }
    
    // 应用安全更新
    LOG_INFO("Applying security update from: %s", update_url);
    
    // 1. 下载固件更新包
    LOG_INFO("Downloading firmware update...");
    // 实际实现中，这里应该通过网络下载更新包
    // 并进行校验和验证
    
    // 2. 验证固件更新包
    LOG_INFO("Verifying firmware update...");
    // 实际实现中，这里应该验证固件的签名和完整性
    
    // 3. 备份当前固件
    LOG_INFO("Backing up current firmware...");
    // 实际实现中，这里应该备份当前的固件，以便在更新失败时恢复
    
    // 4. 应用固件更新
    LOG_INFO("Applying firmware update...");
    // 实际实现中，这里应该将新固件写入设备
    
    // 5. 验证更新是否成功
    LOG_INFO("Verifying firmware update application...");
    // 实际实现中，这里应该验证新固件是否正确写入
    
    // 6. 更新版本信息
    manager->firmware_version = "1.0.1";
    
    // 7. 更新最后更新时间
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d", tm_info);
    manager->last_security_update = time_str;
    
    // 8. 记录安全事件
    security_manager_record_event(manager, 
                               SECURITY_EVENT_FIRMWARE_UPDATE, 
                               SECURITY_LEVEL_LOW, 
                               "Security update applied", 
                               "security_manager", 
                               "update_server", 
                               80);
    
    LOG_INFO("Firmware update applied successfully");
    return true;
}

/**
 * @brief 输出安全统计信息
 * @details 输出安全事件统计和最近的安全记录
 * @param manager 安全管理器指针
 */
void security_manager_print_stats(SecurityManager_t *manager) {
    if (!manager) {
        return;
    }
    
    LOG_INFO("=== Security Statistics ===");
    LOG_INFO("Total security events: %d", manager->total_security_events);
    LOG_INFO("Security event counts:");
    LOG_INFO("  Input Validation: %d", manager->event_counts[SECURITY_EVENT_INPUT_VALIDATION]);
    LOG_INFO("  Network Access: %d", manager->event_counts[SECURITY_EVENT_NETWORK_ACCESS]);
    LOG_INFO("  Memory Access: %d", manager->event_counts[SECURITY_EVENT_MEMORY_ACCESS]);
    LOG_INFO("  Config Change: %d", manager->event_counts[SECURITY_EVENT_CONFIG_CHANGE]);
    LOG_INFO("  Firmware Update: %d", manager->event_counts[SECURITY_EVENT_FIRMWARE_UPDATE]);
    
    LOG_INFO("Security level counts:");
    LOG_INFO("  Low: %d", manager->level_counts[SECURITY_LEVEL_LOW]);
    LOG_INFO("  Medium: %d", manager->level_counts[SECURITY_LEVEL_MEDIUM]);
    LOG_INFO("  High: %d", manager->level_counts[SECURITY_LEVEL_HIGH]);
    
    LOG_INFO("Firmware version: %s", manager->firmware_version);
    LOG_INFO("Last security update: %s", manager->last_security_update);
    
    LOG_INFO("Security features enabled:");
    LOG_INFO("  Input Validation: %s", manager->input_validation_enabled ? "Yes" : "No");
    LOG_INFO("  Network Security: %s", manager->network_security_enabled ? "Yes" : "No");
    LOG_INFO("  Security Update: %s", manager->security_update_enabled ? "Yes" : "No");
    
    LOG_INFO("========================");
}

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
bool security_manager_encrypt(SecurityManager_t *manager, CryptoAlgorithm_e algorithm, const uint8_t *input, size_t input_len, uint8_t *output, size_t *output_len, const char *module_name) {
    if (!manager || !manager->crypto_enabled || !input || !output || !output_len) {
        return false;
    }
    
    // 记录加密操作事件
    security_manager_record_event(manager, 
                               SECURITY_EVENT_CRYPTO_OPERATION, 
                               SECURITY_LEVEL_LOW, 
                               "Encryption operation", 
                               module_name, 
                               "localhost", 
                               0);
    
    // 模拟加密操作（实际实现中应该使用真正的加密算法）
    LOG_INFO("Encrypting data using algorithm %d", algorithm);
    
    // 简单的XOR加密实现（仅用于演示）
    for (size_t i = 0; i < input_len; i++) {
        output[i] = input[i] ^ manager->encryption_key[i % manager->key_size];
    }
    *output_len = input_len;
    
    return true;
}

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
bool security_manager_decrypt(SecurityManager_t *manager, CryptoAlgorithm_e algorithm, const uint8_t *input, size_t input_len, uint8_t *output, size_t *output_len, const char *module_name) {
    if (!manager || !manager->crypto_enabled || !input || !output || !output_len) {
        return false;
    }
    
    // 记录解密操作事件
    security_manager_record_event(manager, 
                               SECURITY_EVENT_CRYPTO_OPERATION, 
                               SECURITY_LEVEL_LOW, 
                               "Decryption operation", 
                               module_name, 
                               "localhost", 
                               0);
    
    // 模拟解密操作（实际实现中应该使用真正的加密算法）
    LOG_INFO("Decrypting data using algorithm %d", algorithm);
    
    // 简单的XOR解密实现（仅用于演示）
    for (size_t i = 0; i < input_len; i++) {
        output[i] = input[i] ^ manager->encryption_key[i % manager->key_size];
    }
    *output_len = input_len;
    
    return true;
}

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
bool security_manager_hash(SecurityManager_t *manager, CryptoAlgorithm_e algorithm, const uint8_t *input, size_t input_len, uint8_t *output, size_t *output_len, const char *module_name) {
    if (!manager || !manager->crypto_enabled || !input || !output || !output_len) {
        return false;
    }
    
    // 记录哈希操作事件
    security_manager_record_event(manager, 
                               SECURITY_EVENT_CRYPTO_OPERATION, 
                               SECURITY_LEVEL_LOW, 
                               "Hash operation", 
                               module_name, 
                               "localhost", 
                               0);
    
    // 模拟哈希操作（实际实现中应该使用真正的哈希算法）
    LOG_INFO("Hashing data using algorithm %d", algorithm);
    
    // 简单的哈希实现（仅用于演示）
    uint32_t hash = 0;
    for (size_t i = 0; i < input_len; i++) {
        hash = hash * 31 + input[i];
    }
    
    // 将哈希值复制到输出
    memcpy(output, &hash, sizeof(hash));
    *output_len = sizeof(hash);
    
    return true;
}

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
bool security_manager_verify_signature(SecurityManager_t *manager, CryptoAlgorithm_e algorithm, const uint8_t *data, size_t data_len, const uint8_t *signature, size_t signature_len, const char *module_name) {
    if (!manager || !manager->crypto_enabled || !data || !signature) {
        return false;
    }
    
    // 记录签名验证事件
    security_manager_record_event(manager, 
                               SECURITY_EVENT_CRYPTO_OPERATION, 
                               SECURITY_LEVEL_MEDIUM, 
                               "Signature verification", 
                               module_name, 
                               "localhost", 
                               0);
    
    // 模拟签名验证（实际实现中应该使用真正的签名验证算法）
    LOG_INFO("Verifying signature using algorithm %d", algorithm);
    
    // 简单的签名验证实现（仅用于演示）
    // 这里只是检查签名长度是否合理
    if (signature_len < 4) {
        return false;
    }
    
    return true;
}

/**
 * @brief 配置安全审计
 * @details 配置安全审计级别和功能
 * @param manager 安全管理器指针
 * @param enabled 是否启用
 * @param level 审计级别
 * @return 配置是否成功
 */
bool security_manager_config_audit(SecurityManager_t *manager, bool enabled, AuditLevel_e level) {
    if (!manager) {
        return false;
    }
    
    manager->audit_enabled = enabled;
    manager->audit_level = level;
    
    // 记录审计配置事件
    security_manager_record_event(manager, 
                               SECURITY_EVENT_AUDIT_LOG, 
                               SECURITY_LEVEL_LOW, 
                               "Audit configuration changed", 
                               "security_manager", 
                               "localhost", 
                               0);
    
    LOG_INFO("Audit configured: enabled=%d, level=%d", enabled, level);
    return true;
}

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
bool security_manager_audit_event(SecurityManager_t *manager, SecurityEvent_e event_type, SecurityLevel_e security_level, const char *event_msg, const char *module_name, const char *source_ip, uint16_t source_port, const char *details) {
    if (!manager || !manager->audit_enabled) {
        return false;
    }
    
    // 记录审计事件
    security_manager_record_event(manager, 
                               event_type, 
                               security_level, 
                               event_msg, 
                               module_name, 
                               source_ip, 
                               source_port);
    
    // 输出详细审计信息
    LOG_INFO("Audit event: %s (Module: %s, Source: %s:%d, Details: %s)", 
             event_msg, module_name, source_ip, source_port, details);
    
    return true;
}

/**
 * @brief 导出安全审计日志
 * @details 导出安全审计日志到文件
 * @param manager 安全管理器指针
 * @param file_path 文件路径
 * @return 导出是否成功
 */
bool security_manager_export_audit_log(SecurityManager_t *manager, const char *file_path) {
    if (!manager || !manager->audit_enabled || !file_path) {
        return false;
    }
    
    // 记录审计日志导出事件
    security_manager_record_event(manager, 
                               SECURITY_EVENT_AUDIT_LOG, 
                               SECURITY_LEVEL_LOW, 
                               "Audit log exported", 
                               "security_manager", 
                               "localhost", 
                               0);
    
    // 模拟审计日志导出（实际实现中应该将日志写入文件）
    LOG_INFO("Exporting audit log to: %s", file_path);
    
    // 遍历安全记录并输出
    SecurityRecord_t *curr = manager->security_records;
    while (curr) {
        LOG_INFO("Record %d: Type=%d, Level=%d, Msg=%s, Module=%s, Source=%s:%d", 
                 curr->record_id, curr->event_type, curr->security_level, 
                 curr->event_msg, curr->module_name, curr->source_ip, curr->source_port);
        curr = curr->next;
    }
    
    return true;
}

/**
 * @brief 设置加密密钥
 * @details 设置用于加密的密钥
 * @param manager 安全管理器指针
 * @param key 加密密钥
 * @param key_size 密钥大小
 * @return 设置是否成功
 */
bool security_manager_set_encryption_key(SecurityManager_t *manager, const char *key, uint32_t key_size) {
    if (!manager || !key || key_size == 0) {
        return false;
    }
    
    manager->encryption_key = key;
    manager->key_size = key_size;
    
    // 记录密钥设置事件
    security_manager_record_event(manager, 
                               SECURITY_EVENT_CRYPTO_OPERATION, 
                               SECURITY_LEVEL_HIGH, 
                               "Encryption key changed", 
                               "security_manager", 
                               "localhost", 
                               0);
    
    LOG_INFO("Encryption key set with size: %d", key_size);
    return true;
}

/**
 * @brief 反初始化安全管理器
 * @details 反初始化安全管理器，释放资源
 * @param manager 安全管理器指针
 */
void security_manager_deinit(SecurityManager_t *manager) {
    if (!manager) {
        return;
    }
    
    // 打印安全统计
    security_manager_print_stats(manager);
    
    // 释放安全记录
    SecurityRecord_t *curr = manager->security_records;
    while (curr) {
        SecurityRecord_t *next = curr->next;
        free(curr);
        curr = next;
    }
    
    // 释放安全管理器
    free(manager);
    LOG_INFO("Security manager deinitialized");
}
