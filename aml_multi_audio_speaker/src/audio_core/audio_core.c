#include "audio_core.h"
#include "audio_core_priv.h"
#include "logger.h"
#include "product_type.h"

#include "hal.h"  // 硬件抽象层

static AudioCoreConfig_t g_audio_cfg = {0};

int audio_core_init(AudioCoreConfig_t *cfg) {
    memset(&g_audio_cfg, 0, sizeof(g_audio_cfg));
    if (cfg != NULL) {
        g_audio_cfg.sample_rate = cfg->sample_rate;
        g_audio_cfg.channel_num = cfg->channel_num;
        g_audio_cfg.pcm_buffer_size = cfg->pcm_buffer_size;
        g_audio_cfg.hw_decode_en = cfg->hw_decode_en;
        g_audio_cfg.dolby_dts_en = cfg->dolby_dts_en;
    } else {
        // 使用默认配置
        g_audio_cfg.sample_rate = 48000;
        g_audio_cfg.channel_num = 2;
        g_audio_cfg.pcm_buffer_size = 1024 * 64;
        g_audio_cfg.hw_decode_en = false;
        g_audio_cfg.dolby_dts_en = false;
    }
    
    // 1. 初始化音频硬件（通过HAL层）
    if (hal_audio_init() != 0) {
        LOG_ERROR("HAL audio init failed");
        return FAILURE;
    }
    
    // 2. 配置音频参数（通过HAL层）
    hal_audio_set_sample_rate(g_audio_cfg.sample_rate);
    hal_audio_set_channels(g_audio_cfg.channel_num);
    hal_audio_set_hw_decode(g_audio_cfg.hw_decode_en);
    hal_audio_set_dolby_dts(g_audio_cfg.dolby_dts_en);
    
    // 3. 调用内部子功能初始化
    audio_decode_init();
    audio_mixer_init();
    audio_ringbuf_init(g_audio_cfg.pcm_buffer_size);
    
    g_audio_cfg.init_ok = 1;
    LOG_INFO("Audio core init success! Sample Rate: %d, Channels: %d", 
             g_audio_cfg.sample_rate, g_audio_cfg.channel_num);
    LOG_INFO("  HW Decode: %s, Dolby/DTS: %s", 
             g_audio_cfg.hw_decode_en ? "ON" : "OFF", 
             g_audio_cfg.dolby_dts_en ? "ON" : "OFF");
    return SUCCESS;
}

int audio_core_deinit(void) {
    if (g_audio_cfg.init_ok) {
        // 停止当前音频播放（通过HAL层）
        hal_audio_stop();
        
        // 反初始化内部子功能
        audio_ringbuf_deinit();
        audio_mixer_deinit();
        audio_decode_deinit();
        
        // 反初始化音频硬件（通过HAL层）
        hal_audio_deinit();
        
        g_audio_cfg.init_ok = 0;
        LOG_INFO("Audio core deinit success!");
    }
    return SUCCESS;
}

int audio_core_play_pcm(uint8_t *pcm_buf, int buf_len) {
    if (!g_audio_cfg.init_ok || !pcm_buf || buf_len <= 0) {
        LOG_ERROR("Play PCM failed: invalid parameters or not initialized");
        return FAILURE;
    }
    unsigned int written = audio_ringbuf_write(pcm_buf, buf_len);
    if (written != buf_len) {
        LOG_WARN("Play PCM: buffer full, only wrote %u bytes", written);
    }
    return SUCCESS;
}

int audio_core_pause(void) {
    if (!g_audio_cfg.init_ok) {
        LOG_ERROR("Pause failed: audio core not initialized");
        return FAILURE;
    }
    
    // 使用HAL层暂停音频播放
    if (hal_audio_pause() != 0) {
        LOG_ERROR("Audio pause failed");
        return FAILURE;
    }
    
    LOG_INFO("Audio pause success");
    return SUCCESS;
}

int audio_core_resume(void) {
    if (!g_audio_cfg.init_ok) {
        LOG_ERROR("Resume failed: audio core not initialized");
        return FAILURE;
    }
    
    // 使用HAL层恢复音频播放
    if (hal_audio_resume() != 0) {
        LOG_ERROR("Audio resume failed");
        return FAILURE;
    }
    
    LOG_INFO("Audio resume success");
    return SUCCESS;
}

int audio_core_stop(void) {
    if (!g_audio_cfg.init_ok) {
        LOG_ERROR("Stop failed: audio core not initialized");
        return FAILURE;
    }
    
    // 使用HAL层停止音频播放
    if (hal_audio_stop() != 0) {
        LOG_ERROR("Audio stop failed");
        return FAILURE;
    }
    
    // 清空环形缓冲区
    audio_ringbuf_deinit();
    audio_ringbuf_init(g_audio_cfg.pcm_buffer_size);
    
    LOG_INFO("Audio stop success");
    return SUCCESS;
}

int audio_core_err_recover(void) {
    if (!g_audio_cfg.init_ok) {
        LOG_ERROR("Error recovery failed: audio core not initialized");
        return FAILURE;
    }
    
    LOG_INFO("Audio error recovery started");
    
    // 1. 停止当前音频播放（通过HAL层）
    hal_audio_stop();
    
    // 2. 清空环形缓冲区
    audio_ringbuf_deinit();
    audio_ringbuf_init(g_audio_cfg.pcm_buffer_size);
    
    // 3. 重新初始化音频解码
    audio_decode_deinit();
    audio_decode_init();
    
    // 4. 重新初始化混音器
    audio_mixer_deinit();
    audio_mixer_init();
    
    // 5. 重启音频驱动（通过HAL层）
    hal_audio_deinit();
    hal_audio_init();
    
    LOG_INFO("Audio error recovery completed");
    return SUCCESS;
}

int audio_core_set_volume(int vol) {
    if (!g_audio_cfg.init_ok) {
        LOG_ERROR("Set volume failed: audio core not initialized");
        return FAILURE;
    }
    if (vol < MIN_VOLUME_VAL || vol > MAX_VOLUME_VAL) {
        LOG_ERROR("Set volume failed: invalid volume value %d", vol);
        return FAILURE;
    }
    
    // 使用HAL层设置音量
    if (hal_audio_set_volume(vol) != 0) {
        LOG_ERROR("Set volume failed");
        return FAILURE;
    }
    
    LOG_INFO("Set volume to %d", vol);
    return SUCCESS;
}

/**
 * @brief  播放提示音
 * @param  tone_data 提示音数据缓冲区
 * @param  tone_size 提示音数据长度
 * @return SUCCESS/FAILURE
 */
int audio_core_play_tone(unsigned char *tone_data, unsigned int tone_size) {
    if (!g_audio_cfg.init_ok || !tone_data || tone_size == 0) {
        LOG_ERROR("Play tone failed: invalid parameters or not initialized");
        return FAILURE;
    }
    unsigned int written = audio_ringbuf_write(tone_data, tone_size);
    if (written != tone_size) {
        LOG_WARN("Play tone: buffer full, only wrote %u bytes", written);
    }
    return SUCCESS;
}

// 内部使用的函数，不需要对外暴露
static AudioCoreConfig_t *audio_core_get_config(void) {
    return &g_audio_cfg;
}