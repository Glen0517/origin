#include "audio_core_priv.h"
#include "logger.h"
#include "product_type.h"

static AudioDecodeCfg_t g_decode_cfg = {0};

int audio_decode_init(void) {
    memset(&g_decode_cfg, 0, sizeof(AudioDecodeCfg_t));
    g_decode_cfg.type = AUDIO_DECODE_MP3;
#ifdef CONFIG_ENABLE_DOLBY_DTS
    g_decode_cfg.dolby_en = 1;
    g_decode_cfg.dts_en = 1;
    LOG_INFO("Audio decode: PCM+MP3+AAC (Dolby/DTS ENABLE)");
#else
    LOG_INFO("Audio decode: PCM+MP3 (Dolby/DTS DISABLE)");
#endif
    g_decode_cfg.init_ok = 1;
    return 0;
}

void audio_decode_deinit(void) {
    if (g_decode_cfg.init_ok) {
        memset(&g_decode_cfg, 0, sizeof(AudioDecodeCfg_t));
    }
}