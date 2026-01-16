#ifndef __PERIPHERAL_PRIV_H__
#define __PERIPHERAL_PRIV_H__

#include "peripheral.h"
#include "product_type.h"

// 外部变量声明
extern bool g_key_ir_init;
extern KeyEvent_e g_last_key_event;

// 内部私有函数声明 - 单一职责拆分
int key_ir_init(void);
void key_ir_deinit(void);
void key_ir_event_poll(void);
KeyEvent_e key_ir_get_event(void);
int key_ir_ir_learn_start(void);
int key_ir_ir_learn_stop(void);

int led_ctrl_init(void);
void led_ctrl_deinit(void);
void led_ctrl_event_poll(void);
int led_ctrl_set_state(int led_idx, LedState_e state);

int lcd_display_init(void);
void lcd_display_deinit(void);
void lcd_display_event_poll(void);

int prompt_sound_init(void);
void prompt_sound_deinit(void);
void prompt_sound_play(int sound_id);

#endif // __PERIPHERAL_PRIV_H__