#ifndef __PLAY_CTRL_PRIV_H__
#define __PLAY_CTRL_PRIV_H__

#include "play_ctrl.h"
#include "product_type.h"

int play_state_init(void);
void play_state_deinit(void);
int sound_field_init(void);
void sound_field_deinit(void);
int sound_field_set_mode(SoundMode_e mode);
SoundMode_e sound_field_get_mode(void);
int eq_effect_init(void);
void eq_effect_deinit(void);
int eq_effect_set_gain(int *gain, int len);

#endif // __PLAY_CTRL_PRIV_H__