/**
 * @file hal.h
 * @brief HAL层主头文件
 * @details 硬件抽象层的主头文件，定义HAL层的基本结构和公共接口
 * @author AML Audio Team
 * @date 2026-01-16
 */

#ifndef HAL_H
#define HAL_H

#include "common_def.h"

/**
 * @brief HAL层初始化
 * @details 初始化所有HAL层模块
 * @return 初始化结果：0表示成功，非0表示失败
 */
int hal_init(void);

/**
 * @brief HAL层反初始化
 * @details 反初始化所有HAL层模块
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int hal_deinit(void);

// 包含各个子模块的头文件
#include "hal_audio.h"
#include "hal_bt.h"
#include "hal_usb.h"
#include "hal_peri.h"

#endif /* HAL_H */