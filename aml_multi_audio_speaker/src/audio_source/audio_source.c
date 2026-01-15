#include "audio_source.h"
#include "audio_source_priv.h"
#include "logger.h"
#include "product_type.h"

static AudioSource_t g_audio_src = {0};
static const int g_base_src[] = {AUDIO_SOURCE_BT, AUDIO_SOURCE_USB};

int audio_source_init(void) {
    memset(&g_audio_src, 0, sizeof(AudioSource_t));
    g_audio_src.cur_source = AUDIO_SOURCE_BT;
    g_audio_src.src_count = sizeof(g_base_src)/sizeof(int);
    memcpy(g_audio_src.support_src, g_base_src, sizeof(g_base_src));

    // 基础音源（必加载）
    src_bt_init();
    src_usb_init();

    // 宏控加载音源
#ifdef CONFIG_ENABLE_HDMI_ARC
    g_audio_src.support_src[g_audio_src.src_count++] = AUDIO_SOURCE_HDMI;
    src_hdmi_init();
#endif
#ifdef CONFIG_ENABLE_SPDIF
    g_audio_src.support_src[g_audio_src.src_count++] = AUDIO_SOURCE_SPDIF;
    src_spdif_init();
#endif
#ifdef CONFIG_ENABLE_AUX
    g_audio_src.support_src[g_audio_src.src_count++] = AUDIO_SOURCE_AUX;
    src_aux_init();
#endif
#ifdef CONFIG_ENABLE_UAC
    g_audio_src.support_src[g_audio_src.src_count++] = AUDIO_SOURCE_UAC;
    src_uac_init();
#endif
#ifdef CONFIG_ENABLE_WIFI_MEDIA
    g_audio_src.support_src[g_audio_src.src_count++] = AUDIO_SOURCE_WIFI;
    src_wifi_init();
#endif

    g_audio_src.init_ok = 1;
    LOG_INFO("Audio source init: cur=%d, support=%d", 
             g_audio_src.cur_source, g_audio_src.src_count);
    return 0;
}

void audio_source_deinit(void) {
    if (g_audio_src.init_ok) {
#ifdef CONFIG_ENABLE_WIFI_MEDIA
        src_wifi_deinit();
#endif
#ifdef CONFIG_ENABLE_UAC
        src_uac_deinit();
#endif
#ifdef CONFIG_ENABLE_AUX
        src_aux_deinit();
#endif
#ifdef CONFIG_ENABLE_SPDIF
        src_spdif_deinit();
#endif
#ifdef CONFIG_ENABLE_HDMI_ARC
        src_hdmi_deinit();
#endif
        src_usb_deinit();
        src_bt_deinit();
        g_audio_src.init_ok = 0;
        LOG_INFO("Audio source deinit success");
    }
}

int audio_source_switch(int source) {
    if (!g_audio_src.init_ok || source < AUDIO_SOURCE_BT || source > AUDIO_SOURCE_WIFI) {
        LOG_ERROR("Source switch failed: invalid source=%d", source);
        return -1;
    }
    // 校验是否支持
    int i;
    for (i = 0; i < g_audio_src.src_count; i++) {
        if (g_audio_src.support_src[i] == source) break;
    }
    if (i >= g_audio_src.src_count) {
        LOG_ERROR("Source %d not supported", source);
        return -1;
    }
    g_audio_src.cur_source = source;
    LOG_INFO("Switch source to: %d", source);
    return 0;
}

void audio_source_event_poll(void) {
    if (!g_audio_src.init_ok) return;
    // 轮询音源热插拔事件
}

int audio_source_get_current(void) {
    return g_audio_src.init_ok ? g_audio_src.cur_source : AUDIO_SOURCE_BT;
}