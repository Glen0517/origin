#include "bluetooth_priv.h"
#include "logger.h"
#include "product_type.h"

static int g_bt_mesh_init = 0;

#ifdef CONFIG_ENABLE_BT_MESH
int bt_mesh_init(void)
{
    g_bt_mesh_init = 1;
    LOG_INFO("Bluetooth: MESH network init success (subwoofer link)");
    return 0;
}

void bt_mesh_deinit(void)
{
    g_bt_mesh_init = 0;
}
#else
int bt_mesh_init(void) { return 0; }
void bt_mesh_deinit(void) {}
#endif