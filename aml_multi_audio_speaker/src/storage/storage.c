#include "storage.h"
#include "storage_priv.h"
#include "logger.h"

static StorageCfg_t g_storage_cfg = {0};

int storage_init(void)
{
    memset(&g_storage_cfg, 0, sizeof(StorageCfg_t));
    usb_mount_init();
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
        usb_mount_deinit();
        g_storage_cfg.init_ok = 0;
        LOG_INFO("Storage module deinit success");
    }
}