/**
 * @file process_manager.h
 * @brief 进程管理器头文件
 * @details 提供进程管理器的接口定义
 * @author AML Audio Team
 * @date 2026-01-22
 */

#ifndef __PROCESS_MANAGER_H__
#define __PROCESS_MANAGER_H__

#include "common_def.h"
#include "process.h"

/******************************************************************************************
 * 进程管理器接口定义
 ******************************************************************************************/

/**
 * @brief 系统进程类型
 */
typedef enum {
    SYS_PROCESS_AUDIO = 0,       // 音频处理进程
    SYS_PROCESS_NETWORK = 1,      // 网络服务进程
    SYS_PROCESS_STORAGE = 2,      // 存储管理进程
    SYS_PROCESS_SYSTEM = 3,       // 系统管理进程
    SYS_PROCESS_MAX
} SysProcessType_t;

/**
 * @brief 进程管理器初始化
 * @return SUCCESS/FAILURE
 */
int process_manager_init(void);

/**
 * @brief 进程管理器反初始化
 * @return SUCCESS/FAILURE
 */
int process_manager_deinit(void);

/**
 * @brief 获取系统进程ID
 * @param type 进程类型
 * @return 进程ID，失败返回-1
 */
pid_t process_manager_get_process_id(SysProcessType_t type);

/**
 * @brief 重启系统进程
 * @param type 进程类型
 * @return SUCCESS/FAILURE
 */
int process_manager_restart_process(SysProcessType_t type);

/**
 * @brief 监控系统进程状态
 * @return SUCCESS/FAILURE
 */
int process_manager_monitor(void);

/**
 * @brief 获取系统进程状态
 * @param type 进程类型
 * @param running 运行状态
 * @return SUCCESS/FAILURE
 */
int process_manager_get_process_status(SysProcessType_t type, bool *running);

/**
 * @brief 获取系统进程信息
 * @param type 进程类型
 * @param info 进程信息
 * @return SUCCESS/FAILURE
 */
int process_manager_get_process_info(SysProcessType_t type, ProcessInfo_t *info);

/**
 * @brief 获取所有系统进程信息
 * @param infos 进程信息数组
 * @param max_count 最大进程数
 * @param count 实际进程数
 * @return SUCCESS/FAILURE
 */
int process_manager_get_all_process_info(ProcessInfo_t *infos, int max_count, int *count);

/**
 * @brief 发送消息到系统进程
 * @param type 进程类型
 * @param msg 消息内容
 * @param msg_len 消息长度
 * @return SUCCESS/FAILURE
 */
int process_manager_send_message(SysProcessType_t type, const void *msg, int msg_len);

/**
 * @brief 接收来自系统进程的消息
 * @param type 进程类型
 * @param msg 消息缓冲区
 * @param msg_len 消息长度
 * @return 实际读取的消息长度，失败返回-1
 */
int process_manager_receive_message(SysProcessType_t type, void *msg, int msg_len);

#endif /* __PROCESS_MANAGER_H__ */
