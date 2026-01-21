#include "peripheral_priv.h"
#include "logger.h"
#include "comm_mcu.h"
#include "common_def.h"

static bool g_led_init = false;

// LED数量定义
#define LED_COUNT            5     // LED数量

int led_ctrl_init(void)
{
    if (g_led_init) {
        LOG_INFO("LED control already initialized");
        return 0;
    }
    
    // 初始化LED控制 - 通过UART与MCU通信，无需直接操作硬件
    g_led_init = true;
    
    LOG_INFO("Peripheral: LED control init success [ALL PRODUCT]");
    LOG_INFO("  LED count: %d", LED_COUNT);
    
    return 0;
}

void led_ctrl_deinit(void)
{
    if (!g_led_init) {
        return;
    }
    
    // 关闭所有LED
    for (int i = 0; i < LED_COUNT; i++) {
        comm_mcu_set_led(i, LED_STATE_OFF);
    }
    
    g_led_init = false;
    
    LOG_INFO("LED control deinitialized");
}

/**
 * @brief 设置LED状态
 */
int led_ctrl_set_state(int led_idx, LedState_e state)
{
    if (!g_led_init || led_idx < 0 || led_idx >= LED_COUNT) {
        LOG_ERROR("LED control: invalid parameters");
        return -1;
    }
    
    // 通过UART发送LED状态设置命令给MCU
    int ret = comm_mcu_set_led(led_idx, state);
    
    if (ret < 0) {
        LOG_ERROR("LED control: failed to send command to MCU");
        return -1;
    }
    
    LOG_INFO("LED %d set to state %d", led_idx, state);
    
    return 0;
}

/**
 * @brief LED事件轮询
 */
void led_ctrl_event_poll(void)
{
    if (!g_led_init) {
        return;
    }
    
    // 通过UART与MCU通信时，无需在SOC端处理闪烁逻辑
    // 闪烁逻辑由MCU端处理
    
    // 可以添加LED状态的周期性检查或其他必要的处理
    LOG_DEBUG("LED control event poll executed");
}
