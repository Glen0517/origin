/**
 * @file hal_bt.c
 * @brief 蓝牙硬件抽象实现
 * @details 实现蓝牙硬件抽象层的接口函数，封装Amlogic蓝牙SDK的功能
 * @author AML Audio Team
 * @date 2026-01-16
 */

#include "hal_bt.h"
#include "logger.h"
#include <aml_bt.h>
#include <aml_bt_a2dp.h>
#include <aml_bt_hfp.h>

static bool g_bt_init = false;

/**
 * @brief 初始化蓝牙硬件
 * @details 初始化Amlogic蓝牙SDK，准备蓝牙硬件
 * @return 初始化结果：0表示成功，非0表示失败
 */
int hal_bt_init(void) {
    if (g_bt_init) {
        LOG_INFO("HAL bluetooth already initialized");
        return SUCCESS;
    }
    
    if (aml_bt_init() != 0) {
        LOG_ERROR("Amlogic bluetooth SDK init failed");
        return FAILURE;
    }
    
    g_bt_init = true;
    LOG_INFO("HAL bluetooth init success");
    return SUCCESS;
}

/**
 * @brief 反初始化蓝牙硬件
 * @details 反初始化Amlogic蓝牙SDK，清理蓝牙硬件资源
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int hal_bt_deinit(void) {
    if (!g_bt_init) {
        LOG_INFO("HAL bluetooth not initialized");
        return SUCCESS;
    }
    
    if (aml_bt_deinit() != 0) {
        LOG_ERROR("Amlogic bluetooth SDK deinit failed");
        return FAILURE;
    }
    
    g_bt_init = false;
    LOG_INFO("HAL bluetooth deinit success");
    return SUCCESS;
}

/**
 * @brief 获取蓝牙连接状态
 * @details 获取当前蓝牙设备的连接状态
 * @return 连接状态：1表示已连接，0表示未连接
 */
int hal_bt_get_connection_status(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return 0;
    }
    
    return aml_bt_get_connection_status();
}

/**
 * @brief 获取蓝牙A2DP媒体状态
 * @details 获取蓝牙A2DP音频流的播放状态
 * @return 媒体状态：1表示正在播放，0表示停止
 */
int hal_bt_a2dp_get_media_status(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return 0;
    }
    
    return aml_bt_a2dp_get_media_status();
}

/**
 * @brief 轮询蓝牙事件
 * @details 处理蓝牙相关的事件，包括连接、断开、媒体流等
 * @return 轮询结果：0表示成功，非0表示失败
 */
int hal_bt_event_poll(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    // 轮询蓝牙A2DP事件
    if (hal_bt_a2dp_event_poll() != 0) {
        LOG_ERROR("Poll A2DP event failed");
    }
    
    // 轮询蓝牙HFP事件
    if (hal_bt_hfp_event_poll() != 0) {
        LOG_ERROR("Poll HFP event failed");
    }
    
    // 轮询蓝牙MESH事件（如果启用）
    #ifdef CONFIG_ENABLE_BT_MESH
    if (hal_bt_mesh_event_poll() != 0) {
        LOG_ERROR("Poll MESH event failed");
    }
    #endif
    
    return SUCCESS;
}

/**
 * @brief 轮询蓝牙A2DP事件
 * @details 处理蓝牙A2DP相关的事件
 * @return 轮询结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_event_poll(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_a2dp_event_poll();
    if (ret != 0) {
        LOG_ERROR("Amlogic A2DP event poll failed: %d", ret);
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 轮询蓝牙HFP事件
 * @details 处理蓝牙HFP（免提电话）相关的事件
 * @return 轮询结果：0表示成功，非0表示失败
 */
int hal_bt_hfp_event_poll(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    int ret = aml_bt_hfp_event_poll();
    if (ret != 0) {
        LOG_ERROR("Amlogic HFP event poll failed: %d", ret);
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 轮询蓝牙MESH事件
 * @details 处理蓝牙MESH网络相关的事件
 * @return 轮询结果：0表示成功，非0表示失败
 */
int hal_bt_mesh_event_poll(void) {
    if (!g_bt_init) {
        LOG_ERROR("HAL bluetooth not initialized");
        return FAILURE;
    }
    
    #ifdef CONFIG_ENABLE_BT_MESH
    int ret = aml_bt_mesh_event_poll();
    if (ret != 0) {
        LOG_ERROR("Amlogic MESH event poll failed: %d", ret);
        return FAILURE;
    }
    return SUCCESS;
    #else
    LOG_WARN("BT MESH not enabled");
    return SUCCESS;
    #endif
}