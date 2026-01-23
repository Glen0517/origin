/**
 * @file bt_a2dp.c
 * @brief 蓝牙A2DP功能实现
 * @details 实现蓝牙A2DP协议的相关功能，包括音频流传输、播放控制等
 * @author AML Audio Team
 * @date 2026-01-16
 */

#include "bluetooth_priv.h"
#include "logger.h"
#include "hal.h"

/**
 * @brief 初始化蓝牙A2DP功能
 * @details 初始化A2DP协议相关的配置和资源
 * @return 初始化结果：0表示成功，非0表示失败
 */
int bt_a2dp_init(void)
{
    LOG_INFO("Initializing Bluetooth A2DP...");
    
    // 初始化A2DP协议
    if (hal_bt_a2dp_init() != 0) {
        LOG_ERROR("HAL A2DP init failed");
        return FAILURE;
    }
    
    // 设置A2DP音频参数
    // 采样率：44.1kHz
    // 声道数：2
    // 比特率：16bit
    if (hal_bt_a2dp_set_audio_params(44100, 2, 16) != 0) {
        LOG_ERROR("Failed to set A2DP audio parameters");
        // 继续执行，使用默认参数
    }
    
    LOG_INFO("Bluetooth A2DP init success");
    return SUCCESS;
}

/**
 * @brief 反初始化蓝牙A2DP功能
 * @details 释放A2DP协议相关的资源
 * @return 反初始化结果：0表示成功，非0表示失败
 */
void bt_a2dp_deinit(void)
{
    LOG_INFO("Deinitializing Bluetooth A2DP...");
    
    // 反初始化A2DP协议
    if (hal_bt_a2dp_deinit() != 0) {
        LOG_ERROR("HAL A2DP deinit failed");
    }
    
    LOG_INFO("Bluetooth A2DP deinit success");
}

/**
 * @brief 开始蓝牙A2DP音频流传输
 * @details 启动A2DP音频流的接收和播放
 * @return 操作结果：0表示成功，非0表示失败
 */
