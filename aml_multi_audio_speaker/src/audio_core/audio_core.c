/**
 * @file audio_core.c
 * @brief 音频核心模块实现
 * @details 负责音频处理、缓冲管理、音频解码和播放控制
 * @author AML Audio Team
 * @date 2026-01-22
 */

#include "audio_core.h"
#include "audio_core_priv.h"
#include "logger.h"
#include "product_type.h"
#include "error_handling.h"  // 统一错误处理
#include "event.h"  // 事件系统

#include "hal.h"  // 硬件抽象层

/**
 * @brief 音频核心配置
 */
static AudioCoreConfig_t g_audio_cfg = {0};

/**
 * @brief 缓冲大小配置
 */
static int g_min_buffer_size = 512 * 32;  // 最小缓冲大小（16KB）
static int g_max_buffer_size = 512 * 128; // 最大缓冲大小（64KB）
static int g_current_buffer_size = 512 * 64; // 当前缓冲大小（32KB）
static int g_buffer_adjust_interval = 1000; // 缓冲调整间隔（毫秒）
static int g_last_adjust_time = 0; // 上次调整时间

/**
 * @brief 缓冲使用历史记录
 */
static int g_buffer_usage_history[10] = {0}; // 缓冲使用历史（最近10次）
static int g_history_index = 0; // 历史记录索引

/**
 * @brief 动态调整缓冲大小
 * @details 根据缓冲使用情况动态调整音频缓冲区大小，以平衡延迟和稳定性
 */
static void adjust_buffer_size(void) {
    if (!g_audio_cfg.init_ok) {
        return;
    }
    
    // 计算当前时间
    int current_time = hal_get_current_time();
    if (current_time - g_last_adjust_time < g_buffer_adjust_interval) {
        return; // 调整间隔未到
    }
    
    // 获取当前缓冲使用情况
    unsigned int used_space = audio_ringbuf_get_used_space();
    unsigned int free_space = audio_ringbuf_get_free_space();
    unsigned int total_space = used_space + free_space;
    
    // 计算缓冲使用率
    int usage_percent = total_space > 0 ? (used_space * 100) / total_space : 0;
    
    // 记录缓冲使用历史
    g_buffer_usage_history[g_history_index] = usage_percent;
    g_history_index = (g_history_index + 1) % 10;
    
    // 计算平均缓冲使用率
    int avg_usage = 0;
    for (int i = 0; i < 10; i++) {
        avg_usage += g_buffer_usage_history[i];
    }
    avg_usage /= 10;
    
    // 根据平均使用率调整缓冲大小
    int new_buffer_size = g_current_buffer_size;
    
    if (avg_usage > 80) {
        // 使用率过高，增加缓冲大小（增加20%）
        new_buffer_size = g_current_buffer_size * 1.2;
        if (new_buffer_size > g_max_buffer_size) {
            new_buffer_size = g_max_buffer_size;
        }
    } else if (avg_usage < 30) {
        // 使用率过低，减少缓冲大小
        new_buffer_size = g_current_buffer_size * 0.8;
        if (new_buffer_size < g_min_buffer_size) {
            new_buffer_size = g_min_buffer_size;
        }
    }
    
    // 如果缓冲大小有变化，重新初始化环形缓冲区
    if (new_buffer_size != g_current_buffer_size) {
        LOG_INFO("Adjusting buffer size from %d to %d (usage: %d%%)", 
                 g_current_buffer_size, new_buffer_size, avg_usage);
        
        g_current_buffer_size = new_buffer_size;
        g_audio_cfg.pcm_buffer_size = new_buffer_size;
        
        // 重新初始化环形缓冲区
        audio_ringbuf_deinit();
        audio_ringbuf_init(new_buffer_size);
    }
    
    g_last_adjust_time = current_time;
}

/**
 * @brief 音频核心模块初始化
 * @param cfg 音频核心配置参数
 * @return SUCCESS表示成功，FAILURE表示失败
 * @details 初始化音频核心模块，包括音频硬件、缓冲区、解码器等
 */
