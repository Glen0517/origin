#ifndef __VOLUME_CTRL_H__
#define __VOLUME_CTRL_H__

#include "common_def.h"

/******************************************************************************************
 * 对外暴露接口 - 音量控制所有功能，宏控裁剪音量轨数
 ******************************************************************************************/
/**
 * @brief  音量模块初始化
 * @param  vol 初始音量结构体
 * @return SUCCESS/FAILURE
 */
int volume_ctrl_init(VolumeInfo_t *vol);

/**
 * @brief  音量模块反初始化
 * @return SUCCESS/FAILURE
 */
int volume_ctrl_deinit(void);

/**
 * @brief  设置主音量
 * @param  vol 音量值 0~30
 * @return SUCCESS/FAILURE/INVALID_PARAM
 */
int volume_ctrl_set_master(int vol);

/**
 * @brief  主音量加
 * @return 加后的音量值
 */
int volume_ctrl_master_up(void);

/**
 * @brief  主音量减
 * @return 减后的音量值
 */
int volume_ctrl_master_down(void);

/**
 * @brief  设置静音状态
 * @param  is_mute TRUE-静音 FALSE-取消静音
 * @return SUCCESS/FAILURE
 */
int volume_ctrl_set_mute(bool is_mute);

/**
 * @brief  获取当前音量信息
 * @param  vol 音量结构体指针
 * @return SUCCESS/FAILURE
 */
int volume_ctrl_get_info(VolumeInfo_t *vol);

#if CONFIG_ENABLE_2VOL_CTRL || CONFIG_ENABLE_3VOL_CTRL
/**
 * @brief  设置低音音量(双/三轨专属)
 * @param  vol 音量值 0~20
 * @return SUCCESS/FAILURE
 */
int volume_ctrl_set_bass(int vol);

/**
 * @brief  低音音量加(双/三轨专属)
 * @return 加后的音量值
 */
int volume_ctrl_bass_up(void);

/**
 * @brief  低音音量减(双/三轨专属)
 * @return 减后的音量值
 */
int volume_ctrl_bass_down(void);
#endif

#if CONFIG_ENABLE_3VOL_CTRL
/**
 * @brief  设置高音音量(三轨专属，仅高端)
 * @param  vol 音量值 0~20
 * @return SUCCESS/FAILURE
 */
int volume_ctrl_set_treble(int vol);

/**
 * @brief  高音音量加(三轨专属，仅高端)
 * @return 加后的音量值
 */
int volume_ctrl_treble_up(void);

/**
 * @brief  高音音量减(三轨专属，仅高端)
 * @return 减后的音量值
 */
int volume_ctrl_treble_down(void);
#endif

#endif // __VOLUME_CTRL_H__