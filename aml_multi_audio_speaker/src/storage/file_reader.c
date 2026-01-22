#include "storage_priv.h"
#include "logger.h"
#include "audio_core.h"
#include "event.h"

#include <aml_audio_decode.h>  // 晶晨音频解码SDK

static int g_file_reader_init = 0;
static bool g_file_playing = false;
static char g_current_playing_file[128] = {0};
static int g_current_volume = 80;
static char **g_audio_file_list = NULL;  // 音频文件列表
static int g_audio_file_count = 0;       // 音频文件数量
static int g_current_file_index = -1;    // 当前播放文件索引

#ifdef _WIN32
static HANDLE g_file_reader_mutex = NULL; // Windows互斥锁
#else
static pthread_mutex_t g_file_reader_mutex = PTHREAD_MUTEX_INITIALIZER; // Unix互斥锁
#endif

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
            
            // 尝试自动播放下一首歌曲
            if (g_audio_file_count > 0 && g_current_file_index >= 0) {
                int next_index = (g_current_file_index + 1) % g_audio_file_count;
                if (g_audio_file_list[next_index] && is_file_accessible(g_audio_file_list[next_index])) {
                    LOG_INFO("Automatically playing next song: %s", g_audio_file_list[next_index]);
                    g_current_file_index = next_index;
                    file_reader_play_file(g_audio_file_list[next_index]);
                    return;
                }
            }
            
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

    // 初始化互斥锁
#ifdef _WIN32
    g_file_reader_mutex = CreateMutex(NULL, FALSE, NULL);
    if (!g_file_reader_mutex) {
        LOG_ERROR("Failed to create mutex: %d", GetLastError());
        return -1;
    }
#else
    if (pthread_mutex_init(&g_file_reader_mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize mutex");
        return -1;
    }
#endif

    // 初始化音频解码SDK
    if (aml_audio_decode_init() != 0) {
        LOG_ERROR("Failed to initialize audio decode SDK");
        
        // 清理互斥锁
#ifdef _WIN32
        if (g_file_reader_mutex) {
            CloseHandle(g_file_reader_mutex);
            g_file_reader_mutex = NULL;
        }
#else
        pthread_mutex_destroy(&g_file_reader_mutex);
#endif
        return -1;
    }
    
    aml_audio_decode_set_data_callback(audio_decode_callback);
    aml_audio_decode_set_status_callback(audio_decode_status_callback);
    aml_audio_decode_set_volume(g_current_volume);

    g_file_reader_init = 1;
    g_file_playing = false;
    g_current_playing_file[0] = '\0';
    g_audio_file_list = NULL;
    g_audio_file_count = 0;
    g_current_file_index = -1;

    LOG_INFO("Storage: Local file reader init (MP3/WAV/FLAC support)");
    return 0;
}

void file_reader_deinit(void)
{
    if (g_file_reader_init) {
        // 停止当前播放的文件
        if (g_file_playing) {
            if (aml_audio_decode_stop() != 0) {
                LOG_ERROR("Failed to stop audio decoding");
            }
        }

        // 释放音频文件列表
        if (g_audio_file_list) {
            for (int i = 0; i < g_audio_file_count; i++) {
                if (g_audio_file_list[i]) {
                    free(g_audio_file_list[i]);
                }
            }
            free(g_audio_file_list);
            g_audio_file_list = NULL;
        }

        // 反初始化音频解码SDK
        if (aml_audio_decode_deinit() != 0) {
            LOG_ERROR("Failed to deinitialize audio decode SDK");
        }

        // 清理互斥锁
#ifdef _WIN32
        if (g_file_reader_mutex) {
            CloseHandle(g_file_reader_mutex);
            g_file_reader_mutex = NULL;
        }
#else
        if (pthread_mutex_destroy(&g_file_reader_mutex) != 0) {
            LOG_ERROR("Failed to destroy mutex");
        }
#endif

        g_file_reader_init = 0;
        g_file_playing = false;
        g_current_playing_file[0] = '\0';
        g_audio_file_count = 0;
        g_current_file_index = -1;

        LOG_INFO("File reader deinitialized");
    }
}

/**
 * @brief 检查文件是否存在且可读
 */
