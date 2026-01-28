/**
 * @file power_manager.c
 * @brief 功耗管理模块实现
 * @details 提供功耗模式管理、系统状态管理等功能
 * @author AML Audio Team
 * @date 2026-01-28
 */

#include "power_manager.h"

// 外部函数声明
void bluetooth_set_power(bool power);

#ifdef CONFIG_ENABLE_WIFI_MEDIA
void wifi_media_set_power(bool power);
#endif

/**
 * @brief 初始化功耗管理器
 * @details 创建并初始化功耗管理器，用于动态功耗管理
 * @return 功耗管理器指针，失败返回NULL
 */
PowerManager_t *power_manager_init(void) {
    PowerManager_t *manager = (PowerManager_t *)malloc(sizeof(PowerManager_t));
    if (!manager) {
        LOG_ERROR("Failed to allocate power manager");
        return NULL;
    }
    
    memset(manager, 0, sizeof(PowerManager_t));
    
    // 初始化默认值
    manager->current_mode = POWER_MODE_NORMAL;
    manager->current_state = SYSTEM_STATE_ACTIVE;
    manager->dynamic_power_management = true;
    manager->low_power_mode_enabled = true;
    manager->idle_threshold = 60000; // 60秒
    manager->low_power_threshold = 300000; // 5分钟
    manager->normal_poll_interval = 10; // 10毫秒
    manager->low_power_poll_interval = 100; // 100毫秒
    manager->last_activity_time = time(NULL) * 1000;
    
    LOG_INFO("Power manager initialized");
    return manager;
}

/**
 * @brief 更新系统活动时间
 * @details 更新系统最后活动时间，用于判断系统是否空闲
 * @param manager 功耗管理器指针
 */
void power_manager_update_activity(PowerManager_t *manager) {
    if (!manager) {
        return;
    }
    
    manager->last_activity_time = time(NULL) * 1000;
    manager->power_stats.idle_time = 0;
    
    // 如果系统处于空闲状态，切换到活跃状态
    if (manager->current_state == SYSTEM_STATE_IDLE) {
        manager->current_state = SYSTEM_STATE_ACTIVE;
        LOG_INFO("System state changed to ACTIVE");
    }
    
    // 如果系统处于低功耗模式，切换到正常模式
    if (manager->current_mode == POWER_MODE_LOW) {
        manager->current_mode = POWER_MODE_NORMAL;
        LOG_INFO("Power mode changed to NORMAL");
    }
}

/**
 * @brief 检查系统空闲状态
 * @details 检查系统是否处于空闲状态，并根据空闲时间调整功耗模式
 * @param manager 功耗管理器指针
 * @return 当前系统状态
 */
SystemState_e power_manager_check_idle(PowerManager_t *manager) {
    if (!manager || !manager->dynamic_power_management) {
        return manager ? manager->current_state : SYSTEM_STATE_ACTIVE;
    }
    
    uint32_t current_time = time(NULL) * 1000;
    uint32_t idle_time = current_time - manager->last_activity_time;
    manager->power_stats.idle_time = idle_time;
    
    // 根据空闲时间调整系统状态和功耗模式
    if (idle_time >= manager->low_power_threshold) {
        // 长时间空闲，进入低功耗模式
        if (manager->current_mode != POWER_MODE_LOW) {
            manager->current_mode = POWER_MODE_LOW;
            manager->current_state = SYSTEM_STATE_IDLE;
            LOG_INFO("System entering LOW power mode (idle for %d ms)", idle_time);
        }
    } else if (idle_time >= manager->idle_threshold) {
        // 短时间空闲，进入空闲状态
        if (manager->current_state != SYSTEM_STATE_IDLE) {
            manager->current_state = SYSTEM_STATE_IDLE;
            LOG_INFO("System state changed to IDLE (idle for %d ms)", idle_time);
        }
    } else {
        // 系统活跃
        if (manager->current_state != SYSTEM_STATE_ACTIVE) {
            manager->current_state = SYSTEM_STATE_ACTIVE;
            LOG_INFO("System state changed to ACTIVE");
        }
        if (manager->current_mode != POWER_MODE_NORMAL) {
            manager->current_mode = POWER_MODE_NORMAL;
            LOG_INFO("Power mode changed to NORMAL");
        }
    }
    
    return manager->current_state;
}

/**
 * @brief 获取当前轮询间隔
 * @details 根据当前功耗模式获取合适的轮询间隔
 * @param manager 功耗管理器指针
 * @return 轮询间隔（毫秒）
 */
uint32_t power_manager_get_poll_interval(PowerManager_t *manager) {
    if (!manager) {
        return 10; // 默认10毫秒
    }
    
    switch (manager->current_mode) {
        case POWER_MODE_LOW:
            return manager->low_power_poll_interval;
        case POWER_MODE_NORMAL:
        default:
            return manager->normal_poll_interval;
    }
}

/**
 * @brief 进入低功耗模式
 * @details 进入低功耗模式，降低系统功耗
 * @param manager 功耗管理器指针
 * @return 是否成功进入低功耗模式
 */
