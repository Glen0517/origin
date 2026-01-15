#include "audio_core_priv.h"
#include "logger.h"
#include "product_type.h"

static AudioMixerCfg_t g_mixer_cfg = {0};

int audio_mixer_init(void) {
    memset(&g_mixer_cfg, 0, sizeof(AudioMixerCfg_t));
    g_mixer_cfg.gain = 80;

#if (CURRENT_PRODUCT_TYPE == PRODUCT_LOW_END) || (CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER)
    g_mixer_cfg.channels = AUDIO_CHANNEL_MONO;
    g_mixer_cfg.sample_rate = AUDIO_SAMPLE_RATE_44100;
#elif (CURRENT_PRODUCT_TYPE == PRODUCT_MID_END)
    g_mixer_cfg.channels = AUDIO_CHANNEL_STEREO;
    g_mixer_cfg.sample_rate = AUDIO_SAMPLE_RATE_48000;
#else
    g_mixer_cfg.channels = AUDIO_CHANNEL_5_1;
    g_mixer_cfg.sample_rate = AUDIO_SAMPLE_RATE_96000;
#endif

    LOG_INFO("Audio mixer: %dCH, %dkHz, gain=%d", 
             g_mixer_cfg.channels, g_mixer_cfg.sample_rate/1000, g_mixer_cfg.gain);
    g_mixer_cfg.init_ok = 1;
    return 0;
}

void audio_mixer_deinit(void) {
    if (g_mixer_cfg.init_ok) {
        memset(&g_mixer_cfg, 0, sizeof(AudioMixerCfg_t));
    }
}