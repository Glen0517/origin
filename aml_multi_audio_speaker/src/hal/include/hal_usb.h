/**
 * @file hal_usb.h
 * @brief USB硬件抽象接口
 * @details 定义USB硬件抽象层的接口函数，封装Amlogic USB SDK的功能
 * @author AML Audio Team
 * @date 2026-01-16
 */

#ifndef HAL_USB_H
#define HAL_USB_H

#include "common_def.h"

/**
 * @brief USB设备连接状态回调函数
 * @param dev_path USB设备路径
 * @param connected 连接状态：true表示已连接，false表示已断开
 */
typedef void (*HalUsbDeviceCallback_t)(const char *dev_path, bool connected);

/**
 * @brief USB音频数据接收回调函数
 * @param pcm_data PCM音频数据指针
 * @param data_len 数据长度，单位为字节
 */
typedef void (*HalUsbDataCallback_t)(uint8_t *pcm_data, int data_len);

/**
 * @brief 初始化USB硬件
 * @details 初始化Amlogic USB SDK，准备USB硬件
 * @return 初始化结果：0表示成功，非0表示失败
 */
int hal_usb_init(void);

/**
 * @brief 反初始化USB硬件
 * @details 反初始化Amlogic USB SDK，清理USB硬件资源
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int hal_usb_deinit(void);

/**
 * @brief 打开USB音频设备
 * @details 打开指定路径的USB音频设备，准备音频数据接收
 * @param dev_path USB音频设备路径
 * @return 打开结果：0表示成功，非0表示失败
 */
int hal_usb_audio_open(const char *dev_path);

/**
 * @brief 关闭USB音频设备
 * @details 关闭当前打开的USB音频设备
 * @return 关闭结果：0表示成功，非0表示失败
 */
int hal_usb_audio_close(void);

/**
 * @brief 设置USB设备连接状态回调
 * @details 注册USB设备连接状态变化的回调函数
 * @param callback 回调函数指针，指向处理设备连接状态变化的函数
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_usb_audio_set_device_callback(HalUsbDeviceCallback_t callback);

/**
 * @brief 设置USB音频数据接收回调
 * @details 注册USB音频数据接收的回调函数
 * @param callback 回调函数指针，指向处理音频数据的函数
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_usb_audio_set_data_callback(HalUsbDataCallback_t callback);

/**
 * @brief 启用/禁用USB音频设备检测
 * @details 控制是否检测USB音频设备的连接状态
 * @param enable 是否启用检测：true表示启用，false表示禁用
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_usb_audio_enable_detection(bool enable);

#endif /* HAL_USB_H */