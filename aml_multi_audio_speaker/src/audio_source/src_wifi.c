#include "audio_source_priv.h"
#include "logger.h"
#include "product_type.h"

// WIFI仅高端产品支持
#ifdef CONFIG_ENABLE_WIFI_MEDIA
int src_wifi_init(void) {
    LOG_INFO("WIFI (cast/network) source init success");
    return 0;
}

void src_wifi_deinit(void) {}
#else
int src_wifi_init(void) { return 0; }
void src_wifi_deinit(void) {}
#endif