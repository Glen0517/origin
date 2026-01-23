/**
 * @file hal_usb.c
 * @brief USB硬件抽象实现
 * @details 实现USB硬件抽象层的接口函数，封装Amlogic USB SDK的功能
 * @author AML Audio Team
 * @date 2026-01-16
 */

#include "hal_usb.h"
#include "logger.h"
#include <aml_usb_audio.h>

static bool g_usb_init = false;
static HalUsbDeviceCallback_t g_device_callback = NULL;
static HalUsbDataCallback_t g_data_callback = NULL;

/**
 * @brief Amlogic USB设备连接状态回调函数
 * @param dev_path USB设备路径
 * @param connected 连接状态：true表示已连接，false表示已断开
 */
static void aml_usb_device_callback(const char *dev_path, bool connected) {
    if (g_device_callback) {
        g_device_callback(dev_path, connected);
    }
}

/**
 * @brief Amlogic USB音频数据接收回调函数
 * @param pcm_data PCM音频数据指针
 * @param data_len 数据长度，单位为字节
 */
static void aml_usb_data_callback(uint8_t *pcm_data, int data_len) {
    if (g_data_callback) {
        g_data_callback(pcm_data, data_len);
    }
}

/**
 * @brief 初始化USB硬件
 * @details 初始化Amlogic USB SDK，准备USB硬件
 * @return 初始化结果：0表示成功，非0表示失败
 */
int hal_usb_init(void) {
    if (g_usb_init) {
        LOG_INFO("HAL USB already initialized");
        return SUCCESS;
    }
    
    if (aml_usb_audio_init() != 0) {
        LOG_ERROR("Amlogic USB audio SDK init failed");
        return FAILURE;
    }
    
    g_usb_init = true;
    LOG_INFO("HAL USB init success");
    return SUCCESS;
}

/**
 * @brief 反初始化USB硬件
 * @details 反初始化Amlogic USB SDK，清理USB硬件资源
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int hal_usb_deinit(void) {
    if (!g_usb_init) {
        LOG_INFO("HAL USB not initialized");
        return SUCCESS;
    }
    
    if (aml_usb_audio_deinit() != 0) {
        LOG_ERROR("Amlogic USB audio SDK deinit failed");
        return FAILURE;
    }
    
    g_usb_init = false;
    g_device_callback = NULL;
    g_data_callback = NULL;
    LOG_INFO("HAL USB deinit success");
    return SUCCESS;
}

/**
 * @brief 打开USB音频设备
 * @details 打开指定路径的USB音频设备，准备音频数据接收
 * @param dev_path USB音频设备路径
 * @return 打开结果：0表示成功，非0表示失败
 */
int hal_usb_audio_open(const char *dev_path) {
    if (!g_usb_init) {
        LOG_ERROR("HAL USB not initialized");
        return FAILURE;
    }
    
    if (!dev_path || strlen(dev_path) == 0) {
        LOG_ERROR("Invalid USB device path: NULL or empty");
        return INVALID_PARAM;
    }
    
    if (strlen(dev_path) > PATH_MAX) {
        LOG_ERROR("Invalid USB device path: too long");
        return INVALID_PARAM;
    }
    
    if (aml_usb_audio_open(dev_path) != 0) {
        LOG_ERROR("Open USB audio device failed: %s", dev_path);
        return FAILURE;
    }
    
    LOG_INFO("USB audio device opened successfully: %s", dev_path);
    return SUCCESS;
}

/**
 * @brief 关闭USB音频设备
 * @details 关闭当前打开的USB音频设备
 * @return 关闭结果：0表示成功，非0表示失败
 */
int hal_usb_audio_close(void) {
    if (!g_usb_init) {
        LOG_ERROR("HAL USB not initialized");
        return FAILURE;
    }
    
    if (aml_usb_audio_close() != 0) {
        LOG_ERROR("Close USB audio device failed");
        return FAILURE;
    }
    
    LOG_INFO("USB audio device closed");
    return SUCCESS;
}

/**
 * @brief 设置USB设备连接状态回调
 * @details 注册USB设备连接状态变化的回调函数
 * @param callback 回调函数指针，指向处理设备连接状态变化的函数
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_usb_audio_set_device_callback(HalUsbDeviceCallback_t callback) {
    if (!g_usb_init) {
        LOG_ERROR("HAL USB not initialized");
        return FAILURE;
    }
    
    g_device_callback = callback;
    
    if (aml_usb_audio_set_device_callback(aml_usb_device_callback) != 0) {
        LOG_ERROR("Set USB device callback failed");
        return FAILURE;
    }
    
    LOG_INFO("USB device callback set %s", callback ? "successfully" : "to NULL");
    return SUCCESS;
}

/**
 * @brief 设置USB音频数据接收回调
 * @details 注册USB音频数据接收的回调函数
 * @param callback 回调函数指针，指向处理音频数据的函数
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_usb_audio_set_data_callback(HalUsbDataCallback_t callback) {
    if (!g_usb_init) {
        LOG_ERROR("HAL USB not initialized");
        return FAILURE;
    }
    
    g_data_callback = callback;
    
    if (aml_usb_audio_set_data_callback(aml_usb_data_callback) != 0) {
        LOG_ERROR("Set USB data callback failed");
        return FAILURE;
    }
    
    LOG_INFO("USB data callback set %s", callback ? "successfully" : "to NULL");
    return SUCCESS;
}

/**
 * @brief 启用/禁用USB音频设备检测
 * @details 控制是否检测USB音频设备的连接状态
 * @param enable 是否启用检测：true表示启用，false表示禁用
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_usb_audio_enable_detection(bool enable) {
    if (!g_usb_init) {
        LOG_ERROR("HAL USB not initialized");
        return FAILURE;
    }
    
    if (aml_usb_audio_enable_detection(enable) != 0) {
        LOG_ERROR("Enable USB detection failed");
        return FAILURE;
    }
    
    LOG_INFO("USB detection %s", enable ? "enabled" : "disabled");
    return SUCCESS;
}