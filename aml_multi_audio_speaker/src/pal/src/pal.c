/**
 * @file pal.c
 * @brief PAL层主实现文件
 * @details 实现PAL层的初始化和反初始化功能，整合所有子模块的操作
 * @author AML Audio Team
 * @date 2026-01-16
 */

#include "pal.h"
#include "logger.h"

/**
 * @brief PAL层初始化
 * @details 初始化所有PAL层模块
 * @return 初始化结果：0表示成功，非0表示失败
 */
int pal_init(void) {
    LOG_INFO("Starting PAL layer initialization...");
    
    // 初始化系统服务模块
    if (pal_system_init() != 0) {
        LOG_ERROR("PAL system service init failed");
        return FAILURE;
    }
    
    // 初始化存储服务模块
    if (pal_storage_init() != 0) {
        LOG_ERROR("PAL storage service init failed");
        return FAILURE;
    }
    
    // 初始化网络服务模块
    if (pal_network_init() != 0) {
        LOG_ERROR("PAL network service init failed");
        return FAILURE;
    }
    
    LOG_INFO("PAL layer initialization completed successfully");
    return SUCCESS;
}

/**
 * @brief PAL层反初始化
 * @details 反初始化所有PAL层模块
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int pal_deinit(void) {
    LOG_INFO("Starting PAL layer deinitialization...");
    
    // 反初始化网络服务模块
    if (pal_network_deinit() != 0) {
        LOG_ERROR("PAL network service deinit failed");
    }
    
    // 反初始化存储服务模块
    if (pal_storage_deinit() != 0) {
        LOG_ERROR("PAL storage service deinit failed");
    }
    
    // 反初始化系统服务模块
    if (pal_system_deinit() != 0) {
        LOG_ERROR("PAL system service deinit failed");
    }
    
    LOG_INFO("PAL layer deinitialization completed");
    return SUCCESS;
}