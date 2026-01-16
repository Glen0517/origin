#include "peripheral_priv.h"
#include "logger.h"
#include "product_type.h"

#include <aml_gpio.h>        // 晶晨GPIO SDK
#include <aml_ir.h>          // 晶晨红外SDK
#include <aml_timer.h>       // 晶晨定时器SDK

#include "comm_mcu.h"        // MCU通信接口

bool g_key_ir_init = false;
static bool g_ir_enabled = false;
static bool g_ir_learning = false;
KeyEvent_e g_last_key_event = KEY_EVENT_NONE;

// 按键GPIO定义（示例值，实际需根据硬件调整）
#define KEY_PLAY_PAUSE_PIN   14     // 播放/暂停键
#define KEY_VOL_UP_PIN       15     // 音量+键
#define KEY_VOL_DOWN_PIN     16     // 音量-键
#define KEY_SOURCE_PIN       17     // 音源切换键
#define KEY_SOUND_MODE_PIN   18     // 音效模式键

// 按键防抖时间（毫秒）
#define KEY_DEBOUNCE_MS      50

/**
 * @brief 按键中断回调函数
 */
static void key_interrupt_callback(int pin, int value)
{
    if (!g_key_ir_init) {
        return;
    }
    
    // 防抖处理
    aml_timer_delay_ms(KEY_DEBOUNCE_MS);
    
    // 再次读取按键状态
    int current_value = aml_gpio_get_value(pin);
    
    if (current_value < 0) {
        LOG_ERROR("Failed to read GPIO value for pin %d", pin);
        return;
    }
    
    if (current_value != value) {
        return; // 状态变化，可能是抖动
    }
    
    // 检测到有效的按键事件
    KeyEvent_e event = KEY_EVENT_NONE;
    
    switch (pin) {
        case KEY_PLAY_PAUSE_PIN:
            event = KEY_EVENT_PLAY_PAUSE;
            break;
        case KEY_VOL_UP_PIN:
            event = KEY_EVENT_VOL_UP;
            break;
        case KEY_VOL_DOWN_PIN:
            event = KEY_EVENT_VOL_DOWN;
            break;
        case KEY_SOURCE_PIN:
            event = KEY_EVENT_SOURCE_SWITCH;
            break;
        case KEY_SOUND_MODE_PIN:
            event = KEY_EVENT_SOUND_MODE;
            break;
        default:
            LOG_WARN("Unknown GPIO pin %d in interrupt callback", pin);
            break;
    }
    
    if (event != KEY_EVENT_NONE && current_value == 0) { // 按键按下时触发
        g_last_key_event = event;
        LOG_INFO("Key pressed: %d", event);
    }
}

/**
 * @brief 红外接收回调函数
 */
static void ir_receive_callback(uint32_t ir_code)
{
    if (!g_key_ir_init || !g_ir_enabled) {
        return;
    }
    
    LOG_INFO("IR code received: 0x%08X", ir_code);
    
    // 根据红外码映射到按键事件
    KeyEvent_e event = KEY_EVENT_NONE;
    
    // 红外码映射表
    switch (ir_code) {
        case 0x00FF00FF:
            event = KEY_EVENT_PLAY_PAUSE;
            break;
        case 0x00FF01FE:
            event = KEY_EVENT_VOL_UP;
            break;
        case 0x00FF02FD:
            event = KEY_EVENT_VOL_DOWN;
            break;
        case 0x00FF03FC:
            event = KEY_EVENT_SOURCE_SWITCH;
            break;
        case 0x00FF04FB:
            event = KEY_EVENT_SOUND_MODE;
            break;
        case 0x00FF05FA:
            event = KEY_EVENT_NEXT;
            break;
        case 0x00FF06F9:
            event = KEY_EVENT_PREV;
            break;
        case 0x00FF07F8:
            event = KEY_EVENT_POWER;
            break;
        case 0x00FF08F7:
            event = KEY_EVENT_MUTE;
            break;
        case 0x00FF09F6:
            event = KEY_EVENT_EQ;
            break;
        case 0x00FF0AF5:
            event = KEY_EVENT_BASS_UP;
            break;
        case 0x00FF0BF4:
            event = KEY_EVENT_BASS_DOWN;
            break;
        case 0x00FF0CF3:
            event = KEY_EVENT_TREBLE_UP;
            break;
        case 0x00FF0DF2:
            event = KEY_EVENT_TREBLE_DOWN;
            break;
        default:
            break;
    }
    
    if (event != KEY_EVENT_NONE) {
        g_last_key_event = event;
        LOG_INFO("IR event: %d", event);
    }
}

