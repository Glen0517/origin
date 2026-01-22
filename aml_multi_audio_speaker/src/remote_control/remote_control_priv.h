/**
 * @file remote_control_priv.h
 * @brief 远程控制模块私有头文件
 * @details 定义远程控制模块的内部数据结构和函数
 * @author AML Audio Team
 * @date 2026-01-22
 */

#ifndef REMOTE_CONTROL_PRIV_H
#define REMOTE_CONTROL_PRIV_H

#include "remote_control.h"
#include "peripheral_priv.h"
#include "logger.h"
#include "product_type.h"
#include "common_def.h"

#include "comm_mcu.h"        // MCU通信接口

/******************************************************************************************
 * 远程控制模块内部数据结构
 ******************************************************************************************/

/**
 * @brief 红外码结构体
 */
typedef struct {
    uint32_t ir_code;         // 红外码值
    KeyEvent_e key_event;     // 对应的按键事件
    char remote_name[64];     // 遥控器名称
    char key_name[32];        // 按键名称
} IrCode_t;

/******************************************************************************************
 * 远程控制模块全局变量
 ******************************************************************************************/

extern bool g_remote_control_init;
extern bool g_ir_enabled;
extern bool g_ir_learning;
extern KeyEvent_e g_last_key_event;
extern IrCode_t g_ir_codes[MAX_IR_CODES];
extern int g_ir_code_count;

/******************************************************************************************
 * 远程控制模块内部函数
 ******************************************************************************************/

/**
 * @brief 初始化远程控制模块
 * @return 初始化结果：0表示成功，非0表示失败
 */
int remote_control_init(void);

/**
 * @brief 反初始化远程控制模块
 */
void remote_control_deinit(void);

/**
 * @brief 处理按键事件
 * @param event 按键事件
 * @return 处理结果：0表示成功，非0表示失败
 */
int remote_control_process_key_event(KeyEvent_e event);

/**
 * @brief 开始红外学习
 * @return 操作结果：0表示成功，非0表示失败
 */
int remote_control_ir_learn_start(void);

/**
 * @brief 停止红外学习
 * @return 操作结果：0表示成功，非0表示失败
 */
int remote_control_ir_learn_stop(void);

/**
 * @brief 设置红外码
 * @param ir_code 红外码值
 * @return 操作结果：0表示成功，非0表示失败
 */
int remote_control_set_ir_code(uint32_t ir_code);

/**
 * @brief 保存红外码配置
 * @return 操作结果：0表示成功，非0表示失败
 */
int remote_control_save_ir_config(void);

/**
 * @brief 加载红外码配置
 * @return 操作结果：0表示成功，非0表示失败
 */
int remote_control_load_ir_config(void);

#endif /* REMOTE_CONTROL_PRIV_H */
