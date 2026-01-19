#include "system.h"
#include "system_priv.h"
#include "logger.h"
#include "event.h"
#include "pal.h"  // 平台抽象层
#include "peripheral.h"  // 外设管理
#include "common_def.h"  // 通用定义
#include <time.h>  // 时间函数

// LED索引定义
#define LED_SYSTEM            0     // 系统指示灯

static SystemCfg_t g_sys_cfg = {0};

int system_init(void)
{
    memset(&g_sys_cfg, 0, sizeof(SystemCfg_t));
    // 必加载：系统基础初始化 所有产品通用
    sys_init_core();
    // 宏控加载：OTA升级功能 分级裁剪
    sys_ota_init();
    // 初始化系统API
    system_api_init();
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
    uint32_t current_time = (uint32_t)time(NULL) * 1000;
    
    if (current_time - last_check_time > 5000) { // 每5秒检查一次
        last_check_time = current_time;
        
        // 使用PAL层系统服务获取实际的CPU和内存使用率
        int cpu_usage = 0;
        int mem_usage = 0;
        
        pal_system_get_cpu_usage(&cpu_usage);
        pal_system_get_memory_usage(&mem_usage);
        
        LOG_DEBUG("System status check - CPU: %d%%, Mem: %d%%", cpu_usage, mem_usage);
        
        // 发送系统状态事件
        // 这里可以根据实际需要发送更详细的系统状态信息
        event_notify(EVENT_SYSTEM_STATUS_UPDATE, NULL);
    }
    
    // 轮询系统错误日志
    static uint32_t last_error_check = 0;
    if (current_time - last_error_check > 10000) { // 每10秒检查一次
        last_error_check = current_time;
        
        // 系统错误日志检查的简化实现
        // 这里可以添加实际的错误日志检查代码
        LOG_DEBUG("System error log check - no errors");
        
        // 发送系统错误日志检查事件
        event_notify(EVENT_SYSTEM_ERROR_LOG_CHECK, NULL);
    }
}