int key_ir_init(void)
{
    if (g_key_ir_init) {
        LOG_INFO("Key/IR already initialized");
        return SUCCESS;
    }
    
    g_key_ir_init = true;
    g_ir_enabled = false;
    g_ir_learning = false;
    g_last_key_event = KEY_EVENT_NONE;
    
#if (CURRENT_PRODUCT_TYPE == PRODUCT_LOW_END)
    // 低端产品：仅保留物理按键，关闭红外
    LOG_INFO("Peripheral: Key init success (IR disable)");
    
    // 初始化按键GPIO
    if (aml_gpio_set_direction(KEY_PLAY_PAUSE_PIN, GPIO_DIR_INPUT) != 0) {
        LOG_ERROR("Failed to set direction for KEY_PLAY_PAUSE_PIN");
    }
    if (aml_gpio_set_direction(KEY_VOL_UP_PIN, GPIO_DIR_INPUT) != 0) {
        LOG_ERROR("Failed to set direction for KEY_VOL_UP_PIN");
    }
    if (aml_gpio_set_direction(KEY_VOL_DOWN_PIN, GPIO_DIR_INPUT) != 0) {
        LOG_ERROR("Failed to set direction for KEY_VOL_DOWN_PIN");
    }
    if (aml_gpio_set_direction(KEY_SOURCE_PIN, GPIO_DIR_INPUT) != 0) {
        LOG_ERROR("Failed to set direction for KEY_SOURCE_PIN");
    }
    
    // 注册按键中断回调
    if (aml_gpio_register_interrupt(KEY_PLAY_PAUSE_PIN, GPIO_INT_FALLING, key_interrupt_callback) != 0) {
        LOG_ERROR("Failed to register interrupt for KEY_PLAY_PAUSE_PIN");
    }
    if (aml_gpio_register_interrupt(KEY_VOL_UP_PIN, GPIO_INT_FALLING, key_interrupt_callback) != 0) {
        LOG_ERROR("Failed to register interrupt for KEY_VOL_UP_PIN");
    }
    if (aml_gpio_register_interrupt(KEY_VOL_DOWN_PIN, GPIO_INT_FALLING, key_interrupt_callback) != 0) {
        LOG_ERROR("Failed to register interrupt for KEY_VOL_DOWN_PIN");
    }
    if (aml_gpio_register_interrupt(KEY_SOURCE_PIN, GPIO_INT_FALLING, key_interrupt_callback) != 0) {
        LOG_ERROR("Failed to register interrupt for KEY_SOURCE_PIN");
    }
    
    // 启用按键中断
    if (aml_gpio_enable_interrupt(KEY_PLAY_PAUSE_PIN, true) != 0) {
        LOG_ERROR("Failed to enable interrupt for KEY_PLAY_PAUSE_PIN");
    }
    if (aml_gpio_enable_interrupt(KEY_VOL_UP_PIN, true) != 0) {
        LOG_ERROR("Failed to enable interrupt for KEY_VOL_UP_PIN");
    }
    if (aml_gpio_enable_interrupt(KEY_VOL_DOWN_PIN, true) != 0) {
        LOG_ERROR("Failed to enable interrupt for KEY_VOL_DOWN_PIN");
    }
    if (aml_gpio_enable_interrupt(KEY_SOURCE_PIN, true) != 0) {
        LOG_ERROR("Failed to enable interrupt for KEY_SOURCE_PIN");
    }
    
#elif (CURRENT_PRODUCT_TYPE == PRODUCT_MID_END) || (CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END)
    // 中/高端产品：物理按键+红外遥控全开
    LOG_INFO("Peripheral: Key + IR remote init success");
    
    // 初始化按键GPIO（同上）
    if (aml_gpio_set_direction(KEY_PLAY_PAUSE_PIN, GPIO_DIR_INPUT) != 0) {
        LOG_ERROR("Failed to set direction for KEY_PLAY_PAUSE_PIN");
    }
    if (aml_gpio_set_direction(KEY_VOL_UP_PIN, GPIO_DIR_INPUT) != 0) {
        LOG_ERROR("Failed to set direction for KEY_VOL_UP_PIN");
    }
    if (aml_gpio_set_direction(KEY_VOL_DOWN_PIN, GPIO_DIR_INPUT) != 0) {
        LOG_ERROR("Failed to set direction for KEY_VOL_DOWN_PIN");
    }
    if (aml_gpio_set_direction(KEY_SOURCE_PIN, GPIO_DIR_INPUT) != 0) {
        LOG_ERROR("Failed to set direction for KEY_SOURCE_PIN");
    }
    if (aml_gpio_set_direction(KEY_SOUND_MODE_PIN, GPIO_DIR_INPUT) != 0) {
        LOG_ERROR("Failed to set direction for KEY_SOUND_MODE_PIN");
    }
    
    // 注册按键中断回调
    if (aml_gpio_register_interrupt(KEY_PLAY_PAUSE_PIN, GPIO_INT_FALLING, key_interrupt_callback) != 0) {
        LOG_ERROR("Failed to register interrupt for KEY_PLAY_PAUSE_PIN");
    }
    if (aml_gpio_register_interrupt(KEY_VOL_UP_PIN, GPIO_INT_FALLING, key_interrupt_callback) != 0) {
        LOG_ERROR("Failed to register interrupt for KEY_VOL_UP_PIN");
    }
    if (aml_gpio_register_interrupt(KEY_VOL_DOWN_PIN, GPIO_INT_FALLING, key_interrupt_callback) != 0) {
        LOG_ERROR("Failed to register interrupt for KEY_VOL_DOWN_PIN");
    }
    if (aml_gpio_register_interrupt(KEY_SOURCE_PIN, GPIO_INT_FALLING, key_interrupt_callback) != 0) {
        LOG_ERROR("Failed to register interrupt for KEY_SOURCE_PIN");
    }
    if (aml_gpio_register_interrupt(KEY_SOUND_MODE_PIN, GPIO_INT_FALLING, key_interrupt_callback) != 0) {
        LOG_ERROR("Failed to register interrupt for KEY_SOUND_MODE_PIN");
    }
    
    // 启用按键中断
    if (aml_gpio_enable_interrupt(KEY_PLAY_PAUSE_PIN, true) != 0) {
        LOG_ERROR("Failed to enable interrupt for KEY_PLAY_PAUSE_PIN");
    }
    if (aml_gpio_enable_interrupt(KEY_VOL_UP_PIN, true) != 0) {
        LOG_ERROR("Failed to enable interrupt for KEY_VOL_UP_PIN");
    }
    if (aml_gpio_enable_interrupt(KEY_VOL_DOWN_PIN, true) != 0) {
        LOG_ERROR("Failed to enable interrupt for KEY_VOL_DOWN_PIN");
    }
    if (aml_gpio_enable_interrupt(KEY_SOURCE_PIN, true) != 0) {
        LOG_ERROR("Failed to enable interrupt for KEY_SOURCE_PIN");
    }
    if (aml_gpio_enable_interrupt(KEY_SOUND_MODE_PIN, true) != 0) {
        LOG_ERROR("Failed to enable interrupt for KEY_SOUND_MODE_PIN");
    }
    
    // 初始化红外接收器
    if (aml_ir_init() == 0) {
        if (aml_ir_set_callback(ir_receive_callback) != 0) {
            LOG_ERROR("Failed to set IR callback");
        }
        if (aml_ir_enable(true) != 0) {
            LOG_ERROR("Failed to enable IR receiver");
        } else {
            g_ir_enabled = true;
            LOG_INFO("IR receiver initialized");
        }
    } else {
        LOG_ERROR("IR receiver init failed");
    }
    
#else
    // 低音炮产品：无按键无红外
    LOG_INFO("Peripheral: Key/IR not supported for current product");
    g_key_ir_init = false;
    return SUCCESS;
#endif
    
    return SUCCESS;
}

