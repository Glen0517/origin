/**
 * @file test_security_manager.c
 * @brief 安全管理模块单元测试
 * @details 测试加密、解密、哈希计算、签名验证和安全审计功能
 * @author AML Audio Team
 * @date 2026-01-28
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/security_manager.h"

/**
 * @brief 测试安全管理器初始化
 * @return 测试是否通过
 */
bool test_security_manager_init(void) {
    printf("=== 测试安全管理器初始化 ===\n");
    
    // 初始化安全管理器
    SecurityManager_t *manager = security_manager_init();
    if (!manager) {
        printf("❌ 安全管理器初始化失败\n");
        return false;
    }
    
    printf("✅ 安全管理器初始化成功\n");
    printf("  固件版本: %s\n", manager->firmware_version);
    printf("  最后安全更新: %s\n", manager->last_security_update);
    printf("  输入验证启用: %s\n", manager->input_validation_enabled ? "是" : "否");
    printf("  网络安全启用: %s\n", manager->network_security_enabled ? "是" : "否");
    printf("  安全更新启用: %s\n", manager->security_update_enabled ? "是" : "否");
    printf("  加密启用: %s\n", manager->crypto_enabled ? "是" : "否");
    printf("  审计启用: %s\n", manager->audit_enabled ? "是" : "否");
    
    // 释放安全管理器
    security_manager_deinit(manager);
    printf("✅ 安全管理器释放成功\n");
    
    return true;
}

/**
 * @brief 测试输入验证
 * @return 测试是否通过
 */
bool test_input_validation(void) {
    printf("\n=== 测试输入验证 ===\n");
    
    // 初始化安全管理器
    SecurityManager_t *manager = security_manager_init();
    if (!manager) {
        printf("❌ 安全管理器初始化失败\n");
        return false;
    }
    
    // 测试空输入
    bool result = security_manager_validate_input(manager, NULL, 0, 100, "test_module");
    printf("✅ 空输入验证: %s\n", result ? "通过" : "失败");
    
    // 测试正常输入
    char test_data[] = "Valid input data";
    result = security_manager_validate_input(manager, test_data, strlen(test_data), 100, "test_module");
    printf("✅ 正常输入验证: %s\n", result ? "通过" : "失败");
    
    // 测试输入超限
    char long_data[200];
    memset(long_data, 'a', sizeof(long_data)-1);
    long_data[sizeof(long_data)-1] = '\0';
    result = security_manager_validate_input(manager, long_data, strlen(long_data), 100, "test_module");
    printf("✅ 输入超限验证: %s\n", result ? "通过" : "失败");
    
    // 释放安全管理器
    security_manager_deinit(manager);
    printf("✅ 安全管理器释放成功\n");
    
    return true;
}

/**
 * @brief 测试网络访问检查
 * @return 测试是否通过
 */
bool test_network_access(void) {
    printf("\n=== 测试网络访问检查 ===\n");
    
    // 初始化安全管理器
    SecurityManager_t *manager = security_manager_init();
    if (!manager) {
        printf("❌ 安全管理器初始化失败\n");
        return false;
    }
    
    // 测试本地地址
    bool result = security_manager_check_network_access(manager, "127.0.0.1", 8080, "test_module");
    printf("✅ 本地地址访问: %s\n", result ? "允许" : "拒绝");
    
    // 测试localhost
    result = security_manager_check_network_access(manager, "localhost", 8080, "test_module");
    printf("✅ localhost访问: %s\n", result ? "允许" : "拒绝");
    
    // 测试外部地址
    result = security_manager_check_network_access(manager, "192.168.1.100", 8080, "test_module");
    printf("✅ 外部地址访问: %s\n", result ? "允许" : "拒绝");
    
    // 释放安全管理器
    security_manager_deinit(manager);
    printf("✅ 安全管理器释放成功\n");
    
    return true;
}

/**
 * @brief 测试加密和解密功能
 * @return 测试是否通过
 */
bool test_crypto_operations(void) {
    printf("\n=== 测试加密和解密功能 ===\n");
    
    // 初始化安全管理器
    SecurityManager_t *manager = security_manager_init();
    if (!manager) {
        printf("❌ 安全管理器初始化失败\n");
        return false;
    }
    
    // 测试数据
    const char *test_data = "Test encryption data";
    size_t data_len = strlen(test_data);
    
    // 测试加密
    uint8_t encrypted[100];
    size_t encrypted_len = sizeof(encrypted);
    bool result = security_manager_encrypt(manager, CRYPTO_ALG_AES_128, (const uint8_t *)test_data, data_len, encrypted, &encrypted_len, "test_module");
    if (!result) {
        printf("❌ 加密失败\n");
        security_manager_deinit(manager);
        return false;
    }
    printf("✅ 加密成功，加密后长度: %zu\n", encrypted_len);
    
    // 测试解密
    uint8_t decrypted[100];
    size_t decrypted_len = sizeof(decrypted);
    result = security_manager_decrypt(manager, CRYPTO_ALG_AES_128, encrypted, encrypted_len, decrypted, &decrypted_len, "test_module");
    if (!result) {
        printf("❌ 解密失败\n");
        security_manager_deinit(manager);
        return false;
    }
    printf("✅ 解密成功，解密后长度: %zu\n", decrypted_len);
    
    // 验证解密结果
    decrypted[decrypted_len] = '\0';
    if (strcmp((char *)decrypted, test_data) == 0) {
        printf("✅ 解密结果正确: %s\n", (char *)decrypted);
    } else {
        printf("❌ 解密结果错误\n");
        security_manager_deinit(manager);
        return false;
    }
    
    // 测试哈希计算
    uint8_t hash[32];
    size_t hash_len = sizeof(hash);
    result = security_manager_hash(manager, CRYPTO_ALG_SHA_256, (const uint8_t *)test_data, data_len, hash, &hash_len, "test_module");
    if (!result) {
        printf("❌ 哈希计算失败\n");
        security_manager_deinit(manager);
        return false;
    }
    printf("✅ 哈希计算成功，哈希长度: %zu\n", hash_len);
    
    // 测试签名验证
    uint8_t signature[32] = {0x01, 0x02, 0x03, 0x04};
    result = security_manager_verify_signature(manager, CRYPTO_ALG_RSA_2048, (const uint8_t *)test_data, data_len, signature, sizeof(signature), "test_module");
    printf("✅ 签名验证: %s\n", result ? "通过" : "失败");
    
    // 释放安全管理器
    security_manager_deinit(manager);
    printf("✅ 安全管理器释放成功\n");
    
    return true;
}

