/**
 * @file voice_noise_reduction.h
 * @brief 语音降噪模块接口
 * @details 提供语音降噪、回声消除和语音增强功能
 * @author AML Audio Team
 * @date 2026-01-19
 */

#ifndef __VOICE_NOISE_REDUCTION_H__
#define __VOICE_NOISE_REDUCTION_H__

#include "common_def.h"

/**
 * @brief 语音降噪模块初始化
 * @details 初始化Amlogic语音处理SDK，配置降噪参数
 * @return SUCCESS/FAILURE
 */
int voice_noise_reduction_init(void);

/**
 * @brief 语音降噪模块反初始化
 * @details 反初始化Amlogic语音处理SDK，释放资源
 */
void voice_noise_reduction_deinit(void);

/**
 * @brief 启用/禁用语音降噪
 * @param enable true-启用，false-禁用
 * @return SUCCESS/FAILURE
 */
int voice_noise_reduction_set_enable(bool enable);

/**
 * @brief 设置降噪等级
 * @param level 降噪等级 1-5（1:弱，5:强）
 * @return SUCCESS/FAILURE
 */
int voice_noise_reduction_set_level(int level);

/**
 * @brief 启用/禁用回声消除
 * @param enable true-启用，false-禁用
 * @return SUCCESS/FAILURE
 */
int voice_noise_reduction_set_echo_cancellation(bool enable);

/**
 * @brief 启用/禁用语音增强
 * @param enable true-启用，false-禁用
 * @return SUCCESS/FAILURE
 */
int voice_noise_reduction_set_voice_enhancement(bool enable);

/**
 * @brief 处理语音数据
 * @param in_buf 输入语音数据缓冲区
 * @param out_buf 输出语音数据缓冲区
 * @param buf_len 数据长度
 * @return 处理成功的数据长度，失败返回-1
 */
int voice_noise_reduction_process(uint8_t *in_buf, uint8_t *out_buf, int buf_len);

#endif // __VOICE_NOISE_REDUCTION_H__
