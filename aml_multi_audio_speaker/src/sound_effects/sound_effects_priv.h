#ifndef __SOUND_EFFECTS_PRIV_H__
#define __SOUND_EFFECTS_PRIV_H__

#include "sound_effects.h"
#include "product_type.h"

// 内部私有函数声明
int dolby_dts_init(void);
void dolby_dts_deinit(void);
int eq_preset_init(void);
void eq_preset_deinit(void);

#endif // __SOUND_EFFECTS_PRIV_H__