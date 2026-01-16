#include "storage_priv.h"
#include "logger.h"
#include "audio_core.h"
#include "event.h"

#include <aml_audio_decode.h>  // 晶晨音频解码SDK

static int g_file_reader_init = 0;
static bool g_file_playing = false;
static char g_current_playing_file[128] = {0};
static int g_current_volume = 80;

/**
 * @brief 音频解码完成回调函数
 */
static void audio_decode_callback(uint8_t *pcm_data, int data_len)
{
    if (g_file_playing && pcm_data && data_len > 0) {
        // 将解码后的PCM数据发送到音频核心
        audio_core_play_pcm(pcm_data, data_len);
    }
}

/**
 * @brief 音频解码状态回调函数
 */
static void audio_decode_status_callback(int status)
{
    switch (status) {
        case AUDIO_DECODE_STATUS_COMPLETE:
            LOG_INFO("File playback completed: %s", g_current_playing_file);
            g_file_playing = false;
            g_current_playing_file[0] = '\0';
            break;
        case AUDIO_DECODE_STATUS_ERROR:
            LOG_ERROR("File playback error: %s", g_current_playing_file);
            g_file_playing = false;
            g_current_playing_file[0] = '\0';
            break;
        default:
            break;
    }
}

int file_reader_init(void)
{
    if (g_file_reader_init) {
        LOG_INFO("File reader already initialized");
        return 0;
    }

    // 初始化音频解码SDK
    aml_audio_decode_init();
    aml_audio_decode_set_data_callback(audio_decode_callback);
    aml_audio_decode_set_status_callback(audio_decode_status_callback);
    aml_audio_decode_set_volume(g_current_volume);

    g_file_reader_init = 1;
    g_file_playing = false;
    g_current_playing_file[0] = '\0';

    LOG_INFO("Storage: Local file reader init (MP3/WAV/FLAC support)");
    return 0;
}

void file_reader_deinit(void)
{
    if (g_file_reader_init) {
        // 停止当前播放的文件
        if (g_file_playing) {
            aml_audio_decode_stop();
        }

        // 反初始化音频解码SDK
        aml_audio_decode_deinit();

        g_file_reader_init = 0;
        g_file_playing = false;
        g_current_playing_file[0] = '\0';

        LOG_INFO("File reader deinitialized");
    }
}

/**
 * @brief 播放指定的音频文件
 */
int file_reader_play_file(const char *file_path)
{
    if (!g_file_reader_init || !file_path) {
        LOG_ERROR("Failed to play file: invalid parameters");
        return -1;
    }

    // 如果当前正在播放文件，先停止
    if (g_file_playing) {
        aml_audio_decode_stop();
    }

    // 开始播放新文件
    if (aml_audio_decode_start(file_path) == 0) {
        strncpy(g_current_playing_file, file_path, sizeof(g_current_playing_file) - 1);
        g_file_playing = true;
        LOG_INFO("Start playing file: %s", file_path);
        // 通知系统文件播放开始
        event_notify(EVENT_FILE_PLAY_START, (void *)file_path);
        return 0;
    } else {
        LOG_ERROR("Failed to play file: %s", file_path);
        // 通知系统文件播放失败
        event_notify(EVENT_FILE_PLAY_ERROR, (void *)file_path);
        return -1;
    }
}

/**
 * @brief 暂停当前播放的音频文件
 */
int file_reader_pause(void)
{
    if (!g_file_reader_init || !g_file_playing) {
        LOG_ERROR("Failed to pause file: invalid state");
        return -1;
    }

    if (aml_audio_decode_pause() == 0) {
        LOG_INFO("Pause playing file: %s", g_current_playing_file);
        // 通知系统文件播放暂停
        event_notify(EVENT_FILE_PLAY_PAUSE, (void *)g_current_playing_file);
        return 0;
    } else {
        LOG_ERROR("Failed to pause file: %s", g_current_playing_file);
        return -1;
    }
}

/**
 * @brief 继续播放当前暂停的音频文件
 */
int file_reader_resume(void)
{
    if (!g_file_reader_init || !g_file_playing) {
        LOG_ERROR("Failed to resume file: invalid state");
        return -1;
    }

    if (aml_audio_decode_resume() == 0) {
        LOG_INFO("Resume playing file: %s", g_current_playing_file);
        // 通知系统文件播放继续
        event_notify(EVENT_FILE_PLAY_RESUME, (void *)g_current_playing_file);
        return 0;
    } else {
        LOG_ERROR("Failed to resume file: %s", g_current_playing_file);
        return -1;
    }
}

/**
 * @brief 停止当前播放的音频文件
 */
int file_reader_stop(void)
{
    if (!g_file_reader_init) {
        LOG_ERROR("Failed to stop file: reader not initialized");
        return -1;
    }

    if (!g_file_playing) {
        LOG_WARN("Failed to stop file: no file is playing");
        return -1;
    }

    if (aml_audio_decode_stop() == 0) {
        LOG_INFO("Stop playing file: %s", g_current_playing_file);
        
        // 通知系统文件播放停止
        event_notify(EVENT_FILE_PLAY_STOP, (void *)g_current_playing_file);
        
        g_file_playing = false;
        g_current_playing_file[0] = '\0';
        return 0;
    } else {
        LOG_ERROR("Failed to stop file: %s", g_current_playing_file);
        return -1;
    }
}

/**
 * @brief 设置播放音量
 */
int file_reader_set_volume(int volume)
{
    if (!g_file_reader_init) {
        return -1;
    }

    if (volume >= 0 && volume <= 100) {
        g_current_volume = volume;
        aml_audio_decode_set_volume(volume);
        LOG_INFO("Set file player volume to %d", volume);
        return 0;
    } else {
        LOG_ERROR("Invalid volume value: %d", volume);
        return -1;
    }
}

/**
 * @brief 获取当前播放状态
 */
bool file_reader_is_playing(void)
{
    return g_file_playing;
}

/**
 * @brief 获取当前播放的文件名
 */
const char *file_reader_get_current_file(void)
{
    return g_current_playing_file;
}