void key_ir_deinit(void)
{
    if (!g_key_ir_init) {
        return;
    }
    
    // 禁用并取消注册按键中断
    if (aml_gpio_enable_interrupt(KEY_PLAY_PAUSE_PIN, false) != 0) {
        LOG_ERROR("Failed to disable interrupt for KEY_PLAY_PAUSE_PIN");
    }
    if (aml_gpio_enable_interrupt(KEY_VOL_UP_PIN, false) != 0) {
        LOG_ERROR("Failed to disable interrupt for KEY_VOL_UP_PIN");
    }
    if (aml_gpio_enable_interrupt(KEY_VOL_DOWN_PIN, false) != 0) {
        LOG_ERROR("Failed to disable interrupt for KEY_VOL_DOWN_PIN");
    }
    if (aml_gpio_enable_interrupt(KEY_SOURCE_PIN, false) != 0) {
        LOG_ERROR("Failed to disable interrupt for KEY_SOURCE_PIN");
    }
    if (aml_gpio_enable_interrupt(KEY_SOUND_MODE_PIN, false) != 0) {
        LOG_ERROR("Failed to disable interrupt for KEY_SOUND_MODE_PIN");
    }
    
    if (aml_gpio_unregister_interrupt(KEY_PLAY_PAUSE_PIN) != 0) {
        LOG_ERROR("Failed to unregister interrupt for KEY_PLAY_PAUSE_PIN");
    }
    if (aml_gpio_unregister_interrupt(KEY_VOL_UP_PIN) != 0) {
        LOG_ERROR("Failed to unregister interrupt for KEY_VOL_UP_PIN");
    }
    if (aml_gpio_unregister_interrupt(KEY_VOL_DOWN_PIN) != 0) {
        LOG_ERROR("Failed to unregister interrupt for KEY_VOL_DOWN_PIN");
    }
    if (aml_gpio_unregister_interrupt(KEY_SOURCE_PIN) != 0) {
        LOG_ERROR("Failed to unregister interrupt for KEY_SOURCE_PIN");
    }
    if (aml_gpio_unregister_interrupt(KEY_SOUND_MODE_PIN) != 0) {
        LOG_ERROR("Failed to unregister interrupt for KEY_SOUND_MODE_PIN");
    }
    
    // 反初始化红外接收器
    if (g_ir_enabled) {
        if (aml_ir_enable(false) != 0) {
            LOG_ERROR("Failed to disable IR receiver");
        }
        if (aml_ir_deinit() != 0) {
            LOG_ERROR("Failed to deinitialize IR receiver");
        }
        g_ir_enabled = false;
    }
    
    // 停止红外学习
    if (g_ir_learning) {
        if (key_ir_ir_learn_stop() != 0) {
            LOG_ERROR("Failed to stop IR learning");
        }
    }
    
    g_key_ir_init = false;
    g_last_key_event = KEY_EVENT_NONE;
    
    LOG_INFO("Key/IR deinitialized");
}

