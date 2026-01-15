#include "prod_test_priv.h"
#include "logger.h"
#include "product_type.h"

static int g_hw_calib_init = 0;

#ifdef CONFIG_ENABLE_HW_CALIB
int hw_calib_init(void)
{
    g_hw_calib_init = 1;
    LOG_INFO("Prod test: HW calib/aging init success (HIGH END ONLY)");
    return 0;
}

void hw_calib_deinit(void)
{
    g_hw_calib_init = 0;
}
#else
int hw_calib_init(void) { return 0; }
void hw_calib_deinit(void) {}
#endif