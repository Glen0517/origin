#include "peripheral_priv.h"
#include "logger.h"
#include "product_type.h"
#include "res_manager.h"

static int g_prompt_init = 0;

int prompt_sound_init(void)
{
    g_prompt_init = 1;
#if (CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END)
    LOG_INFO("Peripheral: Prompt sound init (full resource load)");
#elif (CURRENT_PRODUCT_TYPE == PRODUCT_MID_END)
    LOG_INFO("Peripheral: Prompt sound init (mid resource load)");
#else
    LOG_INFO("Peripheral: Prompt sound init (basic resource load) [ALL PRODUCT]");
#endif
    return 0;
}

void prompt_sound_deinit(void)
{
    g_prompt_init = 0;
}