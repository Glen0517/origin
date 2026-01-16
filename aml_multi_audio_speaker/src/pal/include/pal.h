/**
 * @file pal.h
 * @brief PAL层主头文件
 * @details 平台抽象层的主头文件，定义PAL层的基本结构和公共接口
 * @author AML Audio Team
 * @date 2026-01-16
 */

#ifndef PAL_H
#define PAL_H

#include "common_def.h"

/**
 * @brief PAL层初始化
 * @details 初始化所有PAL层模块
 * @return 初始化结果：0表示成功，非0表示失败
 */
int pal_init(void);

/**
 * @brief PAL层反初始化
 * @details 反初始化所有PAL层模块
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int pal_deinit(void);

// 包含各个子模块的头文件
#include "pal_system.h"
#include "pal_storage.h"
#include "pal_network.h"

#endif /* PAL_H */