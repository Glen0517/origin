#include "peripheral_priv.h"
#include "logger.h"
#include "product_type.h"
#include "common_def.h"

#include "comm_mcu.h"        // MCU通信接口

static bool g_lcd_init = false;

// LCD配置（示例值，实际需根据硬件调整）
#define LCD_ROWS             2       // 行数
#define LCD_COLS             16      // 列数

// LCD命令定义（用于UART通信）
#define LCD_CMD_CLEAR_DISPLAY    0x01
#define LCD_CMD_DISPLAY_TEXT     0x02
#define LCD_CMD_SET_CURSOR       0x03

#ifdef CONFIG_ENABLE_LCD_DISPLAY
int lcd_display_init(void)
{
    if (g_lcd_init) {
        LOG_INFO("LCD display already initialized");
        return 0;
    }
    
    // 检查产品类型是否支持LCD
#if (CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END) || (CURRENT_PRODUCT_TYPE == PRODUCT_MID_END)
    // 通过UART初始化LCD
    // 这里可以发送LCD初始化命令给MCU
    
    g_lcd_init = true;
    
    LOG_INFO("Peripheral: LCD display init success");
    LOG_INFO("  LCD: %dx%d characters", LCD_COLS, LCD_ROWS);
    
    return 0;
#else
    LOG_INFO("LCD display not supported for current product type");
    return 0;
#endif
}

void lcd_display_deinit(void)
{
    if (!g_lcd_init) {
        return;
    }
    
    // 清除LCD显示
    lcd_display_clear();
    
    g_lcd_init = false;
    
    LOG_INFO("LCD display deinitialized");
}

/**
 * @brief LCD事件轮询
 */
void lcd_display_event_poll(void)
{
    if (!g_lcd_init) {
        return;
    }
    
    // 通过UART与MCU通信时，无需在SOC端处理复杂的LCD逻辑
    // 可以添加简单的状态检查逻辑
    static bool init_info_logged = false;
    if (!init_info_logged) {
        LOG_DEBUG("LCD display event poll started");
        init_info_logged = true;
    }
}

/**
 * @brief 在LCD上显示文本
 */
int lcd_display_text(int row, int col, const char *text)
{
    if (!g_lcd_init || !text || row < 0 || row >= LCD_ROWS || col < 0 || col >= LCD_COLS) {
        return -1;
    }
    
    // 构建LCD显示命令数据
    // 格式：[命令码][行][列][文本长度][文本内容]
    uint8_t data[3 + LCD_COLS] = {0};
    data[0] = LCD_CMD_DISPLAY_TEXT;
    data[1] = (uint8_t)row;
    data[2] = (uint8_t)col;
    
    // 复制文本内容
    int text_len = 0;
    while (*text && text_len < LCD_COLS - col) {
        data[3 + text_len] = *text++;
        text_len++;
    }
    
    // 通过UART发送命令给MCU
    // 注意：这里需要根据实际的UART协议扩展来实现
    // 目前暂未实现具体的命令发送，需要在MCU端添加相应的处理逻辑
    LOG_INFO("LCD display text: row=%d, col=%d, text=%s", row, col, text);
    
    return 0;
}

/**
 * @brief 清除LCD显示
 */
int lcd_display_clear(void)
{
    if (!g_lcd_init) {
        return -1;
    }
    
    // 构建LCD清除命令数据
    uint8_t data[1] = {LCD_CMD_CLEAR_DISPLAY};
    
    // 通过UART发送命令给MCU
    // 注意：这里需要根据实际的UART协议扩展来实现
    // 目前暂未实现具体的命令发送，需要在MCU端添加相应的处理逻辑
    LOG_INFO("LCD display clear");
    
    return 0;
}

#else
int lcd_display_init(void) { 
    LOG_INFO("LCD display not enabled in config");
    return 0; 
}

void lcd_display_deinit(void) {
    LOG_INFO("LCD display deinit called (not enabled)");
    // 虽然未启用，但保持函数接口一致
}

/**
 * @brief LCD事件轮询
 */
void lcd_display_event_poll(void) {
    // 虽然未启用，但保持函数接口一致
    // 可以添加简单的状态检查逻辑
    static bool init_warn_logged = false;
    if (!init_warn_logged) {
        LOG_DEBUG("LCD display not enabled, event poll skipped");
        init_warn_logged = true;
    }
}
int lcd_display_text(int row, int col, const char *text) { return -1; }
int lcd_display_clear(void) { return -1; }
#endif