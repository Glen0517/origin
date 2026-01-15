#include "audio_core.h"
#include "audio_core_priv.h"
#include "logger.h"
#include "product_type.h"

static AudioCoreConfig_t g_audio_cfg = {0};

int audio_core_init(void) {
    memset(&g_audio_cfg, 0, sizeof(g_audio_cfg));
    // 调用内部子功能初始化
    audio_decode_init();
    audio_mixer_init();
    audio_ringbuf_init(1024 * 64); // 64KB环形缓冲区
    g_audio_cfg.init_ok = 1;
    LOG_INFO("Audio core init success!");
    return 0;
}

void audio_core_deinit(void) {
    if (g_audio_cfg.init_ok) {
        audio_ringbuf_deinit();
        audio_mixer_deinit();
        audio_decode_deinit();
        g_audio_cfg.init_ok = 0;
        LOG_INFO("Audio core deinit success!");
    }
}

int audio_core_play_tone(unsigned char *tone_data, unsigned int tone_size) {
    if (!g_audio_cfg.init_ok || !tone_data || tone_size == 0) return -1;
    return audio_ringbuf_write(tone_data, tone_size);
}

AudioCoreConfig_t *audio_core_get_config(void) {
    return &g_audio_cfg;
}