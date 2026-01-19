#ifndef __SPDIF_OPTICAL_H__
#define __SPDIF_OPTICAL_H__

#include "common_def.h"
#include "product_type.h"

// 仅高/中端编译
#if CONFIG_ENABLE_SPDIF

/**
 * @brief SPDIF配置结构体
 */
typedef struct {
    bool auto_switch_en;  // 自动切换使能
    int sample_rate;      // 采样率
    int bits_per_sample;  // 位深度
} SpdifConfig_t;

/**
 * @brief SPDIF音频格式枚举
 */
typedef enum {
    SPDIF_FORMAT_PCM = 0,
    SPDIF_FORMAT_DOLBY,
    SPDIF_FORMAT_DTS,
    SPDIF_FORMAT_AC3
} SpdifFormat_e;

/**
 * @brief SPDIF初始化
 * @param cfg SPDIF配置结构体指针
 * @return SUCCESS/FAILURE
 */
int spdif_optical_init(SpdifConfig_t *cfg);

/**
 * @brief SPDIF反初始化
 * @return SUCCESS/FAILURE
 */
int spdif_optical_deinit(void);

/**
 * @brief 检测SPDIF信号
 * @return 信号存在返回true，否则返回false
 */
bool spdif_optical_detect_signal(void);

/**
 * @brief 设置SPDIF音频格式
 * @param format 音频格式 (SpdifFormat_e)
 * @return SUCCESS/FAILURE
 */
int spdif_optical_set_format(SpdifFormat_e format);

/**
 * @brief 获取当前SPDIF状态
 * @return SPDIF状态，信号存在返回true，否则返回false
 */
bool spdif_optical_get_status(void);

/**
 * @brief 轮询处理SPDIF事件
 */
void spdif_optical_event_poll(void);

#endif

#endif // __SPDIF_OPTICAL_H__
