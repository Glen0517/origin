#ifndef __PERIPHERAL_PRIV_H__
#define __PERIPHERAL_PRIV_H__

#include "peripheral.h"
#include "product_type.h"

// 内部私有函数声明 - 单一职责拆分
int key_ir_init(void);
void key_ir_deinit(void);
int led_ctrl_init(void);
void led_ctrl_deinit(void);
int lcd_display_init(void);
void lcd_display_deinit(void);
int prompt_sound_init(void);
void prompt_sound_deinit(void);

#endif // __PERIPHERAL_PRIV_H__