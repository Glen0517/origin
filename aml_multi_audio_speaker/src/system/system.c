#include "system.h"
#include "system_priv.h"
#include "logger.h"
#include "event.h"
#include "pal.h"  // 平台抽象层
#include "peripheral.h"  // 外设管理
#include "common_def.h"  // 通用定义
#include "audio_core.h"  // 音频核心
#include "bluetooth.h"  // 蓝牙
#include "wifi_media.h"  // WiFi媒体
#include "error_handling.h"  // 统一错误处理
#include <time.h>  // 时间函数

// LED索引定义
#define LED_SYSTEM            0     // 系统指示灯

static SystemCfg_t g_sys_cfg = {0};

// 系统资源使用阈值
#define CPU_USAGE_THRESHOLD_HIGH    80  // CPU使用率高阈值
#define CPU_USAGE_THRESHOLD_CRITICAL 90  // CPU使用率临界阈值
#define MEM_USAGE_THRESHOLD_HIGH    80  // 内存使用率高阈值
#define MEM_USAGE_THRESHOLD_CRITICAL 90  // 内存使用率临界阈值

// 系统状态监控结构
typedef struct {
    int cpu_usage;           // CPU使用率
    int mem_usage;           // 内存使用率
    int temp;                // 温度
    int voltage;             // 电压
    int uptime;              // 运行时间（秒）
    int error_count;         // 错误计数
    int recovery_count;      // 恢复计数
    SysState_e last_state;   // 上一次系统状态
    int state_change_count;  // 状态变化计数
} SystemMonitor_t;

// 系统监控数据
static SystemMonitor_t g_sys_monitor = {0};

// 系统错误处理回调函数
static void system_error_callback(ErrorInfo_t *error_info)
{
    LOG_ERROR("System error callback: type=%d, level=%d, module=%s, message=%s, code=%d",
             error_info->type, error_info->level, error_info->module, error_info->message, error_info->code);
    
    // 更新系统监控数据
    g_sys_monitor.error_count++;
    
    // 处理不同级别的错误
    switch (error_info->level) {
        case ERROR_LEVEL_CRITICAL:
        case ERROR_LEVEL_FATAL:
            // 严重错误，触发系统恢复
            system_recovery();
            break;
        case ERROR_LEVEL_ERROR:
            // 记录错误
            break;
        default:
            break;
    }
}

// 系统错误恢复函数
static int system_error_recovery(ErrorInfo_t *error_info)
{
    LOG_INFO("System error recovery: type=%d, message=%s", error_info->type, error_info->message);
    
    // 增加恢复计数
    g_sys_monitor.recovery_count++;
    
    // 根据错误类型执行不同的恢复策略
    switch (error_info->type) {
        case ERROR_SYSTEM_RESOURCE:
            // 资源错误，释放资源
            system_release_resources();
            break;
        case ERROR_SYSTEM_MEMORY:
            // 内存错误，尝试内存回收
            system_memory_recovery();
            break;
        case ERROR_SYSTEM_TEMPERATURE:
            // 温度错误，降低系统负载
            system_reduce_load();
            break;
        default:
            break;
    }
    
    return SUCCESS;
}
// 记录系统错误
static void log_system_error(int error_type, const char *error_msg) {
    // 使用统一错误处理机制
    error_report(error_type, ERROR_LEVEL_ERROR, "system", error_msg, 0, NULL);
}

// 获取系统温度（模拟实现）
static int get_system_temperature(void) {
    // 实际实现中应该从硬件获取温度
    // 这里返回模拟值
    return 45; // 假设温度为45度
}

// 获取系统电压（模拟实现）
static int get_system_voltage(void) {
    // 实际实现中应该从硬件获取电压
    // 这里返回模拟值
    return 5000; // 假设电压为5V
}

// 获取系统运行时间
static int get_system_uptime(void) {
    return (int)time(NULL);
}

