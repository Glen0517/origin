/**
 * @file main_loop.h
 * @brief 主循环模块头文件
 * @details 提供业务主循环和事件处理的功能
 * @author AML Audio Team
 * @date 2026-01-28
 */

#ifndef __MAIN_LOOP_H__
#define __MAIN_LOOP_H__

#include "common_def.h"
#include "logger.h"

// 按键事件定义
typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PLAY_PAUSE = 1,
    KEY_EVENT_VOL_UP = 2,
    KEY_EVENT_VOL_DOWN = 3,
    KEY_EVENT_SOURCE_SWITCH = 4,
    KEY_EVENT_SOUND_MODE = 5,
    KEY_EVENT_BASS_UP = 6,
    KEY_EVENT_TREBLE_UP = 7,
    KEY_EVENT_IR_LEARN = 8,
    KEY_EVENT_NEXT = 9,
    KEY_EVENT_PREV = 10
} KeyEvent_e;

/**
 * @brief 信号处理函数
 * @details 处理系统信号，实现优雅退出
 * @param sig 接收到的信号类型
 * @return 无
 */
void sig_handler(int sig);

/**
 * @brief 业务主循环
 * @details 使用高精度时间函数定期轮询各个模块，处理系统的核心业务逻辑
 * @return 无
 */
void main_business_loop(void);

#endif // __MAIN_LOOP_H__
