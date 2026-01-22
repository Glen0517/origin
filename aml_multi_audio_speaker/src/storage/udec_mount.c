#include "storage_priv.h"
#include "logger.h"
#include "event.h"
#include "pal.h"  // 平台抽象层

static int g_usb_mount_init = 0;
static bool g_usb_mounted = false;
static char g_usb_mount_point[32] = {"/mnt/usb"};

/**
 * @brief 获取U盘挂载状态
 */
bool usb_mount_get_status(void)
{
    if (!g_usb_mount_init) {
        return false;
    }
    
    // 使用PAL层检查存储设备是否已挂载
    int status = pal_storage_is_mounted(g_usb_mount_point);
    g_usb_mounted = (status == 1);
    return g_usb_mounted;
}

/**
 * @brief 获取U盘挂载点
 */
const char *usb_mount_get_mount_point(void)
{
    return g_usb_mounted ? g_usb_mount_point : NULL;
}

/**
 * @brief 手动挂载U盘
 */
int usb_mount_mount_device(const char *dev_path)
{
    if (!g_usb_mount_init || !dev_path) {
        return -1;
    }
    
    // 检查是否已经挂载
    if (g_usb_mounted) {
        LOG_WARN("USB device already mounted: %s", g_usb_mount_point);
        return 0;
    }
    
    // 使用PAL层挂载存储设备
    int ret = pal_storage_mount(dev_path, g_usb_mount_point);
    if (ret == 0) {
        g_usb_mounted = true;
        LOG_INFO("USB device mounted: %s -> %s", dev_path, g_usb_mount_point);
        event_notify(EVENT_USB_MOUNTED, (void *)g_usb_mount_point);
    }
    return ret;
}

/**
 * @brief 手动卸载U盘
 */
int usb_mount_umount_device(void)
{
    if (!g_usb_mount_init || !g_usb_mounted) {
        return -1;
    }
    
    // 检查是否有文件正在播放
    if (storage_is_playing()) {
        LOG_WARN("Trying to unmount USB device while file is playing");
        // 停止播放
        storage_stop();
    }
    
    // 使用PAL层卸载存储设备
    int ret = pal_storage_unmount(g_usb_mount_point);
    if (ret == 0) {
        g_usb_mounted = false;
        LOG_INFO("USB device unmounted: %s", g_usb_mount_point);
        event_notify(EVENT_USB_UNMOUNTED, NULL);
    }
    return ret;
}

int usb_mount_init(void)
{
    if (g_usb_mount_init) {
        LOG_INFO("USB mount already initialized");
        return 0;
    }

    g_usb_mount_init = 1;
    g_usb_mounted = false;
    // 保持默认挂载点
    strcpy(g_usb_mount_point, "/mnt/usb");
    
    LOG_INFO("Storage: USB mount/umount init success");
    LOG_INFO("  Default mount point: %s", g_usb_mount_point);
    
    return 0;
}

void usb_mount_deinit(void)
{
    if (g_usb_mount_init) {
        // 如果U盘已挂载，先卸载
        if (g_usb_mounted) {
            pal_storage_unmount(g_usb_mount_point);
        }
        
        g_usb_mount_init = 0;
        g_usb_mounted = false;
        g_usb_mount_point[0] = '\0';
        
        LOG_INFO("USB mount deinitialized");
    }
}