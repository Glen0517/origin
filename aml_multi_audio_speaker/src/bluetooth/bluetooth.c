#include "bluetooth.h"
#include "bluetooth_priv.h"
#include "logger.h"

static BtCfg_t g_bt_cfg = {0};

int bluetooth_init(void)
{
    memset(&g_bt_cfg, 0, sizeof(BtCfg_t));
    // 必加载：所有产品都有蓝牙A2DP基础功能
    bt_a2dp_init();
    // 宏控加载：仅高端+低音炮支持MESH组网
#ifdef CONFIG_ENABLE_BT_MESH
    bt_mesh_init();
    g_bt_cfg.mesh_en = 1;
#endif
    g_bt_cfg.init_ok = 1;
    LOG_INFO("Bluetooth module init success (MESH: %d)", g_bt_cfg.mesh_en);
    return 0;
}

void bluetooth_deinit(void)
{
    if (g_bt_cfg.init_ok)
    {
#ifdef CONFIG_ENABLE_BT_MESH
        bt_mesh_deinit();
#endif
        bt_a2dp_deinit();
        g_bt_cfg.init_ok = 0;
        LOG_INFO("Bluetooth module deinit success");
    }
}

void bluetooth_event_poll(void)
{
    if (!g_bt_cfg.init_ok) return;
    
    // 轮询蓝牙连接状态
    if (g_bt_cfg.bt_enable) {
        int conn_status = aml_bt_get_connection_status();
        
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
            int media_status = aml_bt_a2dp_get_media_status();
            
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
        
        // 轮询蓝牙A2DP事件
        aml_bt_a2dp_event_poll();
        
        // 轮询蓝牙HFP事件
        aml_bt_hfp_event_poll();
        
        // 轮询蓝牙MESH事件（如果启用）
        #ifdef CONFIG_ENABLE_BT_MESH
        aml_bt_mesh_event_poll();
        #endif
    }
}