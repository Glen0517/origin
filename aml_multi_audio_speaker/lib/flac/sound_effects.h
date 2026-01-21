#ifndef __SOUND_EFFECTS_H__
#define __SOUND_EFFECTS_H__

#include "common_def.h"

//音效处理模块，中低端屏蔽
#if CONFIG_ENABLE_DOLBY_DTS || CONFIG_ENABLE_VIRTUAL_5_1

typedef struct {
    int eq_gain[9];
    int freq_band[9];
    bool dolby_en;
    bool virtual_5_1_en;
} SoundEffectsConfig_t;

int sound_effects_init(SoundEffectsConfig_t *cfg);
int sound_effects_deinit(void);
int sound_effects_set_eq(int *gain, int len);
int sound_effects_set_dolby(bool en);
int sound_effects_set_virtual_5_1(bool en);

#endif

#endif // __SOUND_EFFECTS_H__