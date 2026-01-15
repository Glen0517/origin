#ifndef __AUDIO_CORE_H__
#define __AUDIO_CORE_H__

#include "common_def.h"

/******************************************************************************************
 * 音频核心配置结构体
 ******************************************************************************************/
typedef struct {
    int sample_rate;    // 采样率 48000/96000/192000
    int channel_num;    // 声道数 1/2/5.1
    int pcm_buffer_size;// PCM缓冲区大小
    bool hw_decode_en;  // 是否开启硬件解码
    bool dolby_dts_en;  // 是否开启杜比DTS
} AudioCoreConfig_t;

/******************************************************************************************
 * 对外暴露接口 - 音频核心所有功能，无冗余
 ******************************************************************************************/
/**
 * @brief  音频核心初始化
 * @param  cfg 音频配置结构体指针
 * @return SUCCESS/FAILURE
 */
int audio_core_init(AudioCoreConfig_t *cfg);

/**
 * @brief  音频核心反初始化
 * @return SUCCESS/FAILURE
 */
int audio_core_deinit(void);

/**
 * @brief  PCM数据播放
 * @param  pcm_buf PCM数据缓冲区
 * @param  buf_len 数据长度
 * @return SUCCESS/FAILURE
 */
int audio_core_play_pcm(uint8_t *pcm_buf, int buf_len);

/**
 * @brief  暂停音频播放
 * @return SUCCESS/FAILURE
 */
int audio_core_pause(void);

/**
 * @brief  恢复音频播放
 * @return SUCCESS/FAILURE
 */
int audio_core_resume(void);

/**
 * @brief  停止音频播放
 * @return SUCCESS/FAILURE
 */
int audio_core_stop(void);

/**
 * @brief  音频异常恢复
 * @return SUCCESS/FAILURE
 */
int audio_core_err_recover(void);

/**
 * @brief  设置音量
 * @param  vol 音量值 0~30
 * @return SUCCESS/FAILURE
 */
int audio_core_set_volume(int vol);

#endif // __AUDIO_CORE_H__