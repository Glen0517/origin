#ifndef __SOUND_EFFECTS_PRIV_H__
#define __SOUND_EFFECTS_PRIV_H__

#include "sound_effects.h"
#include "product_type.h"

// 音效内部配置结构体
typedef struct {
    int init_ok;              // 初始化状态
    bool dolby_en;            // 杜比使能状态
    bool virtual_5_1_en;       // 虚拟5.1使能状态
    int eq_gain[9];           // EQ增益值
    int freq_band[9];          // 频率带宽
} SoundEffectCfg_t;

// 内部私有函数声明
int dolby_dts_init(void);
void dolby_dts_deinit(void);
int eq_preset_init(void);
void eq_preset_deinit(void);

#endif // __SOUND_EFFECTS_PRIV_H__