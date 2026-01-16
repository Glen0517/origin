/**
 * @file pal_system.h
 * @brief 系统服务抽象接口
 * @details 定义系统服务抽象层的接口函数，封装系统相关的服务和功能
 * @author AML Audio Team
 * @date 2026-01-16
 */

#ifndef PAL_SYSTEM_H
#define PAL_SYSTEM_H

#include "common_def.h"

/**
 * @brief 系统时间结构体
 */
typedef struct {
    int year;    // 年份
    int month;   // 月份（1-12）
    int day;     // 日期（1-31）
    int hour;    // 小时（0-23）
    int minute;  // 分钟（0-59）
    int second;  // 秒（0-59）
} PalSystemTime_t;

/**
 * @brief 初始化系统服务
 * @details 初始化系统服务模块
 * @return 初始化结果：0表示成功，非0表示失败
 */
int pal_system_init(void);

/**
 * @brief 反初始化系统服务
 * @details 反初始化系统服务模块
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int pal_system_deinit(void);

/**
 * @brief 获取系统时间
 * @details 获取当前系统的日期和时间
 * @param time 时间结构体指针，用于存储获取的时间
 * @return 获取结果：0表示成功，非0表示失败
 */
int pal_system_get_time(PalSystemTime_t *time);

/**
 * @brief 设置系统时间
 * @details 设置系统的日期和时间
 * @param time 时间结构体指针，指向要设置的时间
 * @return 设置结果：0表示成功，非0表示失败
 */
int pal_system_set_time(PalSystemTime_t *time);

/**
 * @brief 系统睡眠
 * @details 使系统睡眠指定的时间
 * @param ms 睡眠时长，单位为毫秒
 * @return 睡眠结果：0表示成功，非0表示失败
 */
int pal_system_sleep(int ms);

/**
 * @brief 获取CPU使用率
 * @details 获取当前系统的CPU使用率
 * @param usage 使用率指针，用于存储获取的使用率（0-100）
 * @return 获取结果：0表示成功，非0表示失败
 */
int pal_system_get_cpu_usage(int *usage);

/**
 * @brief 获取内存使用率
 * @details 获取当前系统的内存使用率
 * @param usage 使用率指针，用于存储获取的使用率（0-100）
 * @return 获取结果：0表示成功，非0表示失败
 */
int pal_system_get_memory_usage(int *usage);

/**
 * @brief 重启系统
 * @details 重启当前系统
 * @return 重启结果：0表示成功，非0表示失败
 */
int pal_system_reboot(void);

/**
 * @brief 关闭系统
 * @details 关闭当前系统
 * @return 关闭结果：0表示成功，非0表示失败
 */
int pal_system_poweroff(void);

#endif /* PAL_SYSTEM_H */