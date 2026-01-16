#include "storage.h"
#include "storage_priv.h"
#include "logger.h"
#include "pal.h"  // 平台抽象层

static StorageCfg_t g_storage_cfg = {0};

int storage_init(void)
{
    memset(&g_storage_cfg, 0, sizeof(StorageCfg_t));
    // 使用PAL层存储服务初始化
    if (pal_storage_init() != 0) {
        LOG_ERROR("PAL storage init failed");
        return FAILURE;
    }
    file_reader_init();
    g_storage_cfg.init_ok = 1;
    LOG_INFO("Storage module init success [NO PRODUCT DIFF]");
    return 0;
}

void storage_deinit(void)
{
    if (g_storage_cfg.init_ok)
    {
        file_reader_deinit();
        // 使用PAL层存储服务反初始化
        pal_storage_deinit();
        g_storage_cfg.init_ok = 0;
        LOG_INFO("Storage module deinit success");
    }
}