// 系统自动恢复函数
static int system_auto_recover(void) {
    LOG_INFO("System auto recovery started (Attempt %d)", ++g_sys_monitor.recovery_count);
    
    int recovery_actions = 0;
    
    // 1. 检查音频核心
    if (audio_core_get_init_status() != SUCCESS) {
        LOG_WARN("Audio core not initialized, attempting recovery");
        if (audio_core_err_recover() == SUCCESS) {
            LOG_INFO("Audio core recovery successful");
            recovery_actions++;
        }
    }
    
    // 2. 检查蓝牙
    if (!bluetooth_get_connect_state()) {
        LOG_DEBUG("Bluetooth not connected, no recovery needed");
    }
    
    // 3. 检查WiFi媒体
    if (!wifi_media_get_connect_state()) {
        LOG_DEBUG("WiFi not connected, no recovery needed");
    }
    
    // 4. 检查系统资源
    if (g_sys_monitor.cpu_usage > CPU_USAGE_THRESHOLD_CRITICAL) {
        LOG_WARN("High CPU usage, attempting to reduce load");
        // 实际实现中应该采取措施降低CPU负载
        recovery_actions++;
    }
    
    if (g_sys_monitor.mem_usage > MEM_USAGE_THRESHOLD_CRITICAL) {
        LOG_WARN("High memory usage, attempting to free memory");
        // 实际实现中应该采取措施释放内存
        recovery_actions++;
    }
    
    // 5. 检查系统温度
    if (g_sys_monitor.temp > 60) {
        LOG_WARN("High system temperature, attempting to cool down");
        // 实际实现中应该采取措施降低温度
        recovery_actions++;
    }
    
    if (recovery_actions > 0) {
        LOG_INFO("System auto recovery completed, %d actions taken", recovery_actions);
        // 发送系统恢复事件
        event_notify(EVENT_SYSTEM_RECOVERY_COMPLETED, (void *)&recovery_actions);
        return SUCCESS;
    } else {
        LOG_INFO("System auto recovery completed, no actions needed");
        return SUCCESS;
    }
}

// 系统状态检查函数
static void check_system_status(void) {
    // 获取系统资源使用情况
    pal_system_get_cpu_usage(&g_sys_monitor.cpu_usage);
    pal_system_get_memory_usage(&g_sys_monitor.mem_usage);
    
    // 获取系统温度和电压
    g_sys_monitor.temp = get_system_temperature();
    g_sys_monitor.voltage = get_system_voltage();
    
    // 更新系统运行时间
    g_sys_monitor.uptime = get_system_uptime();
    
    // 检查CPU使用率
    if (g_sys_monitor.cpu_usage > CPU_USAGE_THRESHOLD_CRITICAL) {
        LOG_WARN("CRITICAL: CPU usage too high: %d%%", g_sys_monitor.cpu_usage);
        log_system_error(1, "CPU usage critical");
        // 触发自动恢复
        system_auto_recover();
    } else if (g_sys_monitor.cpu_usage > CPU_USAGE_THRESHOLD_HIGH) {
        LOG_WARN("WARNING: CPU usage high: %d%%", g_sys_monitor.cpu_usage);
    }
    
    // 检查内存使用率
    if (g_sys_monitor.mem_usage > MEM_USAGE_THRESHOLD_CRITICAL) {
        LOG_WARN("CRITICAL: Memory usage too high: %d%%", g_sys_monitor.mem_usage);
        log_system_error(2, "Memory usage critical");
        // 触发自动恢复
        system_auto_recover();
    } else if (g_sys_monitor.mem_usage > MEM_USAGE_THRESHOLD_HIGH) {
        LOG_WARN("WARNING: Memory usage high: %d%%", g_sys_monitor.mem_usage);
    }
    
    // 检查系统温度
    if (g_sys_monitor.temp > 70) {
        LOG_WARN("CRITICAL: System temperature too high: %dC", g_sys_monitor.temp);
        log_system_error(3, "System temperature critical");
        // 触发自动恢复
        system_auto_recover();
    } else if (g_sys_monitor.temp > 60) {
        LOG_WARN("WARNING: System temperature high: %dC", g_sys_monitor.temp);
    }
    
    // 检查系统电压
    if (g_sys_monitor.voltage < 4500 || g_sys_monitor.voltage > 5500) {
        LOG_WARN("WARNING: System voltage out of range: %d mV", g_sys_monitor.voltage);
        log_system_error(4, "System voltage out of range");
    }
    
    // 检查系统状态变化
    SysState_e current_state = system_api_get_state();
    if (current_state != g_sys_monitor.last_state) {
        g_sys_monitor.state_change_count++;
        LOG_INFO("System state changed: %d -> %d (Change count: %d)", 
                 g_sys_monitor.last_state, current_state, g_sys_monitor.state_change_count);
        g_sys_monitor.last_state = current_state;
    }
}

