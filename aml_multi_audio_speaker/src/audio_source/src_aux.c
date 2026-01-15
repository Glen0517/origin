#include "audio_source_priv.h"
#include "logger.h"
#include "product_type.h"

#ifdef CONFIG_ENABLE_AUX
int src_aux_init(void) {
    LOG_INFO("AUX line-in source init success");
    return 0;
}

void src_aux_deinit(void) {}
#else
int src_aux_init(void) { return 0; }
void src_aux_deinit(void) {}
#endif