int audio_core_init(AudioCoreConfig_t *cfg) {
    // 初始化配置
    memset(&g_audio_cfg, 0, sizeof(g_audio_cfg));
    
    if (cfg != NULL) {
        // 使用用户提供的配置
        g_audio_cfg.sample_rate = cfg->sample_rate;
        g_audio_cfg.channel_num = cfg->channel_num;
        g_audio_cfg.pcm_buffer_size = cfg->pcm_buffer_size;
        g_current_buffer_size = cfg->pcm_buffer_size;
        g_audio_cfg.hw_decode_en = cfg->hw_decode_en;
        g_audio_cfg.dolby_dts_en = cfg->dolby_dts_en;
        
        LOG_INFO("Audio core init with custom config: sample_rate=%d, channel_num=%d, buffer_size=%d",
                 cfg->sample_rate, cfg->channel_num, cfg->pcm_buffer_size);
    } else {
        // 使用默认配置
        g_audio_cfg.sample_rate = 48000;       // 默认采样率48kHz
        g_audio_cfg.channel_num = 2;            // 默认2声道
        g_audio_cfg.pcm_buffer_size = 1024 * 64; // 默认缓冲区大小
        g_current_buffer_size = 1024 * 64;
        g_audio_cfg.hw_decode_en = false;       // 默认禁用硬件解码
        g_audio_cfg.dolby_dts_en = false;       // 默认禁用Dolby/DTS
        
        LOG_INFO("Audio core init with default config: sample_rate=48000, channel_num=2, buffer_size=65536");
    }
    
    // 1. 初始化音频硬件（通过HAL层）
    LOG_INFO("Initializing audio hardware via HAL...");
    if (hal_audio_init() != 0) {
        LOG_ERROR("HAL audio init failed");
        error_report(ERROR_AUDIO_HAL_INIT, ERROR_LEVEL_ERROR, "audio_core", "HAL audio init failed", 0, NULL);
        return FAILURE;
    }
    
    // 2. 配置音频参数（通过HAL层）
    if (hal_audio_set_sample_rate(g_audio_cfg.sample_rate) != 0) {
        LOG_ERROR("Failed to set sample rate");
        handle_audio_error(AUDIO_ERR_HAL_CONFIG, "Failed to set sample rate");
    }
    if (hal_audio_set_channels(g_audio_cfg.channel_num) != 0) {
        LOG_ERROR("Failed to set channels");
        handle_audio_error(AUDIO_ERR_HAL_CONFIG, "Failed to set channels");
    }
    if (hal_audio_set_hw_decode(g_audio_cfg.hw_decode_en) != 0) {
        LOG_ERROR("Failed to set hardware decode");
        handle_audio_error(AUDIO_ERR_HAL_CONFIG, "Failed to set hardware decode");
    }
    if (hal_audio_set_dolby_dts(g_audio_cfg.dolby_dts_en) != 0) {
        LOG_ERROR("Failed to set Dolby/DTS");
        handle_audio_error(AUDIO_ERR_HAL_CONFIG, "Failed to set Dolby/DTS");
    }
    
    // 3. 调用内部子功能初始化
    if (audio_decode_init() != 0) {
        LOG_ERROR("Failed to initialize audio decode");
        handle_audio_error(AUDIO_ERR_DECODE_INIT, "Failed to initialize audio decode");
    }
    if (audio_mixer_init() != 0) {
        LOG_ERROR("Failed to initialize audio mixer");
        handle_audio_error(AUDIO_ERR_MIXER_INIT, "Failed to initialize audio mixer");
    }
    if (audio_ringbuf_init(g_current_buffer_size) != 0) {
        LOG_ERROR("Failed to initialize ring buffer");
        handle_audio_error(AUDIO_ERR_BUFFER_INIT, "Failed to initialize ring buffer");
    }
    
    // 初始化缓冲使用历史和错误统计
    memset(g_buffer_usage_history, 0, sizeof(g_buffer_usage_history));
    memset(g_audio_error_count, 0, sizeof(g_audio_error_count));
    g_history_index = 0;
    g_last_adjust_time = hal_get_current_time();
    g_last_audio_error = AUDIO_ERR_NONE;
    g_error_recovery_count = 0;
    
    g_audio_cfg.init_ok = 1;
    LOG_INFO("Audio core init success! Sample Rate: %d, Channels: %d", 
             g_audio_cfg.sample_rate, g_audio_cfg.channel_num);
    LOG_INFO("  HW Decode: %s, Dolby/DTS: %s", 
             g_audio_cfg.hw_decode_en ? "ON" : "OFF", 
             g_audio_cfg.dolby_dts_en ? "ON" : "OFF");
    LOG_INFO("  Buffer size: %d (min: %d, max: %d)", 
             g_current_buffer_size, g_min_buffer_size, g_max_buffer_size);
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
        handle_audio_error(AUDIO_ERR_PCM_PLAY, "Invalid parameters or not initialized");
        return FAILURE;
    }
    
    // 动态调整缓冲大小
    adjust_buffer_size();
    
    unsigned int written = audio_ringbuf_write(pcm_buf, buf_len);
    if (written != buf_len) {
        LOG_WARN("Play PCM: buffer full, only wrote %u bytes", written);
        // 缓冲区满，可能需要自动调整
        adjust_buffer_size();
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
    if (audio_ringbuf_init(g_audio_cfg.pcm_buffer_size) != 0) {
        LOG_ERROR("Failed to reinitialize ring buffer during stop");
        handle_audio_error(AUDIO_ERR_BUFFER_INIT, "Failed to reinitialize ring buffer during stop");
    }
    
    LOG_INFO("Audio stop success");
    return SUCCESS;
}

// 错误类型定义
typedef enum {
    AUDIO_ERR_NONE = 0,
    AUDIO_ERR_HAL_INIT,        // HAL初始化错误
    AUDIO_ERR_HAL_CONFIG,      // HAL配置错误
    AUDIO_ERR_DECODE_INIT,      // 解码初始化错误
    AUDIO_ERR_MIXER_INIT,       // 混音器初始化错误
    AUDIO_ERR_BUFFER_INIT,      // 缓冲区初始化错误
    AUDIO_ERR_PCM_PLAY,         // PCM播放错误
    AUDIO_ERR_VOLUME_SET,       // 音量设置错误
    AUDIO_ERR_MAX               // 最大错误类型
} AudioErrorType_e;

