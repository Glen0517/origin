#ifndef __PERIPHERAL_H__
#define __PERIPHERAL_H__

#include "common_def.h"

/******************************************************************************************
 * 按键事件枚举
 ******************************************************************************************/
typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PLAY_PAUSE,
    KEY_EVENT_VOL_UP,
    KEY_EVENT_VOL_DOWN,
    KEY_EVENT_SOURCE_SWITCH,
    KEY_EVENT_SOUND_MODE,
    KEY_EVENT_BASS_UP,
    KEY_EVENT_TREBLE_UP,
    KEY_EVENT_IR_LEARN,
    KEY_EVENT_BT_PAIR, // 低音炮专属配对键
    KEY_EVENT_NEXT,    // 下一曲
    KEY_EVENT_PREV     // 上一曲
} KeyEvent_e;

/******************************************************************************************
 * 外设配置结构体
 ******************************************************************************************/
typedef struct {
    int key_debounce_ms;    // 按键防抖时间
    int long_press_ms;      // 长按判定时间
    bool ir_learn_en;       // 是否开启红外学习
    bool mic_mute_en;       // 是否开启麦克风静音
} PeripheralConfig_t;

/******************************************************************************************
 * 对外暴露接口 - 外设管理所有功能，宏控裁剪红外/麦克风
 ******************************************************************************************/
/**
 * @brief  外设模块初始化
 * @param  cfg 外设配置结构体指针
 * @return SUCCESS/FAILURE
 */
int peripheral_init(PeripheralConfig_t *cfg);

/**
 * @brief  外设模块反初始化
 * @return SUCCESS/FAILURE
 */
int peripheral_deinit(void);

/**
 * @brief  设置LED状态
 * @param  led_idx LED索引
 * @param  state LED状态
 * @return SUCCESS/FAILURE
 */
int peripheral_set_led(int led_idx, LedState_e state);

/**
 * @brief  获取按键事件
 * @return 按键事件
 */
KeyEvent_e peripheral_get_key_event(void);

/**
 * @brief  外设事件轮询
 * @return 无
 */
void peripheral_event_poll(void);

#if CONFIG_ENABLE_IR_LEARN
/**
 * @brief  红外学习开始
 * @return SUCCESS/FAILURE
 */
int peripheral_ir_learn_start(void);

/**
 * @brief  红外学习停止
 * @return SUCCESS/FAILURE
 */
int peripheral_ir_learn_stop(void);
#endif

#endif // __PERIPHERAL_H__