/**
 * @brief 测试安全审计
 * @return 测试是否通过
 */
bool test_security_audit(void) {
    printf("\n=== 测试安全审计 ===\n");
    
    // 初始化安全管理器
    SecurityManager_t *manager = security_manager_init();
    if (!manager) {
        printf("❌ 安全管理器初始化失败\n");
        return false;
    }
    
    // 配置审计
    bool result = security_manager_config_audit(manager, true, AUDIT_LEVEL_DETAILED);
    if (!result) {
        printf("❌ 审计配置失败\n");
        security_manager_deinit(manager);
        return false;
    }
    printf("✅ 审计配置成功\n");
    
    // 记录审计事件
    result = security_manager_audit_event(manager, SECURITY_EVENT_INPUT_VALIDATION, SECURITY_LEVEL_MEDIUM, "Test audit event", "test_module", "192.168.1.100", 8080, "Detailed audit information");
    if (!result) {
        printf("❌ 审计事件记录失败\n");
        security_manager_deinit(manager);
        return false;
    }
    printf("✅ 审计事件记录成功\n");
    
    // 导出审计日志
    result = security_manager_export_audit_log(manager, "/tmp/audit_log.txt");
    if (!result) {
        printf("❌ 审计日志导出失败\n");
        security_manager_deinit(manager);
        return false;
    }
    printf("✅ 审计日志导出成功\n");
    
    // 释放安全管理器
    security_manager_deinit(manager);
    printf("✅ 安全管理器释放成功\n");
    
    return true;
}

/**
 * @brief 测试固件更新检查
 * @return 测试是否通过
 */
bool test_firmware_update(void) {
    printf("\n=== 测试固件更新检查 ===\n");
    
    // 初始化安全管理器
    SecurityManager_t *manager = security_manager_init();
    if (!manager) {
        printf("❌ 安全管理器初始化失败\n");
        return false;
    }
    
    // 检查固件更新
    bool update_available = security_manager_check_update(manager);
    printf("✅ 固件更新检查完成，是否有更新: %s\n", update_available ? "是" : "否");
    
    // 应用固件更新
    bool result = security_manager_apply_update(manager, "http://update-server/firmware.bin");
    printf("✅ 固件更新应用: %s\n", result ? "成功" : "失败");
    
    // 释放安全管理器
    security_manager_deinit(manager);
    printf("✅ 安全管理器释放成功\n");
    
    return true;
}

/**
 * @brief 测试加密密钥设置
 * @return 测试是否通过
 */
bool test_encryption_key(void) {
    printf("\n=== 测试加密密钥设置 ===\n");
    
    // 初始化安全管理器
    SecurityManager_t *manager = security_manager_init();
    if (!manager) {
        printf("❌ 安全管理器初始化失败\n");
        return false;
    }
    
    // 设置新的加密密钥
    const char *new_key = "secure_encryption_key_2026";
    bool result = security_manager_set_encryption_key(manager, new_key, strlen(new_key));
    if (!result) {
        printf("❌ 加密密钥设置失败\n");
        security_manager_deinit(manager);
        return false;
    }
    printf("✅ 加密密钥设置成功\n");
    
    // 释放安全管理器
    security_manager_deinit(manager);
    printf("✅ 安全管理器释放成功\n");
    
    return true;
}

/**
 * @brief 主测试函数
 * @return 测试结果
 */
int main(void) {
    printf("开始安全管理模块单元测试\n\n");
    
    bool result1 = test_security_manager_init();
    bool result2 = test_input_validation();
    bool result3 = test_network_access();
    bool result4 = test_crypto_operations();
    bool result5 = test_security_audit();
    bool result6 = test_firmware_update();
    bool result7 = test_encryption_key();
    
    printf("\n=== 测试结果汇总 ===\n");
    printf("安全管理器初始化测试: %s\n", result1 ? "✅ 通过" : "❌ 失败");
    printf("输入验证测试: %s\n", result2 ? "✅ 通过" : "❌ 失败");
    printf("网络访问测试: %s\n", result3 ? "✅ 通过" : "❌ 失败");
    printf("加密操作测试: %s\n", result4 ? "✅ 通过" : "❌ 失败");
    printf("安全审计测试: %s\n", result5 ? "✅ 通过" : "❌ 失败");
    printf("固件更新测试: %s\n", result6 ? "✅ 通过" : "❌ 失败");
    printf("加密密钥测试: %s\n", result7 ? "✅ 通过" : "❌ 失败");
    
    if (result1 && result2 && result3 && result4 && result5 && result6 && result7) {
        printf("\n🎉 所有测试通过!\n");
        return 0;
    } else {
        printf("\n❌ 部分测试失败!\n");
        return 1;
    }
}
