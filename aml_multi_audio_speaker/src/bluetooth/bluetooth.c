#include "bluetooth.h"
#include "bluetooth_priv.h"
#include "logger.h"
#include "event.h"
#include "hal.h"  // 硬件抽象层

static BtCfg_t g_bt_cfg = {0};

int bluetooth_init(void)
{
    memset(&g_bt_cfg, 0, sizeof(BtCfg_t));
    // 必加载：所有产品都有蓝牙A2DP基础功能
    if (hal_bt_init() != 0) {
        LOG_ERROR("HAL bluetooth init failed");
        return FAILURE;
    }
    // 宏控加载：仅高端+低音炮支持MESH组网
#ifdef CONFIG_ENABLE_BT_MESH
    bt_mesh_init();
    g_bt_cfg.mesh_en = 1;
#endif
    g_bt_cfg.init_ok = 1;
    g_bt_cfg.bt_enable = true;
    LOG_INFO("Bluetooth module init success (MESH: %d)", g_bt_cfg.mesh_en);
    return SUCCESS;
}

void bluetooth_deinit(void)
{
    if (g_bt_cfg.init_ok)
    {
#ifdef CONFIG_ENABLE_BT_MESH
        bt_mesh_deinit();
#endif
        if (hal_bt_deinit() != 0) {
            LOG_ERROR("HAL bluetooth deinit failed");
        }
        g_bt_cfg.init_ok = 0;
        g_bt_cfg.bt_enable = false;
        LOG_INFO("Bluetooth module deinit success");
    }
}

void bluetooth_event_poll(void)
{
    if (!g_bt_cfg.init_ok) return;
    
    // 轮询蓝牙连接状态
        if (g_bt_cfg.bt_enable) {
            // 获取蓝牙连接状态 - 检查蓝牙设备是否已连接
            // 返回值：1表示已连接，0表示未连接
            int conn_status = hal_bt_get_connection_status();
        
        if (conn_status != g_bt_cfg.bt_connected) {
            g_bt_cfg.bt_connected = conn_status;
            
            if (conn_status) {
                LOG_INFO("Bluetooth connected");
                // 发送蓝牙连接通知
                // 更新LED状态
                led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_ON);
                // 更新LCD显示
                char lcd_msg[32] = {0};
                snprintf(lcd_msg, sizeof(lcd_msg), "BT: Connected");
                lcd_display_text(0, 0, lcd_msg);
                // 发送事件给其他模块
                event_notify(EVENT_BT_CONNECTED, NULL);
            } else {
                LOG_INFO("Bluetooth disconnected");
                // 发送蓝牙断开通知
                // 更新LED状态
                led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_OFF);
                // 更新LCD显示
                lcd_display_text(0, 0, "BT: Disconnected");
                // 发送事件给其他模块
                event_notify(EVENT_BT_DISCONNECTED, NULL);
            }
        }
        
        // 轮询蓝牙音频流状态
        if (conn_status && g_bt_cfg.bt_media_enable) {
            // 获取蓝牙A2DP媒体状态 - 检查蓝牙音频流是否正在播放
            // 返回值：1表示正在播放，0表示停止
            int media_status = hal_bt_a2dp_get_media_status();
            
            if (media_status != g_bt_cfg.bt_media_playing) {
                g_bt_cfg.bt_media_playing = media_status;
                
                if (media_status) {
                    LOG_INFO("Bluetooth media playing");
                    // 处理蓝牙媒体播放开始
                    // 更新LED状态
                    led_ctrl_set_state(LED_PLAY, LED_STATE_ON);
                    // 更新LCD显示
                    lcd_display_text(1, 0, "BT: Playing");
                    // 发送事件给其他模块
                    event_notify(EVENT_BT_MEDIA_START, NULL);
                } else {
                    LOG_INFO("Bluetooth media stopped");
                    // 处理蓝牙媒体播放停止
                    // 更新LED状态
                    led_ctrl_set_state(LED_PLAY, LED_STATE_OFF);
                    // 更新LCD显示
                    lcd_display_text(1, 0, "BT: Stopped");
                    // 发送事件给其他模块
                    event_notify(EVENT_BT_MEDIA_STOP, NULL);
                }
            }
        }
        
        // 轮询蓝牙事件 - 处理蓝牙相关的所有事件
        // 通过HAL层统一处理蓝牙A2DP、HFP和MESH事件
        hal_bt_event_poll();
    }
}