// 打印系统状态摘要
static void print_system_status_summary(void) {
    LOG_INFO("System Status Summary:");
    LOG_INFO("  CPU Usage: %d%%", g_sys_monitor.cpu_usage);
    LOG_INFO("  Memory Usage: %d%%", g_sys_monitor.mem_usage);
    LOG_INFO("  Temperature: %dC", g_sys_monitor.temp);
    LOG_INFO("  Voltage: %d mV", g_sys_monitor.voltage);
    LOG_INFO("  Uptime: %d seconds", g_sys_monitor.uptime);
    LOG_INFO("  Error Count: %d", g_sys_monitor.error_count);
    LOG_INFO("  Recovery Count: %d", g_sys_monitor.recovery_count);
    LOG_INFO("  State Change Count: %d", g_sys_monitor.state_change_count);
    LOG_INFO("  Current State: %d", system_api_get_state());
    
    // 打印最近的错误
    if (g_error_log_count > 0) {
        LOG_INFO("  Recent Errors: %d", g_error_log_count);
        int start_idx = (g_error_log_index - g_error_log_count + MAX_ERROR_LOGS) % MAX_ERROR_LOGS;
        for (int i = 0; i < g_error_log_count && i < 3; i++) {
            int idx = (start_idx + i) % MAX_ERROR_LOGS;
            LOG_INFO("    Error %d: Type=%d, Msg=%s", i+1, 
                     g_error_logs[idx].error_type, g_error_logs[idx].error_msg);
        }
    }
}

int system_init(void)
{
    memset(&g_sys_cfg, 0, sizeof(SystemCfg_t));
    // 必加载：系统基础初始化 所有产品通用
    sys_init_core();
    // 宏控加载：OTA升级功能 分级裁剪
    sys_ota_init();
    // 初始化系统API
    system_api_init();
    
    // 初始化系统监控数据
    memset(&g_sys_monitor, 0, sizeof(SystemMonitor_t));
    g_sys_monitor.last_state = SYS_STATE_IDLE;
    
    // 初始化统一错误处理模块
    error_handling_init();
    
    // 注册系统错误回调函数
    error_register_callback(ERROR_SYSTEM_BASE, system_error_callback);
    error_register_recovery(ERROR_SYSTEM_BASE, system_error_recovery);
    
    g_sys_cfg.init_ok = 1;
    LOG_INFO("System module init success (OTA: %d)", CONFIG_ENABLE_DUAL_OTA);
    return 0;
}

void system_deinit(void)
{
    if (g_sys_cfg.init_ok)
    {
        sys_ota_deinit();
        sys_init_deinit();
        // 反初始化系统API
        system_api_deinit();
        // 反初始化错误处理模块
        error_handling_deinit();
        g_sys_cfg.init_ok = 0;
        LOG_INFO("System module deinit success");
    }
}

/**
 * @brief 设置系统状态
 * @param state 系统状态
 * @return SUCCESS表示成功，FAILURE表示失败
 */
