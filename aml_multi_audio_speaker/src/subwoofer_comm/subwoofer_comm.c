#include "subwoofer_comm.h"
#include "logger.h"
#include "product_type.h"

#ifdef CONFIG_ENABLE_BT_MESH
int subwoofer_comm_init(void)
{
    LOG_INFO("Subwoofer comm module init success (HIGH END + SUBWOOFER)");
    return 0;
}

void subwoofer_comm_deinit(void)
{
    LOG_INFO("Subwoofer comm module deinit success");
}

void subwoofer_comm_event_poll(void) {}
#else
int subwoofer_comm_init(void) { return 0; }
void subwoofer_comm_deinit(void) {}
void subwoofer_comm_event_poll(void) {}
#endif