/**
 * @brief 按键/红外事件轮询
 */
void key_ir_event_poll(void)
{
    if (!g_key_ir_init) {
        return;
    }
    
    // 轮询红外事件
    if (g_ir_enabled) {
        if (aml_ir_event_poll() != 0) {
            LOG_ERROR("Failed to poll IR events");
        }
    }
    
    // 通过UART接收MCU发送的按键/红外状态指令
    uint8_t rx_buf[MAX_PACKET_LEN] = {0};
    int recv_len = uart_mcu_recv_data(rx_buf, MAX_PACKET_LEN, 10); // 10ms超时
    
    if (recv_len > 0) {
        LOG_DEBUG("Received %d bytes from MCU", recv_len);
        
        // 解析接收到的数据包
        uint8_t cmd = 0;
        uint8_t data[MAX_PACKET_LEN] = {0};
        int data_len = 0;
        
        int ret = uart_unpack_data(rx_buf, recv_len, &cmd, data, &data_len);
        if (ret != UART_ERR_NONE) {
            LOG_ERROR("Failed to unpack key/IR data: %d", ret);
            return;
        }
        
        // 处理按键/红外状态响应
        if (cmd == CMD_KEY_STATUS_RESP) {
            if (data_len >= 1) {
                uint8_t key_state = data[0];
                LOG_DEBUG("Received key state from MCU: 0x%02X", key_state);
                
                // 解析按键状态并转换为事件
                KeyEvent_e event = KEY_EVENT_NONE;
                
                if (key_state & KEY_BIT_PLAY_PAUSE) {
                    event = KEY_EVENT_PLAY_PAUSE;
                } else if (key_state & KEY_BIT_VOL_UP) {
                    event = KEY_EVENT_VOL_UP;
                } else if (key_state & KEY_BIT_VOL_DOWN) {
                    event = KEY_EVENT_VOL_DOWN;
                } else if (key_state & KEY_BIT_SOURCE_SWITCH) {
                    event = KEY_EVENT_SOURCE_SWITCH;
                } else if (key_state & KEY_BIT_SOUND_MODE) {
                    event = KEY_EVENT_SOUND_MODE;
                } else if (key_state & KEY_BIT_BASS_UP) {
                    event = KEY_EVENT_BASS_UP;
                } else if (key_state & KEY_BIT_TREBLE_UP) {
                    event = KEY_EVENT_TREBLE_UP;
                } else if (key_state & KEY_BIT_IR_LEARN) {
                    event = KEY_EVENT_IR_LEARN;
                }
                
                if (event != KEY_EVENT_NONE) {
                    g_last_key_event = event;
                    LOG_INFO("Key event from MCU: %d", event);
                }
            } else {
                LOG_ERROR("Invalid data length for CMD_KEY_STATUS_RESP: %d", data_len);
            }
        } else {
            LOG_WARN("Unexpected command from MCU: 0x%02X", cmd);
        }
    } else if (recv_len < 0) {
        LOG_ERROR("Failed to receive data from MCU: %d", recv_len);
    }
    
    // 备用：轮询本地GPIO按键状态（当MCU通信失败时使用）
    static uint8_t last_gpio_state = 0xFF;
    uint8_t current_gpio_state = 0;
    
    // 读取所有按键GPIO状态
    int play_pause_val = aml_gpio_get_value(KEY_PLAY_PAUSE_PIN);
    int vol_up_val = aml_gpio_get_value(KEY_VOL_UP_PIN);
    int vol_down_val = aml_gpio_get_value(KEY_VOL_DOWN_PIN);
    int source_val = aml_gpio_get_value(KEY_SOURCE_PIN);
    int sound_mode_val = aml_gpio_get_value(KEY_SOUND_MODE_PIN);
    
    if (play_pause_val >= 0) current_gpio_state |= (play_pause_val << 0);
    if (vol_up_val >= 0) current_gpio_state |= (vol_up_val << 1);
    if (vol_down_val >= 0) current_gpio_state |= (vol_down_val << 2);
    if (source_val >= 0) current_gpio_state |= (source_val << 3);
    if (sound_mode_val >= 0) current_gpio_state |= (sound_mode_val << 4);
    
    // 检测按键状态变化（下降沿触发）
    uint8_t key_changed = last_gpio_state & (~current_gpio_state);
    
    if (key_changed != 0) {
        // 防抖处理
        aml_timer_delay_ms(KEY_DEBOUNCE_MS);
        
        // 再次读取状态确认
        uint8_t confirm_state = 0;
        play_pause_val = aml_gpio_get_value(KEY_PLAY_PAUSE_PIN);
        vol_up_val = aml_gpio_get_value(KEY_VOL_UP_PIN);
        vol_down_val = aml_gpio_get_value(KEY_VOL_DOWN_PIN);
        source_val = aml_gpio_get_value(KEY_SOURCE_PIN);
        sound_mode_val = aml_gpio_get_value(KEY_SOUND_MODE_PIN);
        
        if (play_pause_val >= 0) confirm_state |= (play_pause_val << 0);
        if (vol_up_val >= 0) confirm_state |= (vol_up_val << 1);
        if (vol_down_val >= 0) confirm_state |= (vol_down_val << 2);
        if (source_val >= 0) confirm_state |= (source_val << 3);
        if (sound_mode_val >= 0) confirm_state |= (sound_mode_val << 4);
        
        // 确认按键确实被按下
        key_changed = last_gpio_state & (~confirm_state);
        
        if (key_changed != 0) {
            KeyEvent_e event = KEY_EVENT_NONE;
            
            if (key_changed & (1 << 0)) {
                event = KEY_EVENT_PLAY_PAUSE;
            } else if (key_changed & (1 << 1)) {
                event = KEY_EVENT_VOL_UP;
            } else if (key_changed & (1 << 2)) {
                event = KEY_EVENT_VOL_DOWN;
            } else if (key_changed & (1 << 3)) {
                event = KEY_EVENT_SOURCE_SWITCH;
            } else if (key_changed & (1 << 4)) {
                event = KEY_EVENT_SOUND_MODE;
            }
            
            if (event != KEY_EVENT_NONE) {
                g_last_key_event = event;
                LOG_INFO("Key event (local GPIO): %d", event);
            }
        }
    }
    
    last_gpio_state = current_gpio_state;
}

