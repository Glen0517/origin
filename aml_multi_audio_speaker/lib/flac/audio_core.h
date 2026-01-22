/**
 * @file audio_core.h
 * @brief 音频核心模块接口定义
 * @details 提供音频核心模块的所有对外接口，包括初始化、播放控制、音量设置等功能
 * @author AML Audio Team
 * @date 2026-01-22
 */

#ifndef __AUDIO_CORE_H__
#define __AUDIO_CORE_H__

#include "common_def.h"

/******************************************************************************************
 * 音频核心配置结构体
 ******************************************************************************************/
/**
 * @brief 音频核心配置结构体
 * @details 用于配置音频核心模块的各项参数
 */
typedef struct {
    int sample_rate;    // 采样率，支持48000/96000/192000 Hz
    int channel_num;    // 声道数，支持1(单声道)/2(立体声)/5.1(环绕声)
    int pcm_buffer_size;// PCM缓冲区大小，单位为字节
    bool hw_decode_en;  // 是否开启硬件解码，true表示开启，false表示关闭
    bool dolby_dts_en;  // 是否开启杜比DTS解码，true表示开启，false表示关闭
    int init_ok;        // 初始化状态，0表示未初始化，1表示已初始化
} AudioCoreConfig_t;

/******************************************************************************************
 * 对外暴露接口 - 音频核心所有功能，无冗余
 ******************************************************************************************/
/**
 * @brief 音频核心初始化
 * @param cfg 音频配置结构体指针，如果为NULL则使用默认配置
 * @return SUCCESS表示成功，FAILURE表示失败
 * @details 初始化音频核心模块，包括音频硬件、缓冲区、解码器等
 * @example
 * AudioCoreConfig_t cfg;
 * cfg.sample_rate = 48000;
 * cfg.channel_num = 2;
 * cfg.pcm_buffer_size = 1024 * 64;
 * cfg.hw_decode_en = false;
 * cfg.dolby_dts_en = false;
 * audio_core_init(&cfg);
 */
int audio_core_init(AudioCoreConfig_t *cfg);

/**
 * @brief 音频核心反初始化
 * @return SUCCESS表示成功，FAILURE表示失败
 * @details 反初始化音频核心模块，释放所有相关资源
 */
int audio_core_deinit(void);

/**
 * @brief PCM数据播放
 * @param pcm_buf PCM数据缓冲区指针
 * @param buf_len 数据长度，单位为字节
 * @return SUCCESS表示成功，FAILURE表示失败
 * @details 播放PCM格式的音频数据
 */
int audio_core_play_pcm(uint8_t *pcm_buf, int buf_len);

/**
 * @brief 暂停音频播放
 * @return SUCCESS表示成功，FAILURE表示失败
 * @details 暂停当前音频播放，调用resume()可以恢复
 */
int audio_core_pause(void);

/**
 * @brief 恢复音频播放
 * @return SUCCESS表示成功，FAILURE表示失败
 * @details 恢复被暂停的音频播放
 */
int audio_core_resume(void);

/**
 * @brief 停止音频播放
 * @return SUCCESS表示成功，FAILURE表示失败
 * @details 停止当前音频播放，清空缓冲区
 */
int audio_core_stop(void);

/**
 * @brief 音频异常恢复
 * @return SUCCESS表示成功，FAILURE表示失败
 * @details 当音频系统出现异常时，尝试自动恢复
 */
int audio_core_err_recover(void);

/**
 * @brief 设置音量
 * @param vol 音量值，范围0~30，0表示静音，30表示最大音量
 * @return SUCCESS表示成功，FAILURE表示失败
 * @details 设置系统音量
 */
int audio_core_set_volume(int vol);

/**
 * @brief 播放提示音
 * @param tone_data 提示音数据缓冲区指针
 * @param tone_size 提示音数据长度，单位为字节
 * @return SUCCESS表示成功，FAILURE表示失败
 * @details 播放短提示音，优先级高于普通音频播放
 */
int audio_core_play_tone(unsigned char *tone_data, unsigned int tone_size);

#endif // __AUDIO_CORE_H__