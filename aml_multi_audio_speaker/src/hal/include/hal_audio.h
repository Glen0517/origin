/**
 * @file hal_audio.h
 * @brief 音频硬件抽象接口
 * @details 定义音频硬件抽象层的接口函数，封装Amlogic音频SDK的功能
 * @author AML Audio Team
 * @date 2026-01-16
 */

#ifndef HAL_AUDIO_H
#define HAL_AUDIO_H

#include "common_def.h"

/**
 * @brief 初始化音频硬件
 * @details 初始化Amlogic音频SDK，准备音频硬件
 * @return 初始化结果：0表示成功，非0表示失败
 */
int hal_audio_init(void);

/**
 * @brief 反初始化音频硬件
 * @details 反初始化Amlogic音频SDK，清理音频硬件资源
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int hal_audio_deinit(void);

/**
 * @brief 设置音频采样率
 * @details 配置音频硬件的采样率
 * @param sample_rate 采样率值，如44100、48000等
 * @return 配置结果：0表示成功，非0表示失败
 */
int hal_audio_set_sample_rate(int sample_rate);

/**
 * @brief 设置音频通道数
 * @details 配置音频硬件的通道数
 * @param channels 通道数，如1（单声道）、2（立体声）等
 * @return 配置结果：0表示成功，非0表示失败
 */
int hal_audio_set_channels(int channels);

/**
 * @brief 播放PCM音频数据
 * @details 向音频硬件写入PCM格式的音频数据并播放
 * @param data 音频数据指针，指向PCM数据缓冲区
 * @param len 数据长度，单位为字节
 * @return 播放结果：0表示成功，非0表示失败
 */
int hal_audio_play_pcm(uint8_t *data, int len);

/**
 * @brief 停止音频播放
 * @details 停止当前的音频播放
 * @return 停止结果：0表示成功，非0表示失败
 */
int hal_audio_stop(void);

/**
 * @brief 获取音频播放状态
 * @details 获取当前音频播放的状态
 * @return 播放状态：1表示正在播放，0表示停止
 */
int hal_audio_get_status(void);

/**
 * @brief 设置音频硬件解码
 * @details 启用或禁用音频硬件解码功能
 * @param enable 是否启用硬件解码：true表示启用，false表示禁用
 * @return 配置结果：0表示成功，非0表示失败
 */
int hal_audio_set_hw_decode(bool enable);

/**
 * @brief 设置杜比DTS功能
 * @details 启用或禁用杜比DTS音效功能
 * @param enable 是否启用杜比DTS：true表示启用，false表示禁用
 * @return 配置结果：0表示成功，非0表示失败
 */
int hal_audio_set_dolby_dts(bool enable);

/**
 * @brief 播放测试音频
 * @details 播放用于测试的音频信号
 * @return 播放结果：0表示成功，非0表示失败
 */
int hal_audio_play_test_audio(void);

/**
 * @brief 停止测试音频播放
 * @details 停止当前播放的测试音频
 * @return 停止结果：0表示成功，非0表示失败
 */
int hal_audio_stop_test_audio(void);

/**
 * @brief 暂停音频播放
 * @details 暂停当前的音频播放
 * @return 暂停结果：0表示成功，非0表示失败
 */
int hal_audio_pause(void);

/**
 * @brief 恢复音频播放
 * @details 恢复暂停的音频播放
 * @return 恢复结果：0表示成功，非0表示失败
 */
int hal_audio_resume(void);

/**
 * @brief 设置音量
 * @details 设置音频播放的音量
 * @param volume 音量值，范围根据硬件而定
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_audio_set_volume(int volume);

#endif /* HAL_AUDIO_H */