// 错误统计
static int g_audio_error_count[AUDIO_ERR_MAX] = {0};
static int g_last_audio_error = AUDIO_ERR_NONE;
static int g_error_recovery_count = 0;

// 错误处理函数
static void handle_audio_error(AudioErrorType_e error_type, const char *error_msg) {
    g_audio_error_count[error_type]++;
    g_last_audio_error = error_type;
    
    LOG_ERROR("Audio error (%d): %s", error_type, error_msg);
    LOG_ERROR("Error count - HAL_INIT: %d, HAL_CONFIG: %d, DECODE_INIT: %d, MIXER_INIT: %d, BUFFER_INIT: %d, PCM_PLAY: %d, VOLUME_SET: %d",
             g_audio_error_count[AUDIO_ERR_HAL_INIT],
             g_audio_error_count[AUDIO_ERR_HAL_CONFIG],
             g_audio_error_count[AUDIO_ERR_DECODE_INIT],
             g_audio_error_count[AUDIO_ERR_MIXER_INIT],
             g_audio_error_count[AUDIO_ERR_BUFFER_INIT],
             g_audio_error_count[AUDIO_ERR_PCM_PLAY],
             g_audio_error_count[AUDIO_ERR_VOLUME_SET]);
    
    // 发送音频错误事件
    event_notify(EVENT_AUDIO_ERROR, (void *)&error_type);
    
    // 自动恢复机制
    static int last_recovery_time = 0;
    int current_time = hal_get_current_time();
    
    // 避免频繁恢复
    if (current_time - last_recovery_time > 1000) {
        last_recovery_time = current_time;
        LOG_INFO("Initiating auto error recovery");
        audio_core_err_recover();
    }
}

int audio_core_err_recover(void) {
    if (!g_audio_cfg.init_ok) {
        LOG_ERROR("Error recovery failed: audio core not initialized");
        return FAILURE;
    }
    
    LOG_INFO("Audio error recovery started (Attempt %d)", ++g_error_recovery_count);
    
    // 1. 停止当前音频播放（通过HAL层）
    if (hal_audio_stop() != 0) {
        LOG_WARN("Failed to stop audio during recovery");
    }
    
    // 2. 清空环形缓冲区
    audio_ringbuf_deinit();
    if (audio_ringbuf_init(g_audio_cfg.pcm_buffer_size) != 0) {
        LOG_ERROR("Failed to reinitialize ring buffer during recovery");
        handle_audio_error(AUDIO_ERR_BUFFER_INIT, "Failed to reinitialize ring buffer");
    }
    
    // 3. 重新初始化音频解码
    audio_decode_deinit();
    if (audio_decode_init() != 0) {
        LOG_ERROR("Failed to reinitialize audio decode during recovery");
        handle_audio_error(AUDIO_ERR_DECODE_INIT, "Failed to reinitialize audio decode");
    }
    
    // 4. 重新初始化混音器
    audio_mixer_deinit();
    if (audio_mixer_init() != 0) {
        LOG_ERROR("Failed to reinitialize audio mixer during recovery");
        handle_audio_error(AUDIO_ERR_MIXER_INIT, "Failed to reinitialize audio mixer");
    }
    
    // 5. 重启音频驱动（通过HAL层）
    hal_audio_deinit();
    if (hal_audio_init() != 0) {
        LOG_ERROR("Failed to reinitialize HAL audio during recovery");
        handle_audio_error(AUDIO_ERR_HAL_INIT, "Failed to reinitialize HAL audio");
    } else {
        // 重新配置音频参数
        hal_audio_set_sample_rate(g_audio_cfg.sample_rate);
        hal_audio_set_channels(g_audio_cfg.channel_num);
        hal_audio_set_hw_decode(g_audio_cfg.hw_decode_en);
        hal_audio_set_dolby_dts(g_audio_cfg.dolby_dts_en);
    }
    
    LOG_INFO("Audio error recovery completed");
    
    // 发送恢复完成事件
    event_notify(EVENT_AUDIO_ERROR_RECOVERED, NULL);
    
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
        handle_audio_error(AUDIO_ERR_VOLUME_SET, "Failed to set volume");
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
        handle_audio_error(AUDIO_ERR_PCM_PLAY, "Invalid parameters or not initialized for tone playback");
        return FAILURE;
    }
    unsigned int written = audio_ringbuf_write(tone_data, tone_size);
    if (written != tone_size) {
        LOG_WARN("Play tone: buffer full, only wrote %u bytes", written);
        // 缓冲区满，可能需要自动调整
        adjust_buffer_size();
    }
    return SUCCESS;
}

// 内部使用的函数，不需要对外暴露
static AudioCoreConfig_t *audio_core_get_config(void) {
    return &g_audio_cfg;
}