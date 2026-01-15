#include "peripheral_priv.h"
#include "logger.h"

#include <aml_gpio.h>        // 晶晨GPIO SDK
#include <aml_pwm.h>         // 晶晨PWM SDK
#include <aml_timer.h>       // 晶晨定时器SDK

static bool g_led_init = false;

// LED定义（示例值，实际需根据硬件调整）
#define LED_POWER_PIN        19     // 电源指示灯
#define LED_SOURCE_PIN       20     // 音源指示灯
#define LED_VOLUME_PIN       21     // 音量指示灯
#define LED_BT_PIN           22     // 蓝牙指示灯
#define LED_WIFI_PIN         23     // WIFI指示灯

// LED状态定义
#define LED_OFF              0
#define LED_ON               1
#define LED_BLINK_SLOW       2
#define LED_BLINK_FAST       3

/**
 * @brief LED状态结构体
 */
typedef struct {
    int pin;                  // LED GPIO引脚
    LedState_e state;         // 当前状态
    int blink_interval_ms;    // 闪烁间隔（毫秒）
    bool current_level;       // 当前电平
    uint64_t last_toggle;     // 上次切换时间
} LedCtrl_t;

static LedCtrl_t g_leds[] = {
    {LED_POWER_PIN, LED_STATE_OFF, 0, false, 0},
    {LED_SOURCE_PIN, LED_STATE_OFF, 0, false, 0},
    {LED_VOLUME_PIN, LED_STATE_OFF, 0, false, 0},
    {LED_BT_PIN, LED_STATE_OFF, 0, false, 0},
    {LED_WIFI_PIN, LED_STATE_OFF, 0, false, 0},
};

static int g_led_count = sizeof(g_leds) / sizeof(g_leds[0]);

/**
 * @brief 设置LED电平
 */
static void set_led_level(int led_idx, bool level)
{
    if (led_idx < 0 || led_idx >= g_led_count) {
        return;
    }
    
    LedCtrl_t *led = &g_leds[led_idx];
    aml_gpio_set_value(led->pin, level);
    led->current_level = level;
}

/**
 * @brief 初始化单个LED
 */
static void init_single_led(int led_idx)
{
    if (led_idx < 0 || led_idx >= g_led_count) {
        return;
    }
    
    LedCtrl_t *led = &g_leds[led_idx];
    aml_gpio_set_direction(led->pin, GPIO_DIR_OUTPUT);
    set_led_level(led_idx, LED_OFF);
}

int led_ctrl_init(void)
{
    if (g_led_init) {
        LOG_INFO("LED control already initialized");
        return 0;
    }
    
    g_led_init = true;
    
    // 初始化所有LED
    for (int i = 0; i < g_led_count; i++) {
        init_single_led(i);
    }
    
    LOG_INFO("Peripheral: LED control init success [ALL PRODUCT]");
    LOG_INFO("  LED count: %d", g_led_count);
    
    return 0;
}

void led_ctrl_deinit(void)
{
    if (!g_led_init) {
        return;
    }
    
    // 关闭所有LED
    for (int i = 0; i < g_led_count; i++) {
        set_led_level(i, LED_OFF);
    }
    
    g_led_init = false;
    
    LOG_INFO("LED control deinitialized");
}

/**
 * @brief 设置LED状态
 */
int led_ctrl_set_state(int led_idx, LedState_e state)
{
    if (!g_led_init || led_idx < 0 || led_idx >= g_led_count) {
        LOG_ERROR("LED control: invalid parameters");
        return -1;
    }
    
    LedCtrl_t *led = &g_leds[led_idx];
    led->state = state;
    
    switch (state) {
        case LED_STATE_OFF:
            led->blink_interval_ms = 0;
            set_led_level(led_idx, LED_OFF);
            break;
        
        case LED_STATE_ON:
            led->blink_interval_ms = 0;
            set_led_level(led_idx, LED_ON);
            break;
        
        case LED_STATE_BLINK_SLOW:
            led->blink_interval_ms = 1000; // 1秒闪烁一次
            led->last_toggle = aml_timer_get_ms();
            break;
        
        case LED_STATE_BLINK_FAST:
            led->blink_interval_ms = 200;  // 200毫秒闪烁一次
            led->last_toggle = aml_timer_get_ms();
            break;
        
        default:
            LOG_ERROR("LED control: invalid state");
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
    
    uint64_t current_time = aml_timer_get_ms();
    
    // 更新所有LED状态
    for (int i = 0; i < g_led_count; i++) {
        LedCtrl_t *led = &g_leds[i];
        
        // 处理闪烁状态
        if (led->blink_interval_ms > 0) {
            if (current_time - led->last_toggle >= led->blink_interval_ms) {
                // 切换LED状态
                set_led_level(i, !led->current_level);
                led->last_toggle = current_time;
            }
        }
    }
}