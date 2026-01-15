#include "peripheral.h"
#include "peripheral_priv.h"
#include "logger.h"

static PeripheralConfig_t g_peri_cfg = {0};
static bool g_peripheral_init = false;
static KeyEvent_e g_last_key_event = KEY_EVENT_NONE;

/**
 * @brief 外设模块初始化
 */
int peripheral_init(PeripheralConfig_t *cfg)
{
    if (g_peripheral_init) {
        LOG_INFO("Peripheral module already initialized");
        return 0;
    }
    
    memset(&g_peri_cfg, 0, sizeof(PeripheralConfig_t));
    
    // 使用默认配置或用户提供的配置
    if (cfg) {
        g_peri_cfg = *cfg;
    } else {
        // 默认配置
        g_peri_cfg.key_debounce_ms = 50;
        g_peri_cfg.long_press_ms = 1000;
        g_peri_cfg.ir_learn_en = false;
        g_peri_cfg.mic_mute_en = false;
    }
    
    LOG_INFO("Peripheral module init with config:");
    LOG_INFO("  Key debounce: %d ms", g_peri_cfg.key_debounce_ms);
    LOG_INFO("  Long press: %d ms", g_peri_cfg.long_press_ms);
    LOG_INFO("  IR learn: %s", g_peri_cfg.ir_learn_en ? "enabled" : "disabled");
    LOG_INFO("  Mic mute: %s", g_peri_cfg.mic_mute_en ? "enabled" : "disabled");
    
    // 必加载外设：所有产品都有
    if (led_ctrl_init() != 0) {
        LOG_ERROR("LED control init failed");
        return -1;
    }
    
    if (prompt_sound_init() != 0) {
        LOG_ERROR("Prompt sound init failed");
        led_ctrl_deinit();
        return -1;
    }
    
    // 宏控加载外设：按需裁剪
    if (key_ir_init() != 0) {
        LOG_ERROR("Key/IR init failed");
        prompt_sound_deinit();
        led_ctrl_deinit();
        return -1;
    }
    
    if (lcd_display_init() != 0) {
        LOG_ERROR("LCD display init failed");
        key_ir_deinit();
        prompt_sound_deinit();
        led_ctrl_deinit();
        return -1;
    }
    
    g_peripheral_init = true;
    
    LOG_INFO("Peripheral module init success");
    return 0;
}

/**
 * @brief 外设模块反初始化
 */
int peripheral_deinit(void)
{
    if (!g_peripheral_init) {
        LOG_INFO("Peripheral module not initialized");
        return 0;
    }
    
    lcd_display_deinit();
    key_ir_deinit();
    prompt_sound_deinit();
    led_ctrl_deinit();
    
    g_peripheral_init = false;
    g_last_key_event = KEY_EVENT_NONE;
    
    LOG_INFO("Peripheral module deinit success");
    return 0;
}

/**
 * @brief 设置LED状态
 */
int peripheral_set_led(int led_idx, LedState_e state)
{
    if (!g_peripheral_init) {
        LOG_ERROR("Peripheral module not initialized");
        return -1;
    }
    
    return led_ctrl_set_state(led_idx, state);
}

/**
 * @brief 获取按键事件
 */
KeyEvent_e peripheral_get_key_event(void)
{
    if (!g_peripheral_init) {
        return KEY_EVENT_NONE;
    }
    
    // 获取按键事件
    KeyEvent_e event = key_ir_get_event();
    
    if (event != KEY_EVENT_NONE) {
        g_last_key_event = event;
        LOG_INFO("Key event: %d", event);
    }
    
    return event;
}

/**
 * @brief 外设事件轮询
 */
void peripheral_event_poll(void)
{
    if (!g_peripheral_init) {
        return;
    }
    
    // 轮询按键/红外事件
    key_ir_event_poll();
    
    // 轮询LCD显示事件
    lcd_display_event_poll();
    
    // 轮询LED控制事件
    led_ctrl_event_poll();
}

/**
 * @brief 红外学习开始
 */
#if CONFIG_ENABLE_IR_LEARN
int peripheral_ir_learn_start(void)
{
    if (!g_peripheral_init || !g_peri_cfg.ir_learn_en) {
        LOG_ERROR("IR learn not supported or not initialized");
        return -1;
    }
    
    return key_ir_ir_learn_start();
}

/**
 * @brief 红外学习停止
 */
int peripheral_ir_learn_stop(void)
{
    if (!g_peripheral_init || !g_peri_cfg.ir_learn_en) {
        LOG_ERROR("IR learn not supported or not initialized");
        return -1;
    }
    
    return key_ir_ir_learn_stop();
}
#endif