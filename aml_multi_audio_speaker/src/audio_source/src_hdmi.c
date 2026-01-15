#include "audio_source_priv.h"
#include "logger.h"
#include "product_type.h"

#ifdef CONFIG_ENABLE_HDMI_ARC
int src_hdmi_init(void) {
    LOG_INFO("HDMI ARC source init success");
    return 0;
}

void src_hdmi_deinit(void) {}
#else
int src_hdmi_init(void) { return 0; }
void src_hdmi_deinit(void) {}
#endif