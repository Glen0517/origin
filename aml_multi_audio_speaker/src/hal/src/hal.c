/**
 * @file hal.c
 * @brief HAL层主实现文件
 * @details 实现HAL层的初始化和反初始化功能，整合所有子模块的操作
 * @author AML Audio Team
 * @date 2026-01-16
 */

#include "hal.h"
#include "logger.h"

/**
 * @brief HAL层初始化
 * @details 初始化所有HAL层模块
 * @return 初始化结果：0表示成功，非0表示失败
 */
int hal_init(void) {
    LOG_INFO("Starting HAL layer initialization...");
    
    // 初始化音频模块
    if (hal_audio_init() != 0) {
        LOG_ERROR("HAL audio init failed");
        return FAILURE;
    }
    
    // 初始化蓝牙模块
    if (hal_bt_init() != 0) {
        LOG_ERROR("HAL bluetooth init failed");
        return FAILURE;
    }
    
    // 初始化USB模块
    if (hal_usb_init() != 0) {
        LOG_ERROR("HAL USB init failed");
        return FAILURE;
    }
    
    // 初始化外设模块
    if (hal_peri_init() != 0) {
        LOG_ERROR("HAL peripheral init failed");
        return FAILURE;
    }
    
    LOG_INFO("HAL layer initialization completed successfully");
    return SUCCESS;
}

/**
 * @brief HAL层反初始化
 * @details 反初始化所有HAL层模块
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int hal_deinit(void) {
    LOG_INFO("Starting HAL layer deinitialization...");
    
    // 反初始化外设模块
    if (hal_peri_deinit() != 0) {
        LOG_ERROR("HAL peripheral deinit failed");
    }
    
    // 反初始化USB模块
    if (hal_usb_deinit() != 0) {
        LOG_ERROR("HAL USB deinit failed");
    }
    
    // 反初始化蓝牙模块
    if (hal_bt_deinit() != 0) {
        LOG_ERROR("HAL bluetooth deinit failed");
    }
    
    // 反初始化音频模块
    if (hal_audio_deinit() != 0) {
        LOG_ERROR("HAL audio deinit failed");
    }
    
    LOG_INFO("HAL layer deinitialization completed");
    return SUCCESS;
}