int system_set_state(SysState_e state) {
    LOG_INFO("Setting system state to: %d", state);
    
    // 验证状态值
    if (state < SYS_STATE_IDLE || state > SYS_STATE_STANDBY) {
        LOG_ERROR("Invalid system state: %d", state);
        return FAILURE;
    }
    
    // 处理特殊状态 - 待机状态
    if (state == SYS_STATE_STANDBY) {
        LOG_INFO("System entering standby state");
        // 执行待机操作
        led_ctrl_set_state(LED_SYSTEM, LED_STATE_OFF);
        lcd_display_text(0, 0, "System: Standby");
        event_notify(EVENT_SYSTEM_STANDBY, NULL);
        return SUCCESS;
    }
    
    // 调用系统API设置状态
    int ret = system_api_set_state(state);
    if (ret != SUCCESS) {
        LOG_ERROR("Failed to set system state: %d", ret);
        return FAILURE;
    }
    
    LOG_INFO("System state set to: %d successfully", state);
    return SUCCESS;
}

void system_event_poll(void)
{
    if (!g_sys_cfg.init_ok) return;
    
    // 轮询系统状态变化
    static SysState_e last_sys_state = SYS_STATE_IDLE;
    SysState_e current_sys_state = system_api_get_state();
    
    if (current_sys_state != last_sys_state) {
        LOG_INFO("System state changed: %d -> %d", last_sys_state, current_sys_state);
        last_sys_state = current_sys_state;
        
        // 根据系统状态执行相应操作
        switch (current_sys_state) {
            case SYS_STATE_WORKING:
                // 处理系统工作状态
                LOG_INFO("System entering working state");
                led_ctrl_set_state(LED_SYSTEM, LED_STATE_ON);
                lcd_display_text(0, 0, "System: Working");
                event_notify(EVENT_SYSTEM_WORKING, NULL);
                break;
            case SYS_STATE_OTA:
                // 处理系统OTA升级状态
                LOG_INFO("System entering OTA state");
                led_ctrl_set_state(LED_SYSTEM, LED_STATE_BREATH);
                lcd_display_text(0, 0, "System: OTA");
                event_notify(EVENT_SYSTEM_OTA_START, NULL);
                break;
            case SYS_STATE_CALIB:
                // 处理系统校准状态
                LOG_INFO("System entering calibration state");
                led_ctrl_set_state(LED_SYSTEM, LED_STATE_FLASH_SLOW);
                lcd_display_text(0, 0, "System: Calib");
                event_notify(EVENT_SYSTEM_CALIB_START, NULL);
                break;
            case SYS_STATE_AGE_TEST:
                // 处理系统老化测试状态
                LOG_INFO("System entering age test state");
                led_ctrl_set_state(LED_SYSTEM, LED_STATE_FLASH_FAST);
                lcd_display_text(0, 0, "System: AgeTest");
                event_notify(EVENT_SYSTEM_AGE_TEST_START, NULL);
                break;
            case SYS_STATE_ERROR:
                // 处理系统错误状态
                LOG_INFO("System entering error state");
                led_ctrl_set_state(LED_SYSTEM, LED_STATE_FLASH_FAST);
                lcd_display_text(0, 0, "System: Error");
                event_notify(EVENT_SYSTEM_ERROR, NULL);
                break;
            default:
                break;
        }
    }
    
    // 轮询系统资源使用情况
    static uint32_t last_check_time = 0;
    static uint32_t last_summary_time = 0;
    uint32_t current_time = (uint32_t)time(NULL) * 1000;
    
    if (current_time - last_check_time > 2000) { // 每2秒检查一次
        last_check_time = current_time;
        
        // 检查系统状态
        check_system_status();
        
        // 发送系统状态事件
        event_notify(EVENT_SYSTEM_STATUS_UPDATE, NULL);
    }
    
    // 每30秒打印一次系统状态摘要
    if (current_time - last_summary_time > 30000) {
        last_summary_time = current_time;
        print_system_status_summary();
    }
    
    // 轮询系统错误日志
    static uint32_t last_error_check = 0;
    if (current_time - last_error_check > 10000) { // 每10秒检查一次
        last_error_check = current_time;
        
        // 系统错误日志检查
        if (g_error_log_count > 0) {
            LOG_INFO("System error log check - %d errors recorded", g_error_log_count);
        } else {
            LOG_DEBUG("System error log check - no errors");
        }
        
        // 发送系统错误日志检查事件
        event_notify(EVENT_SYSTEM_ERROR_LOG_CHECK, NULL);
    }
}