static bool is_file_accessible(const char *file_path)
{
    if (!file_path) {
        return false;
    }
    
    // 检查文件是否存在
    struct stat statbuf;
    if (stat(file_path, &statbuf) != 0) {
        LOG_ERROR("File does not exist: %s", file_path);
        return false;
    }
    
    // 检查是否为常规文件
    if (!S_ISREG(statbuf.st_mode)) {
        LOG_ERROR("Not a regular file: %s", file_path);
        return false;
    }
    
    // 检查是否可读
    if (access(file_path, R_OK) != 0) {
        LOG_ERROR("File not readable: %s", file_path);
        return false;
    }
    
    return true;
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

    // 加锁
#ifdef _WIN32
    WaitForSingleObject(g_file_reader_mutex, INFINITE);
#else
    pthread_mutex_lock(&g_file_reader_mutex);
#endif

    // 检查文件是否存在且可读
    if (!is_file_accessible(file_path)) {
        LOG_ERROR("Failed to play file: file not accessible");
        event_notify(EVENT_FILE_PLAY_ERROR, (void *)file_path);
        
        // 解锁
#ifdef _WIN32
        ReleaseMutex(g_file_reader_mutex);
#else
        pthread_mutex_unlock(&g_file_reader_mutex);
#endif
        return -1;
    }

    // 如果当前正在播放文件，先停止
    if (g_file_playing) {
        aml_audio_decode_stop();
    }

    // 开始播放新文件
    int ret = -1;
    if (aml_audio_decode_start(file_path) == 0) {
        strncpy(g_current_playing_file, file_path, sizeof(g_current_playing_file) - 1);
        g_file_playing = true;
        LOG_INFO("Start playing file: %s", file_path);
        // 通知系统文件播放开始
        event_notify(EVENT_FILE_PLAY_START, (void *)file_path);
        ret = 0;
    } else {
        LOG_ERROR("Failed to play file: %s", file_path);
        // 通知系统文件播放失败
        event_notify(EVENT_FILE_PLAY_ERROR, (void *)file_path);
        ret = -1;
    }

    // 解锁
#ifdef _WIN32
    ReleaseMutex(g_file_reader_mutex);
#else
    pthread_mutex_unlock(&g_file_reader_mutex);
#endif

    return ret;
}

/**
 * @brief 检查播放状态，处理U盘拔出等错误情况
 */
