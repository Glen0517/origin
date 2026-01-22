/**
 * @file pal.c
 * @brief PAL层主实现文件
 * @details 实现PAL层的初始化和反初始化功能，整合所有子模块的操作
 * @author AML Audio Team
 * @date 2026-01-16
 */

#include "pal.h"
#include "log/aml_log.h"

// 定义PAL模块日志分类
AML_LOG_DEFINE(pal_log);
// 设置默认日志分类
#define AML_LOG_DEFAULT AML_LOG_GET_CAT(pal_log)

/**
 * @brief PAL层初始化
 * @details 初始化所有PAL层模块
 * @return 初始化结果：0表示成功，非0表示失败
 */
int pal_init(void) {
    AML_LOGI("Starting PAL layer initialization...");
    
    // 初始化系统服务模块
    if (pal_system_init() != 0) {
        AML_LOGE("PAL system service init failed");
        return FAILURE;
    }
    
    // 初始化存储服务模块
    if (pal_storage_init() != 0) {
        AML_LOGE("PAL storage service init failed");
        return FAILURE;
    }
    
    // 初始化网络服务模块
    if (pal_network_init() != 0) {
        AML_LOGE("PAL network service init failed");
        return FAILURE;
    }
    
    AML_LOGI("PAL layer initialization completed successfully");
    return SUCCESS;
}

/**
 * @brief PAL层反初始化
 * @details 反初始化所有PAL层模块
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int pal_deinit(void) {
    AML_LOGI("Starting PAL layer deinitialization...");
    
    // 反初始化网络服务模块
    if (pal_network_deinit() != 0) {
        AML_LOGE("PAL network service deinit failed");
    }
    
    // 反初始化存储服务模块
    if (pal_storage_deinit() != 0) {
        AML_LOGE("PAL storage service deinit failed");
    }
    
    // 反初始化系统服务模块
    if (pal_system_deinit() != 0) {
        AML_LOGE("PAL system service deinit failed");
    }
    
    AML_LOGI("PAL layer deinitialization completed");
    return SUCCESS;
}