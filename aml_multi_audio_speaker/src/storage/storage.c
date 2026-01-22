#include "storage.h"
#include "storage_priv.h"
#include "logger.h"
#include "pal.h"  // 平台抽象层

// 存储配置结构体
typedef struct {
    int init_ok;          // 初始化状态
    bool media_scanning;   // 媒体扫描状态
    bool file_playing;     // 文件播放状态
} StorageCfg_t;

static StorageCfg_t g_storage_cfg = {0};

int storage_init(void)
{
    memset(&g_storage_cfg, 0, sizeof(StorageCfg_t));
    // 使用PAL层存储服务初始化
    if (pal_storage_init() != 0) {
        LOG_ERROR("PAL storage init failed");
        return FAILURE;
    }
    
    // 初始化子模块，检查错误
    if (file_reader_init() != 0) {
        LOG_ERROR("File reader init failed");
        pal_storage_deinit();
        return FAILURE;
    }
    
    if (media_scan_init() != 0) {
        LOG_ERROR("Media scan init failed");
        file_reader_deinit();
        pal_storage_deinit();
        return FAILURE;
    }
    
    if (usb_mount_init() != 0) {
        LOG_ERROR("USB mount init failed");
        media_scan_deinit();
        file_reader_deinit();
        pal_storage_deinit();
        return FAILURE;
    }
    
    g_storage_cfg.init_ok = 1;
    LOG_INFO("Storage module init success [NO PRODUCT DIFF]");
    return 0;
}

void storage_deinit(void)
{
    if (g_storage_cfg.init_ok)
    {
        file_reader_deinit();
        media_scan_deinit();
        usb_mount_deinit();
        // 使用PAL层存储服务反初始化
        pal_storage_deinit();
        g_storage_cfg.init_ok = 0;
        LOG_INFO("Storage module deinit success");
    }
}

int storage_play_file(const char *file_path)
{
    if (!g_storage_cfg.init_ok) {
        LOG_ERROR("Storage module not initialized");
        return FAILURE;
    }
    return file_reader_play_file(file_path);
}

int storage_pause(void)
{
    if (!g_storage_cfg.init_ok) {
        LOG_ERROR("Storage module not initialized");
        return FAILURE;
    }
    return file_reader_pause();
}

int storage_resume(void)
{
    if (!g_storage_cfg.init_ok) {
        LOG_ERROR("Storage module not initialized");
        return FAILURE;
    }
    return file_reader_resume();
}

int storage_stop(void)
{
    if (!g_storage_cfg.init_ok) {
        LOG_ERROR("Storage module not initialized");
        return FAILURE;
    }
    return file_reader_stop();
}

int storage_set_volume(int volume)
{
    if (!g_storage_cfg.init_ok) {
        LOG_ERROR("Storage module not initialized");
        return FAILURE;
    }
    return file_reader_set_volume(volume);
}

/**
 * @brief 检查存储系统状态
 * @details 检查文件播放状态，处理U盘拔出等错误情况
 */
int storage_check_status(void)
{
    if (!g_storage_cfg.init_ok) {
        LOG_ERROR("Storage module not initialized");
        return FAILURE;
    }
    
    // 检查文件播放状态
    file_reader_check_play_status();
    
    return 0;
}

int storage_scan_media(const char *path)
{
    if (!g_storage_cfg.init_ok) {
        LOG_ERROR("Storage module not initialized");
        return FAILURE;
    }
    return media_scan_scan_path(path);
}

int storage_get_media_count(void)
{
    if (!g_storage_cfg.init_ok) {
        LOG_ERROR("Storage module not initialized");
        return 0;
    }
    return media_scan_get_file_count();
}

const char *storage_get_media_file(int index)
{
    if (!g_storage_cfg.init_ok) {
        LOG_ERROR("Storage module not initialized");
        return NULL;
    }
    return media_scan_get_file_path(index);
}

bool storage_is_playing(void)
{
    if (!g_storage_cfg.init_ok) {
        LOG_ERROR("Storage module not initialized");
        return false;
    }
    return file_reader_is_playing();
}

const char *storage_get_current_file(void)
{
    if (!g_storage_cfg.init_ok) {
        LOG_ERROR("Storage module not initialized");
        return NULL;
    }
    return file_reader_get_current_file();
}