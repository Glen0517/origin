#include "common_def.h"

#ifndef __SYSTEM_H__
#define __SYSTEM_H__

/******************************************************************************************
 * 系统配置结构体
 ******************************************************************************************/
typedef struct {
    bool ota_en;              // 是否开启OTA升级
    bool auto_restart_en;     // 是否开启自动重启
    bool error_report_en;     // 是否开启错误报告
    int log_level;            // 日志级别
} SystemCfg_t;

/******************************************************************************************
 * 对外暴露接口 - 系统管理所有功能
 ******************************************************************************************/
/**
 * @brief  系统模块初始化
 * @return SUCCESS/FAILURE
 */
int system_init(void);

/**
 * @brief  系统模块反初始化
 * @return SUCCESS/FAILURE
 */
void system_deinit(void);

/**
 * @brief  设置系统状态
 * @param  state 系统状态
 * @return SUCCESS/FAILURE
 */
int system_set_state(SysState_e state);

/**
 * @brief  获取系统状态
 * @return 系统状态
 */
SysState_e system_get_state(void);

/**
 * @brief  系统事件轮询
 * @return 无
 */
void system_event_poll(void);

#endif // __SYSTEM_H__