/**
 * @brief 获取按键/红外事件
 */
KeyEvent_e key_ir_get_event(void)
{
    if (!g_key_ir_init) {
        return KEY_EVENT_NONE;
    }
    
    KeyEvent_e event = g_last_key_event;
    g_last_key_event = KEY_EVENT_NONE; // 清除事件
    
    return event;
}

/**
 * @brief 开始红外学习
 */
int key_ir_ir_learn_start(void)
{
    if (!g_key_ir_init || !g_ir_enabled) {
        LOG_ERROR("IR learning not supported");
        return -1;
    }
    
    if (g_ir_learning) {
        LOG_INFO("IR learning already started");
        return 0;
    }
    
    if (aml_ir_start_learning() == 0) {
        g_ir_learning = true;
        LOG_INFO("IR learning started");
        return 0;
    } else {
        LOG_ERROR("IR learning start failed");
        return -1;
    }
}

/**
 * @brief 停止红外学习
 */
int key_ir_ir_learn_stop(void)
{
    if (!g_key_ir_init || !g_ir_enabled || !g_ir_learning) {
        LOG_INFO("IR learning not running");
        return 0;
    }
    
    if (aml_ir_stop_learning() == 0) {
        g_ir_learning = false;
        LOG_INFO("IR learning stopped");
        return 0;
    } else {
        LOG_ERROR("IR learning stop failed");
        return -1;
    }
}