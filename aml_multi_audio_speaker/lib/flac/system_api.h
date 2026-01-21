#include "common_def.h"

#ifndef __SYSTEM_API_H__
#define __SYSTEM_API_H__

/******************************************************************************************
 * 系统API接口 - 提供系统核心功能
 ******************************************************************************************/
/**
 * @brief  系统API初始化
 * @return SUCCESS/FAILURE
 */
int system_api_init(void);

/**
 * @brief  系统API反初始化
 * @return SUCCESS/FAILURE
 */
int system_api_deinit(void);

/**
 * @brief  获取系统状态
 * @return 系统状态
 */
SysState_e system_api_get_state(void);

/**
 * @brief  设置系统状态
 * @param  state 系统状态
 * @return SUCCESS/FAILURE
 */
int system_api_set_state(SysState_e state);

/**
 * @brief  OTA升级
 * @param  file_path 升级文件路径
 * @return SUCCESS/FAILURE
 */
int system_api_ota_upgrade(const char *file_path);

/**
 * @brief  恢复出厂设置
 * @return SUCCESS/FAILURE
 */
int system_api_factory_reset(void);

/**
 * @brief  系统重启
 * @return SUCCESS/FAILURE
 */
int system_api_restart(void);

#endif // __SYSTEM_API_H__
