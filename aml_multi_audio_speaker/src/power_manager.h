/**
 * @file power_manager.h
 * @brief 功耗管理模块头文件
 * @details 提供功耗模式管理、系统状态管理等功能
 * @author AML Audio Team
 * @date 2026-01-28
 */

#ifndef __POWER_MANAGER_H__
#define __POWER_MANAGER_H__

#include "common_def.h"
#include "logger.h"

// 功耗模式定义
typedef enum {
    POWER_MODE_NORMAL = 0,
    POWER_MODE_LOW = 1,
    POWER_MODE_STANDBY = 2,
    POWER_MODE_MAX
} PowerMode_e;

// 系统状态定义
typedef enum {
    SYSTEM_STATE_ACTIVE = 0,
    SYSTEM_STATE_IDLE = 1,
    SYSTEM_STATE_SUSPENDED = 2,
    SYSTEM_STATE_MAX
} SystemState_e;

// 功耗统计结构体
typedef struct {
    uint64_t total_power_consumption;
    uint32_t mode_duration[POWER_MODE_MAX];
    uint32_t state_duration[SYSTEM_STATE_MAX];
    uint32_t wakeup_count;
    uint32_t last_wakeup_time;
    uint32_t idle_time;
} PowerStats_t;

// 功耗管理器结构体
typedef struct {
    PowerMode_e current_mode;
    SystemState_e current_state;
    PowerStats_t power_stats;
    bool dynamic_power_management;
    bool low_power_mode_enabled;
    uint32_t idle_threshold;
    uint32_t low_power_threshold;
    uint32_t normal_poll_interval;
    uint32_t low_power_poll_interval;
    uint32_t last_activity_time;
} PowerManager_t;

/**
 * @brief 初始化功耗管理器
 * @details 创建并初始化功耗管理器，用于动态功耗管理
 * @return 功耗管理器指针，失败返回NULL
 */
PowerManager_t *power_manager_init(void);

/**
 * @brief 更新系统活动时间
 * @details 更新系统最后活动时间，用于判断系统是否空闲
 * @param manager 功耗管理器指针
 */
void power_manager_update_activity(PowerManager_t *manager);

/**
 * @brief 检查系统空闲状态
 * @details 检查系统是否处于空闲状态，并根据空闲时间调整功耗模式
 * @param manager 功耗管理器指针
 * @return 当前系统状态
 */
SystemState_e power_manager_check_idle(PowerManager_t *manager);

/**
 * @brief 获取当前轮询间隔
 * @details 根据当前功耗模式获取合适的轮询间隔
 * @param manager 功耗管理器指针
 * @return 轮询间隔（毫秒）
 */
uint32_t power_manager_get_poll_interval(PowerManager_t *manager);

/**
 * @brief 进入低功耗模式
 * @details 进入低功耗模式，降低系统功耗
 * @param manager 功耗管理器指针
 * @return 是否成功进入低功耗模式
 */
bool power_manager_enter_low_power(PowerManager_t *manager);

/**
 * @brief 退出低功耗模式
 * @details 退出低功耗模式，恢复正常运行状态
 * @param manager 功耗管理器指针
 * @return 是否成功退出低功耗模式
 */
bool power_manager_exit_low_power(PowerManager_t *manager);

/**
 * @brief 输出功耗统计信息
 * @details 输出功耗统计和系统状态信息
 * @param manager 功耗管理器指针
 */
void power_manager_print_stats(PowerManager_t *manager);

/**
 * @brief 反初始化功耗管理器
 * @details 反初始化功耗管理器，释放资源
 * @param manager 功耗管理器指针
 */
void power_manager_deinit(PowerManager_t *manager);

#endif // __POWER_MANAGER_H__
