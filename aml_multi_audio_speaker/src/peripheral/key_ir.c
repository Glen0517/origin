#include "peripheral_priv.h"
#include "logger.h"
#include "product_type.h"

static int g_key_ir_init = 0;

int key_ir_init(void)
{
#if (CURRENT_PRODUCT_TYPE == PRODUCT_LOW_END)
    // 低端产品：仅保留物理按键，关闭红外
    g_key_ir_init = 1;
    LOG_INFO("Peripheral: Key init success (IR disable)");
#elif (CURRENT_PRODUCT_TYPE == PRODUCT_MID_END) || (CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END)
    // 中/高端产品：物理按键+红外遥控全开
    g_key_ir_init = 1;
    LOG_INFO("Peripheral: Key + IR remote init success");
#else
    // 低音炮产品：无按键无红外
    g_key_ir_init = 0;
#endif
    return 0;
}

void key_ir_deinit(void)
{
    g_key_ir_init = 0;
}