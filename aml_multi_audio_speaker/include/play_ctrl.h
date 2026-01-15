#ifndef __PLAY_CTRL_H__
#define __PLAY_CTRL_H__

#include "common_def.h"

/******************************************************************************************
 * 播放控制配置结构体
 ******************************************************************************************/
typedef struct {
    bool power_off_resume_en; // 是否开启断电续播
    bool boot_default_play_en;// 是否开机默认播放
    int bt_reconnect_timeout; // 蓝牙重连超时
    SoundMode_e default_mode; // 默认声场模式
} PlayCtrlConfig_t;

/******************************************************************************************
 * 对外暴露接口 - 播放控制所有功能
 ******************************************************************************************/
/**
 * @brief  播放控制模块初始化
 * @param  cfg 播放配置结构体指针
 * @return SUCCESS/FAILURE
 */
int play_ctrl_init(PlayCtrlConfig_t *cfg);

/**
 * @brief  播放控制模块反初始化
 * @return SUCCESS/FAILURE
 */
int play_ctrl_deinit(void);

/**
 * @brief  设置播放状态
 * @param  state 播放状态
 * @return SUCCESS/FAILURE
 */
int play_ctrl_set_state(PlayState_e state);

/**
 * @brief  获取当前播放状态
 * @return 当前播放状态
 */
PlayState_e play_ctrl_get_state(void);

/**
 * @brief  设置声场模式
 * @param  mode 声场模式
 * @return SUCCESS/FAILURE/NOT_SUPPORT
 */
int play_ctrl_set_sound_mode(SoundMode_e mode);

/**
 * @brief  获取当前声场模式
 * @return 当前声场模式
 */
SoundMode_e play_ctrl_get_sound_mode(void);

/**
 * @brief  下一曲
 * @return SUCCESS/FAILURE
 */
int play_ctrl_next_song(void);

/**
 * @brief  上一曲
 * @return SUCCESS/FAILURE
 */
int play_ctrl_prev_song(void);

#endif // __PLAY_CTRL_H__