#include "prod_test.h"
#include "prod_test_priv.h"
#include "logger.h"

static ProdTestCfg_t g_test_cfg = {0};

int prod_test_init(void)
{
    memset(&g_test_cfg, 0, sizeof(ProdTestCfg_t));
    // 必加载：基础硬件自检 所有产品通用
    hw_selfcheck_init();
    // 宏控加载：硬件校准/老化测试 仅高端支持
    hw_calib_init();
    g_test_cfg.init_ok = 1;
    LOG_INFO("Product test module init success (CALIB: %d)", CONFIG_ENABLE_HW_CALIB);
    return 0;
}

void prod_test_deinit(void)
{
    if (g_test_cfg.init_ok)
    {
        hw_calib_deinit();
        hw_selfcheck_deinit();
        g_test_cfg.init_ok = 0;
        LOG_INFO("Product test module deinit success");
    }
}