int bt_a2dp_start_stream(void)
{
    LOG_INFO("Starting Bluetooth A2DP stream...");
    
    if (hal_bt_a2dp_start_stream() != 0) {
        LOG_ERROR("Failed to start A2DP stream");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth A2DP stream started");
    return SUCCESS;
}

/**
 * @brief 停止蓝牙A2DP音频流传输
 * @details 停止A2DP音频流的接收和播放
 * @return 操作结果：0表示成功，非0表示失败
 */
int bt_a2dp_stop_stream(void)
{
    LOG_INFO("Stopping Bluetooth A2DP stream...");
    
    if (hal_bt_a2dp_stop_stream() != 0) {
        LOG_ERROR("Failed to stop A2DP stream");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth A2DP stream stopped");
    return SUCCESS;
}

/**
 * @brief 设置蓝牙A2DP音量
 * @details 设置A2DP音频流的音量大小
 * @param volume 音量值（0-100）
 * @return 操作结果：0表示成功，非0表示失败
 */
int bt_a2dp_set_volume(int volume)
{
    if (volume < 0 || volume > 100) {
        LOG_ERROR("Invalid A2DP volume: %d", volume);
        return FAILURE;
    }
    
    LOG_INFO("Setting Bluetooth A2DP volume to %d", volume);
    
    if (hal_bt_a2dp_set_volume(volume) != 0) {
        LOG_ERROR("Failed to set A2DP volume");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 获取蓝牙A2DP音量
 * @details 获取当前A2DP音频流的音量大小
 * @return 音量值（0-100），失败返回-1
 */
int bt_a2dp_get_volume(void)
{
    int volume = hal_bt_a2dp_get_volume();
    
    if (volume < 0) {
        LOG_ERROR("Failed to get A2DP volume");
        return -1;
    }
    
    LOG_DEBUG("Current Bluetooth A2DP volume: %d", volume);
    return volume;
}

/**
 * @brief 设置蓝牙A2DP缓冲区大小
 * @details 设置A2DP音频流的缓冲区大小
 * @param buffer_size 缓冲区大小（毫秒）
 * @return 操作结果：0表示成功，非0表示失败
 */
int bt_a2dp_set_buffer_size(int buffer_size)
{
    if (buffer_size < 0) {
        LOG_ERROR("Invalid buffer size: %d", buffer_size);
        return FAILURE;
    }
    
    LOG_INFO("Setting Bluetooth A2DP buffer size to %d ms", buffer_size);
    
    if (hal_bt_a2dp_set_buffer_size(buffer_size) != 0) {
        LOG_ERROR("Failed to set A2DP buffer size");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 获取蓝牙A2DP缓冲区大小
 * @details 获取当前A2DP音频流的缓冲区大小
 * @return 缓冲区大小（毫秒），失败返回-1
 */
int bt_a2dp_get_buffer_size(void)
{
    int buffer_size = hal_bt_a2dp_get_buffer_size();
    
    if (buffer_size < 0) {
        LOG_ERROR("Failed to get A2DP buffer size");
        return -1;
    }
    
    LOG_DEBUG("Current Bluetooth A2DP buffer size: %d ms", buffer_size);
    return buffer_size;
}

/**
 * @brief 蓝牙A2DP播放控制：播放
 * @details 发送播放命令到A2DP设备
 * @return 操作结果：0表示成功，非0表示失败
 */
int bt_a2dp_play(void)
{
    LOG_INFO("Bluetooth A2DP play");
    
    if (hal_bt_a2dp_play() != 0) {
        LOG_ERROR("Failed to send A2DP play command");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 蓝牙A2DP播放控制：暂停
 * @details 发送暂停命令到A2DP设备
 * @return 操作结果：0表示成功，非0表示失败
 */
int bt_a2dp_pause(void)
{
    LOG_INFO("Bluetooth A2DP pause");
    
    if (hal_bt_a2dp_pause() != 0) {
        LOG_ERROR("Failed to send A2DP pause command");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 蓝牙A2DP播放控制：停止
 * @details 发送停止命令到A2DP设备
 * @return 操作结果：0表示成功，非0表示失败
 */
int bt_a2dp_stop(void)
{
    LOG_INFO("Bluetooth A2DP stop");
    
    if (hal_bt_a2dp_stop() != 0) {
        LOG_ERROR("Failed to send A2DP stop command");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 蓝牙A2DP播放控制：下一曲
 * @details 发送下一曲命令到A2DP设备
 * @return 操作结果：0表示成功，非0表示失败
 */
int bt_a2dp_next(void)
{
    LOG_INFO("Bluetooth A2DP next");
    
    if (hal_bt_a2dp_next() != 0) {
        LOG_ERROR("Failed to send A2DP next command");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 蓝牙A2DP播放控制：上一曲
 * @details 发送上一曲命令到A2DP设备
 * @return 操作结果：0表示成功，非0表示失败
 */
int bt_a2dp_prev(void)
{
    LOG_INFO("Bluetooth A2DP previous");
    
    if (hal_bt_a2dp_prev() != 0) {
        LOG_ERROR("Failed to send A2DP previous command");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 蓝牙A2DP音量控制：音量增加
 * @details 发送音量增加命令到A2DP设备
 * @return 操作结果：0表示成功，非0表示失败
 */
int bt_a2dp_volume_up(void)
{
    LOG_INFO("Bluetooth A2DP volume up");
    
    if (hal_bt_a2dp_volume_up() != 0) {
        LOG_ERROR("Failed to send A2DP volume up command");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 蓝牙A2DP音量控制：音量减少
 * @details 发送音量减少命令到A2DP设备
 * @return 操作结果：0表示成功，非0表示失败
 */
int bt_a2dp_volume_down(void)
{
    LOG_INFO("Bluetooth A2DP volume down");
    
    if (hal_bt_a2dp_volume_down() != 0) {
        LOG_ERROR("Failed to send A2DP volume down command");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 获取蓝牙A2DP媒体状态
 * @details 获取当前A2DP设备的媒体播放状态
 * @return 媒体状态：1表示正在播放，0表示停止，-1表示失败
 */
int bt_a2dp_get_media_status(void)
{
    int media_status = hal_bt_a2dp_get_media_status();
    
    if (media_status < 0) {
        LOG_ERROR("Failed to get A2DP media status");
        return -1;
    }
    
    LOG_DEBUG("Current Bluetooth A2DP media status: %d", media_status);
    return media_status;
}
