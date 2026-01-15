#include "storage_priv.h"
#include "logger.h"

static int g_usb_mount_init = 0;

int usb_mount_init(void)
{
    g_usb_mount_init = 1;
    LOG_INFO("Storage: USB mount/umount/Hotplug init success");
    return 0;
}

void usb_mount_deinit(void)
{
    g_usb_mount_init = 0;
}