void file_reader_check_play_status(void)
{
    if (g_file_playing && g_current_playing_file[0] != '\0') {
        // 检查文件是否仍然存在
        if (!is_file_accessible(g_current_playing_file)) {
            LOG_ERROR("File no longer accessible, stopping playback: %s", g_current_playing_file);
            file_reader_stop();
        }
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

/**
 * @brief 设置音频文件列表
 * @param file_list 音频文件路径数组
 * @param count 文件数量
 * @return 0表示成功，非0表示失败
 */
int file_reader_set_audio_file_list(char **file_list, int count)
{
    if (!g_file_reader_init) {
        LOG_ERROR("Failed to set file list: reader not initialized");
        return -1;
    }

    // 加锁
#ifdef _WIN32
    WaitForSingleObject(g_file_reader_mutex, INFINITE);
#else
    pthread_mutex_lock(&g_file_reader_mutex);
#endif

    // 释放旧的文件列表
    if (g_audio_file_list) {
        for (int i = 0; i < g_audio_file_count; i++) {
            if (g_audio_file_list[i]) {
                free(g_audio_file_list[i]);
            }
        }
        free(g_audio_file_list);
        g_audio_file_list = NULL;
    }

    // 设置新的文件列表
    g_audio_file_count = count;
    g_current_file_index = -1;

    if (count > 0 && file_list) {
        g_audio_file_list = (char **)malloc(sizeof(char *) * count);
        if (!g_audio_file_list) {
            LOG_ERROR("Failed to allocate memory for file list");
            g_audio_file_count = 0;
            
            // 解锁
#ifdef _WIN32
            ReleaseMutex(g_file_reader_mutex);
#else
            pthread_mutex_unlock(&g_file_reader_mutex);
#endif
            return -1;
        }

        for (int i = 0; i < count; i++) {
            if (file_list[i]) {
                g_audio_file_list[i] = strdup(file_list[i]);
                if (!g_audio_file_list[i]) {
                    LOG_ERROR("Failed to duplicate file path");
                    // 释放已分配的内存
                    for (int j = 0; j < i; j++) {
                        if (g_audio_file_list[j]) {
                            free(g_audio_file_list[j]);
                        }
                    }
                    free(g_audio_file_list);
                    g_audio_file_list = NULL;
                    g_audio_file_count = 0;
                    
                    // 解锁
#ifdef _WIN32
                    ReleaseMutex(g_file_reader_mutex);
#else
                    pthread_mutex_unlock(&g_file_reader_mutex);
#endif
                    return -1;
                }
            } else {
                g_audio_file_list[i] = NULL;
            }
        }

        LOG_INFO("Set audio file list: %d files", count);
    }

    // 解锁
#ifdef _WIN32
    ReleaseMutex(g_file_reader_mutex);
#else
    pthread_mutex_unlock(&g_file_reader_mutex);
#endif

    return 0;
}

/**
 * @brief 播放下一首歌曲
 * @return 0表示成功，非0表示失败
 */
int file_reader_play_next(void)
{
    if (!g_file_reader_init || g_audio_file_count == 0) {
        LOG_ERROR("Failed to play next: no audio files available");
        return -1;
    }

    // 加锁
#ifdef _WIN32
    WaitForSingleObject(g_file_reader_mutex, INFINITE);
#else
    pthread_mutex_lock(&g_file_reader_mutex);
#endif

    // 计算下一首歌曲的索引
    int next_index = (g_current_file_index + 1) % g_audio_file_count;
    if (next_index < 0) {
        next_index = 0;
    }

    // 检查下一首歌曲是否存在且可读
    int ret = -1;
    if (g_audio_file_list[next_index] && is_file_accessible(g_audio_file_list[next_index])) {
        LOG_INFO("Playing next song: %s", g_audio_file_list[next_index]);
        g_current_file_index = next_index;
        
        // 解锁
#ifdef _WIN32
        ReleaseMutex(g_file_reader_mutex);
#else
        pthread_mutex_unlock(&g_file_reader_mutex);
#endif
        
        ret = file_reader_play_file(g_audio_file_list[next_index]);
    } else {
        LOG_ERROR("Failed to play next song: file not accessible");
        
        // 解锁
#ifdef _WIN32
        ReleaseMutex(g_file_reader_mutex);
#else
        pthread_mutex_unlock(&g_file_reader_mutex);
#endif
        
        ret = -1;
    }

    return ret;
}

/**
 * @brief 播放上一首歌曲
 * @return 0表示成功，非0表示失败
 */
int file_reader_play_prev(void)
{
    if (!g_file_reader_init || g_audio_file_count == 0) {
        LOG_ERROR("Failed to play previous: no audio files available");
        return -1;
    }

    // 加锁
#ifdef _WIN32
    WaitForSingleObject(g_file_reader_mutex, INFINITE);
#else
    pthread_mutex_lock(&g_file_reader_mutex);
#endif

    // 计算上一首歌曲的索引
    int prev_index = g_current_file_index - 1;
    if (prev_index < 0) {
        prev_index = g_audio_file_count - 1;
    }

    // 检查上一首歌曲是否存在且可读
    int ret = -1;
    if (g_audio_file_list[prev_index] && is_file_accessible(g_audio_file_list[prev_index])) {
        LOG_INFO("Playing previous song: %s", g_audio_file_list[prev_index]);
        g_current_file_index = prev_index;
        
        // 解锁
#ifdef _WIN32
        ReleaseMutex(g_file_reader_mutex);
#else
        pthread_mutex_unlock(&g_file_reader_mutex);
#endif
        
        ret = file_reader_play_file(g_audio_file_list[prev_index]);
    } else {
        LOG_ERROR("Failed to play previous song: file not accessible");
        
        // 解锁
#ifdef _WIN32
        ReleaseMutex(g_file_reader_mutex);
#else
        pthread_mutex_unlock(&g_file_reader_mutex);
#endif
        
        ret = -1;
    }

    return ret;
}

/**
 * @brief 获取当前播放的歌曲索引
 * @return 当前歌曲索引，-1表示无歌曲播放
 */
int file_reader_get_current_index(void)
{
    return g_current_file_index;
}

/**
 * @brief 获取音频文件总数
 * @return 文件总数
 */
int file_reader_get_file_count(void)
{
    return g_audio_file_count;
}
