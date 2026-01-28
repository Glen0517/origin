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
    POWER_MODE_NORMAL = 0,    // 正常模式
    POWER_MODE_LOW = 1,       // 低功耗模式
    POWER_MODE_STANDBY = 2,    // 待机模式
    POWER_MODE_DEEP_SLEEP = 3, // 深度睡眠模式
    POWER_MODE_MAX
} PowerMode_e;

// 系统状态定义
typedef enum {
    SYSTEM_STATE_ACTIVE = 0,     // 活跃状态
    SYSTEM_STATE_IDLE = 1,       // 空闲状态
    SYSTEM_STATE_SUSPENDED = 2,   // 挂起状态
    SYSTEM_STATE_DEEP_SLEEP = 3,  // 深度睡眠状态
    SYSTEM_STATE_MAX
} SystemState_e;

// 唤醒源类型
typedef enum {
    WAKEUP_SOURCE_KEY = 0,        // 按键唤醒
    WAKEUP_SOURCE_BLUETOOTH = 1,   // 蓝牙唤醒
    WAKEUP_SOURCE_WIFI = 2,        // WiFi唤醒
    WAKEUP_SOURCE_AUDIO = 3,       // 音频唤醒
    WAKEUP_SOURCE_TIMER = 4,       // 定时器唤醒
    WAKEUP_SOURCE_MAX
} WakeupSource_e;

// 功耗统计结构体
typedef struct {
    uint64_t total_power_consumption;
    uint32_t mode_duration[POWER_MODE_MAX];
    uint32_t state_duration[SYSTEM_STATE_MAX];
    uint32_t wakeup_count;
    uint32_t last_wakeup_time;
    uint32_t idle_time;
    uint32_t wakeup_source_count[WAKEUP_SOURCE_MAX];
    uint32_t last_wakeup_source;
} PowerStats_t;

// 唤醒源配置结构体
typedef struct {
    bool enabled[WAKEUP_SOURCE_MAX];
    uint32_t sensitivity[WAKEUP_SOURCE_MAX];
    uint32_t debounce_time[WAKEUP_SOURCE_MAX];
} WakeupConfig_t;

// 功耗管理器结构体
typedef struct {
    PowerMode_e current_mode;
    SystemState_e current_state;
    PowerStats_t power_stats;
    WakeupConfig_t wakeup_config;
    bool dynamic_power_management;
    bool low_power_mode_enabled;
    bool deep_sleep_enabled;
    bool standby_enabled;
    uint32_t idle_threshold;
    uint32_t low_power_threshold;
    uint32_t standby_threshold;
    uint32_t deep_sleep_threshold;
    uint32_t normal_poll_interval;
    uint32_t low_power_poll_interval;
    uint32_t standby_poll_interval;
    uint32_t deep_sleep_poll_interval;
    uint32_t last_activity_time;
    uint32_t last_mode_change_time;
    uint32_t last_state_change_time;
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
 * @brief 进入待机模式
 * @details 进入待机模式，进一步降低系统功耗
 * @param manager 功耗管理器指针
 * @return 是否成功进入待机模式
 */
bool power_manager_enter_standby(PowerManager_t *manager);

/**
 * @brief 退出待机模式
 * @details 退出待机模式，恢复正常运行状态
 * @param manager 功耗管理器指针
 * @return 是否成功退出待机模式
 */
bool power_manager_exit_standby(PowerManager_t *manager);

/**
 * @brief 进入深度睡眠模式
 * @details 进入深度睡眠模式，最大程度降低系统功耗
 * @param manager 功耗管理器指针
 * @return 是否成功进入深度睡眠模式
 */
bool power_manager_enter_deep_sleep(PowerManager_t *manager);

/**
 * @brief 退出深度睡眠模式
 * @details 退出深度睡眠模式，恢复正常运行状态
 * @param manager 功耗管理器指针
 * @return 是否成功退出深度睡眠模式
 */
bool power_manager_exit_deep_sleep(PowerManager_t *manager);

/**
 * @brief 配置唤醒源
 * @details 配置唤醒源的启用状态、灵敏度和去抖时间
 * @param manager 功耗管理器指针
 * @param source 唤醒源类型
 * @param enabled 是否启用
 * @param sensitivity 灵敏度（0-100）
 * @param debounce_time 去抖时间（毫秒）
 * @return 是否配置成功
 */
bool power_manager_config_wakeup_source(PowerManager_t *manager, WakeupSource_e source, bool enabled, uint32_t sensitivity, uint32_t debounce_time);

/**
 * @brief 处理唤醒事件
 * @details 处理系统唤醒事件，更新唤醒统计信息
 * @param manager 功耗管理器指针
 * @param source 唤醒源类型
 * @return 是否处理成功
 */
bool power_manager_handle_wakeup(PowerManager_t *manager, WakeupSource_e source);

/**
 * @brief 设置功耗模式
 * @details 手动设置系统功耗模式
 * @param manager 功耗管理器指针
 * @param mode 功耗模式
 * @return 是否设置成功
 */
bool power_manager_set_mode(PowerManager_t *manager, PowerMode_e mode);

/**
 * @brief 获取当前功耗模式
 * @details 获取当前系统功耗模式
 * @param manager 功耗管理器指针
 * @return 当前功耗模式
 */
PowerMode_e power_manager_get_mode(PowerManager_t *manager);

/**
 * @brief 获取当前系统状态
 * @details 获取当前系统状态
 * @param manager 功耗管理器指针
 * @return 当前系统状态
 */
SystemState_e power_manager_get_state(PowerManager_t *manager);

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
