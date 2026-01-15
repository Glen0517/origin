#include "prod_test_priv.h"
#include "logger.h"

static int g_hw_check_init = 0;

int hw_selfcheck_init(void)
{
    g_hw_check_init = 1;
    LOG_INFO("Prod test: HW selfcheck init success [ALL PRODUCT NO MACRO]");
    return 0;
}

void hw_selfcheck_deinit(void)
{
    g_hw_check_init = 0;
}