bool power_manager_enter_low_power(PowerManager_t *manager) {
    if (!manager || !manager->low_power_mode_enabled) {
        return false;
    }
    
    if (manager->current_mode != POWER_MODE_LOW) {
        manager->current_mode = POWER_MODE_LOW;
        manager->current_state = SYSTEM_STATE_IDLE;
        LOG_INFO("System entering LOW power mode");
        
        // 实现具体的低功耗模式逻辑
        
        // 1. 关闭不必要的外设
        LOG_INFO("Disabling unnecessary peripherals...");
        // 关闭WiFi
#ifdef CONFIG_ENABLE_WIFI_MEDIA
        wifi_media_set_power(false);
#endif
        // 关闭蓝牙
        bluetooth_set_power(false);
        
        // 2. 降低CPU频率
        LOG_INFO("Reducing CPU frequency...");
        // 实际实现中，这里应该调用系统API降低CPU频率
        
        // 3. 减少轮询频率
        LOG_INFO("Reducing polling frequency...");
        manager->low_power_poll_interval = 500; // 增加到500ms
        
        // 4. 关闭不必要的LED
        LOG_INFO("Turning off unnecessary LEDs...");
        // 实际实现中，这里应该关闭装饰性LED等
        
        // 5. 降低音频处理精度（如果适用）
        LOG_INFO("Reducing audio processing precision...");
        // 实际实现中，这里应该降低音频采样率或关闭某些音效处理
        
        return true;
    }
    
    return false;
}

/**
 * @brief 退出低功耗模式
 * @details 退出低功耗模式，恢复正常运行状态
 * @param manager 功耗管理器指针
 * @return 是否成功退出低功耗模式
 */
bool power_manager_exit_low_power(PowerManager_t *manager) {
    if (!manager) {
        return false;
    }
    
    if (manager->current_mode == POWER_MODE_LOW) {
        manager->current_mode = POWER_MODE_NORMAL;
        manager->current_state = SYSTEM_STATE_ACTIVE;
        manager->last_activity_time = time(NULL) * 1000;
        LOG_INFO("System exiting LOW power mode");
        
        // 实现具体的退出低功耗模式逻辑
        
        // 1. 恢复必要的外设
        LOG_INFO("Enabling necessary peripherals...");
        // 恢复蓝牙
        bluetooth_set_power(true);
        // 恢复WiFi
#ifdef CONFIG_ENABLE_WIFI_MEDIA
        wifi_media_set_power(true);
#endif
        
        // 2. 恢复CPU频率
        LOG_INFO("Restoring CPU frequency...");
        // 实际实现中，这里应该调用系统API恢复CPU频率
        
        // 3. 恢复轮询频率
        LOG_INFO("Restoring polling frequency...");
        manager->low_power_poll_interval = 100; // 恢复到100ms
        
        // 4. 恢复LED状态
        LOG_INFO("Restoring LED status...");
        // 实际实现中，这里应该恢复LED到正常状态
        
        // 5. 恢复音频处理精度
        LOG_INFO("Restoring audio processing precision...");
        // 实际实现中，这里应该恢复音频采样率或重新启用音效处理
        
        return true;
    }
    
    return false;
}

/**
 * @brief 输出功耗统计信息
 * @details 输出功耗统计和系统状态信息
 * @param manager 功耗管理器指针
 */
void power_manager_print_stats(PowerManager_t *manager) {
    if (!manager) {
        return;
    }
    
    LOG_INFO("=== Power Statistics ===");
    LOG_INFO("Current power mode: %d", manager->current_mode);
    LOG_INFO("Current system state: %d", manager->current_state);
    LOG_INFO("Idle time: %d ms", manager->power_stats.idle_time);
    LOG_INFO("Wakeup count: %d", manager->power_stats.wakeup_count);
    LOG_INFO("Mode durations (ms):");
    LOG_INFO("  Normal: %d", manager->power_stats.mode_duration[POWER_MODE_NORMAL]);
    LOG_INFO("  Low: %d", manager->power_stats.mode_duration[POWER_MODE_LOW]);
    LOG_INFO("  Standby: %d", manager->power_stats.mode_duration[POWER_MODE_STANDBY]);
    LOG_INFO("State durations (ms):");
    LOG_INFO("  Active: %d", manager->power_stats.state_duration[SYSTEM_STATE_ACTIVE]);
    LOG_INFO("  Idle: %d", manager->power_stats.state_duration[SYSTEM_STATE_IDLE]);
    LOG_INFO("  Suspended: %d", manager->power_stats.state_duration[SYSTEM_STATE_SUSPENDED]);
    LOG_INFO("Power management features:");
    LOG_INFO("  Dynamic power management: %s", manager->dynamic_power_management ? "Yes" : "No");
    LOG_INFO("  Low power mode: %s", manager->low_power_mode_enabled ? "Yes" : "No");
    LOG_INFO("=====================");
}

/**
 * @brief 反初始化功耗管理器
 * @details 反初始化功耗管理器，释放资源
 * @param manager 功耗管理器指针
 */
void power_manager_deinit(PowerManager_t *manager) {
    if (!manager) {
        return;
    }
    
    // 打印功耗统计
    power_manager_print_stats(manager);
    
    // 退出低功耗模式
    power_manager_exit_low_power(manager);
    
    // 释放功耗管理器
    free(manager);
    LOG_INFO("Power manager deinitialized");
}
