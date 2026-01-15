#include "audio_source_priv.h"
#include "logger.h"
#include "product_type.h"

// UAC仅高端产品支持
#ifdef CONFIG_ENABLE_UAC
int src_uac_init(void) {
    LOG_INFO("UAC (USB Audio Class) source init success");
    return 0;
}

void src_uac_deinit(void) {}
#else
int src_uac_init(void) { return 0; }
void src_uac_deinit(void) {}
#endif