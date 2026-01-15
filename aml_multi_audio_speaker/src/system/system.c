#include "system.h"
#include "system_priv.h"
#include "logger.h"

static SystemCfg_t g_sys_cfg = {0};

int system_init(void)
{
    memset(&g_sys_cfg, 0, sizeof(SystemCfg_t));
    // 必加载：系统基础初始化 所有产品通用
    sys_init_core();
    // 宏控加载：OTA升级功能 分级裁剪
    sys_ota_init();
    g_sys_cfg.init_ok = 1;
    LOG_INFO("System module init success (OTA: %d)", CONFIG_ENABLE_DUAL_OTA);
    return 0;
}

void system_deinit(void)
{
    if (g_sys_cfg.init_ok)
    {
        sys_ota_deinit();
        sys_init_deinit();
        g_sys_cfg.init_ok = 0;
        LOG_INFO("System module deinit success");
    }
}

void system_event_poll(void)
{
    if (!g_sys_cfg.init_ok) return;
    // 轮询系统状态/OTA升级事件
}