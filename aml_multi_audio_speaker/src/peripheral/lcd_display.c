#include "peripheral_priv.h"
#include "logger.h"
#include "product_type.h"

static int g_lcd_init = 0;

#ifdef CONFIG_ENABLE_LCD_DISPLAY
int lcd_display_init(void)
{
    g_lcd_init = 1;
    LOG_INFO("Peripheral: LCD display init success");
    return 0;
}

void lcd_display_deinit(void)
{
    g_lcd_init = 0;
}
#else
int lcd_display_init(void) { return 0; }
void lcd_display_deinit(void) {}
#endif