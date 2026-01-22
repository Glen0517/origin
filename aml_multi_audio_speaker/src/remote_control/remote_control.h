/**
 * @file remote_control.h
 * @brief 远程控制模块公共头文件
 * @details 定义远程控制模块的对外接口和常量
 * @author AML Audio Team
 * @date 2026-01-22
 */

#ifndef REMOTE_CONTROL_H
#define REMOTE_CONTROL_H

#include "common_def.h"
#include "product_type.h"

/******************************************************************************************
 * 远程控制模块常量定义
 ******************************************************************************************/

// 最大红外码数量
#define MAX_IR_CODES 128

// 红外学习超时时间（毫秒）
#define IR_LEARN_TIMEOUT 30000

// 红外码文件路径
#define IR_CODE_CONFIG_FILE "/etc/ir_codes.conf"

/******************************************************************************************
 * 远程控制模块枚举定义
 ******************************************************************************************/

// 按键事件枚举（从peripheral.h导入）
// 注意：这里需要确保与peripheral.h中的定义一致

/******************************************************************************************
 * 远程控制模块对外接口
 ******************************************************************************************/

/**
 * @brief 初始化远程控制管理进程
 * @return 初始化结果：0表示成功，非0表示失败
 */
int remote_control_process_init(void);

/**
 * @brief 反初始化远程控制管理进程
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int remote_control_process_deinit(void);

/**
 * @brief 处理按键事件
 * @param event 按键事件
 * @return 处理结果：0表示成功，非0表示失败
 */
int remote_control_process_handle_key_event(KeyEvent_e event);

/**
 * @brief 开始红外学习
 * @return 操作结果：0表示成功，非0表示失败
 */
int remote_control_process_start_ir_learn(void);

/**
 * @brief 停止红外学习
 * @return 操作结果：0表示成功，非0表示失败
 */
int remote_control_process_stop_ir_learn(void);

/**
 * @brief 获取远程控制管理进程ID
 * @return 进程ID，失败返回-1
 */
pid_t remote_control_process_get_pid(void);

/**
 * @brief 检查远程控制管理进程是否运行
 * @return true表示运行，false表示未运行
 */
bool remote_control_process_is_running(void);

#endif /* REMOTE_CONTROL_H */
