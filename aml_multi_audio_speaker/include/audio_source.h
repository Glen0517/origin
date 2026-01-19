#ifndef __AUDIO_SOURCE_H__
#define __AUDIO_SOURCE_H__

#include "common_def.h"
/******************************************************************************************
 * 音源优先级配置结构体
 ******************************************************************************************/
typedef struct {
    AudioSourceType_e source_list[6]; // 音源优先级列表
    int source_cnt;                   // 有效音源数量
    AudioSourceType_e default_source; // 默认音源
    bool auto_switch_en;              // 是否开启自动切换
} AudioSourceConfig_t;

/******************************************************************************************
 * 对外暴露接口 - 音源管理所有功能
 ******************************************************************************************/
/**
 * @brief  音源模块初始化
 * @param  cfg 音源配置结构体指针
 * @return SUCCESS/FAILURE
 */
int audio_source_init(AudioSourceConfig_t *cfg);

/**
 * @brief  音源模块反初始化
 * @return SUCCESS/FAILURE
 */
int audio_source_deinit(void);

/**
 * @brief  切换到指定音源
 * @param  source 音源类型
 * @return SUCCESS/FAILURE/NOT_SUPPORT
 */
int audio_source_switch(AudioSourceType_e source);

/**
 * @brief  切换到下一个音源(按优先级)
 * @return 切换后的音源类型
 */
AudioSourceType_e audio_source_switch_next(void);

/**
 * @brief  获取当前音源类型
 * @return 当前音源类型
 */
AudioSourceType_e audio_source_get_current(void);

/**
 * @brief  音源状态检测
 * @param  source 音源类型
 * @return TRUE-有信号 FALSE-无信号
 */
bool audio_source_detect(AudioSourceType_e source);

/**
 * @brief  音源事件轮询
 * @return 无
 */
void audio_source_event_poll(void);

#endif // __AUDIO_SOURCE_H__