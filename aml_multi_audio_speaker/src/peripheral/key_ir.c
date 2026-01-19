#include "peripheral_priv.h"
#include "logger.h"
#include "product_type.h"
#include "common_def.h"

#include "comm_mcu.h"        // MCU通信接口

bool g_key_ir_init = false;
static bool g_ir_enabled = false;
static bool g_ir_learning = false;
KeyEvent_e g_last_key_event = KEY_EVENT_NONE;

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
    
#elif (CURRENT_PRODUCT_TYPE == PRODUCT_MID_END) || (CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END)
    // 中/高端产品：物理按键+红外遥控全开
    LOG_INFO("Peripheral: Key + IR remote init success");
    g_ir_enabled = true;
    
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
    
    // 通过UART查询MCU的按键/红外状态
    comm_mcu_query_key_status();
    
    // 注意：按键/红外状态的响应会在UART接收回调中处理
    // 具体处理逻辑在uart_txrx.c中的uart_rx_callback函数
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
    
    // 这里可以通过UART发送红外学习开始命令给MCU
    // 目前暂未实现，需要根据实际协议扩展
    g_ir_learning = true;
    LOG_INFO("IR learning started");
    
    return 0;
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
    
    // 这里可以通过UART发送红外学习停止命令给MCU
    // 目前暂未实现，需要根据实际协议扩展
    g_ir_learning = false;
    LOG_INFO("IR learning stopped");
    
    return 0;
}