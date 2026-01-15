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
    // 轮询蓝牙连接/断开/播放事件
}