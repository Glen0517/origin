/**
 * @file module_init.h
 * @brief 模块初始化模块头文件
 * @details 提供模块初始化和反初始化的功能
 * @author AML Audio Team
 * @date 2026-01-28
 */

#ifndef __MODULE_INIT_H__
#define __MODULE_INIT_H__

#include "common_def.h"
#include "logger.h"

// 模块状态枚举
typedef enum {
    MODULE_STATE_UNINIT = 0,
    MODULE_STATE_INIT_SUCCESS,
    MODULE_STATE_INIT_FAILED
} ModuleState_e;

// 配置结构体定义
typedef struct {
    int sample_rate;
    int channel_num;
    int pcm_buffer_size;
    bool hw_decode_en;
    bool dolby_dts_en;
    int init_ok;
} AudioCoreConfig_t;

typedef struct {
    bool cec_en;
    bool auto_switch_en;
    int sample_rate;
} HdmiArcConfig_t;

typedef struct {
    bool auto_switch_en;
    int sample_rate;
    int bits_per_sample;
} SpdifConfig_t;

typedef struct {
    int key_debounce_ms;
    int long_press_ms;
    bool ir_learn_en;
    bool mic_mute_en;
} PeripheralConfig_t;

/**
 * @brief 模块初始化总入口
 * @details 根据宏控配置初始化所有启用的模块，适配不同产品类型
 * @return 初始化结果，0表示成功，非0表示失败
 */
int module_init_all(void);

/**
 * @brief 模块反初始化总入口
 * @details 按照与初始化相反的顺序反初始化所有模块，确保资源正确释放
 * @return 无
 */
void module_deinit_all(void);

#endif // __MODULE_INIT_H__
