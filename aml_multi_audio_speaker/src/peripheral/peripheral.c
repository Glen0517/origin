#include "peripheral.h"
#include "peripheral_priv.h"
#include "logger.h"

static PeripheralCfg_t g_peri_cfg = {0};

int peripheral_init(void)
{
    memset(&g_peri_cfg, 0, sizeof(PeripheralCfg_t));
    // 必加载外设：所有产品都有
    led_ctrl_init();
    prompt_sound_init();
    // 宏控加载外设：按需裁剪
    key_ir_init();
    lcd_display_init();

    g_peri_cfg.init_ok = 1;
    LOG_INFO("Peripheral module init success");
    return 0;
}

void peripheral_deinit(void)
{
    if (g_peri_cfg.init_ok)
    {
        lcd_display_deinit();
        key_ir_deinit();
        prompt_sound_deinit();
        led_ctrl_deinit();
        g_peri_cfg.init_ok = 0;
        LOG_INFO("Peripheral module deinit success");
    }
}

void peripheral_event_poll(void)
{
    if (!g_peri_cfg.init_ok) return;
    // 轮询按键/红外/外设状态事件
}