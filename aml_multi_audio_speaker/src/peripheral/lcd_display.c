#include "peripheral_priv.h"
#include "logger.h"
#include "product_type.h"

#include <aml_i2c.h>          // 晶晨I2C SDK（假设LCD通过I2C接口）

static bool g_lcd_init = false;

// LCD配置（示例值，实际需根据硬件调整）
#define LCD_I2C_BUS          0       // I2C总线号
#define LCD_I2C_ADDR         0x27    // I2C地址
#define LCD_ROWS             2       // 行数
#define LCD_COLS             16      // 列数

// LCD命令定义
#define LCD_CLEAR_DISPLAY    0x01
#define LCD_RETURN_HOME      0x02
#define LCD_ENTRY_MODE_SET   0x06
#define LCD_DISPLAY_CONTROL  0x08
#define LCD_FUNCTION_SET     0x20
#define LCD_SET_CGRAM_ADDR   0x40
#define LCD_SET_DDRAM_ADDR   0x80

// LCD命令位定义
#define LCD_DISPLAY_ON       0x04
#define LCD_CURSOR_ON        0x02
#define LCD_BLINK_ON         0x01
#define LCD_8BIT_MODE        0x10
#define LCD_2LINE            0x08
#define LCD_5x10DOTS         0x04

/**
 * @brief 向LCD发送命令
 */
static int lcd_send_command(uint8_t cmd)
{
    if (!g_lcd_init) {
        return -1;
    }
    
    // 通过I2C发送命令
    uint8_t data[] = {0x80, cmd}; // 0x80表示命令模式
    return aml_i2c_write(LCD_I2C_BUS, LCD_I2C_ADDR, data, sizeof(data));
}

/**
 * @brief 向LCD发送数据
 */
static int lcd_send_data(uint8_t data)
{
    if (!g_lcd_init) {
        return -1;
    }
    
    // 通过I2C发送数据
    uint8_t buf[] = {0x40, data}; // 0x40表示数据模式
    return aml_i2c_write(LCD_I2C_BUS, LCD_I2C_ADDR, buf, sizeof(buf));
}

/**
 * @brief 初始化LCD
 */
static int lcd_hw_init(void)
{
    // 初始化I2C
    if (aml_i2c_init(LCD_I2C_BUS, 100000) != 0) { // 100kHz
        LOG_ERROR("LCD I2C init failed");
        return -1;
    }
    
    // 初始化LCD
    aml_timer_delay_ms(50);
    lcd_send_command(LCD_FUNCTION_SET | LCD_8BIT_MODE | LCD_2LINE);
    aml_timer_delay_ms(5);
    lcd_send_command(LCD_DISPLAY_CONTROL | LCD_DISPLAY_ON);
    aml_timer_delay_ms(5);
    lcd_send_command(LCD_CLEAR_DISPLAY);
    aml_timer_delay_ms(5);
    lcd_send_command(LCD_ENTRY_MODE_SET);
    
    return 0;
}

#ifdef CONFIG_ENABLE_LCD_DISPLAY
int lcd_display_init(void)
{
    if (g_lcd_init) {
        LOG_INFO("LCD display already initialized");
        return 0;
    }
    
    // 检查产品类型是否支持LCD
#if (CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END) || (CURRENT_PRODUCT_TYPE == PRODUCT_MID_END)
    // 初始化LCD硬件
    if (lcd_hw_init() != 0) {
        LOG_ERROR("LCD hardware init failed");
        return -1;
    }
    
    g_lcd_init = true;
    
    LOG_INFO("Peripheral: LCD display init success");
    LOG_INFO("  LCD: %dx%d characters", LCD_COLS, LCD_ROWS);
    LOG_INFO("  I2C: bus %d, address 0x%02X", LCD_I2C_BUS, LCD_I2C_ADDR);
    
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
    lcd_send_command(LCD_CLEAR_DISPLAY);
    
    // 关闭LCD
    lcd_send_command(LCD_DISPLAY_CONTROL);
    
    // 反初始化I2C
    aml_i2c_deinit(LCD_I2C_BUS);
    
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
    
    // 静态变量存储上次更新时间
    static uint32_t last_update_time = 0;
    uint32_t current_time = (uint32_t)time(NULL) * 1000;
    
    // 每200毫秒更新一次LCD显示
    if (current_time - last_update_time > 200) {
        last_update_time = current_time;
        
        // 这里可以添加LCD显示更新的逻辑
        // 例如：根据当前音源、音量、播放状态更新显示
        
        // 示例：检查LCD连接状态
        static bool last_lcd_status = true;
        bool current_lcd_status = (aml_i2c_probe(LCD_I2C_BUS, LCD_I2C_ADDR) == 0);
        
        if (current_lcd_status != last_lcd_status) {
            last_lcd_status = current_lcd_status;
            
            if (current_lcd_status) {
                LOG_INFO("LCD reconnected");
                // 重新初始化LCD显示
                lcd_hw_init();
                g_lcd_init = true;
                lcd_display_text(0, 0, "LCD Reinit");
                LOG_INFO("LCD display reinitialized");
            } else {
                LOG_ERROR("LCD disconnected");
                // 处理LCD断开连接
                g_lcd_init = false;
                LOG_ERROR("LCD display deinitialized");
            }
        }
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
    
    // 设置光标位置
    int addr = col;
    if (row == 1) {
        addr += 0x40; // 第二行地址偏移
    }
    lcd_send_command(LCD_SET_DDRAM_ADDR | addr);
    
    // 发送文本数据
    while (*text && col < LCD_COLS) {
        lcd_send_data(*text++);
        col++;
    }
    
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
    
    return lcd_send_command(LCD_CLEAR_DISPLAY);
}

#else
int lcd_display_init(void) { 
    LOG_INFO("LCD display not enabled in config");
    return 0; 
}
void lcd_display_deinit(void) {}
void lcd_display_event_poll(void) {}
int lcd_display_text(int row, int col, const char *text) { return -1; }
int lcd_display_clear(void) { return -1; }
#endif