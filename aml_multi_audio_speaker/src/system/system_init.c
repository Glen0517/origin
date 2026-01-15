#include "system_priv.h"
#include "logger.h"

static int g_sys_core_init = 0;

int sys_init_core(void)
{
    g_sys_core_init = 1;
    LOG_INFO("System: Core init success [ALL PRODUCT NO MACRO]");
    return 0;
}

void sys_init_deinit(void)
{
    g_sys_core_init = 0;
}