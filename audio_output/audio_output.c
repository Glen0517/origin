#include "audio_output.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <errno.h>
#include <alsa/asoundlib.h>
#include <linux/sched.h>

// 音频输出设备结构体实现
struct AudioOutputDevice {
    AudioOutputConfig config;
    snd_pcm_t* pcm_handle;
    snd_pcm_hw_params_t* hw_params;
    snd_pcm_sw_params_t* sw_params;
    void* buffer;
    size_t buffer_size_bytes;
    int fd;
    pthread_t thread;
    volatile int state;
    float volume;
    pthread_mutex_t mutex;
    // 音频回调函数
    void (*data_callback)(void* user_data, void* output_buffer, size_t frames);
    void* user_data;
};

// 静态函数声明
static int set_realtime_priority(int priority);
static snd_pcm_format_t audio_format_to_alsa(enum AudioOutputFormat format);
static size_t format_to_bytes(enum AudioOutputFormat format);
static void* output_thread(void* arg);

// 创建音频输出设备
AudioOutputDevice* audio_output_create(const AudioOutputConfig* config)
{
    if (!config || config->sample_rate == 0 || config->channels == 0 || config->buffer_size == 0) {
        fprintf(stderr, "Invalid audio output configuration\n");
        return NULL;
    }

    AudioOutputDevice* device = malloc(sizeof(AudioOutputDevice));
    if (!device) {
        fprintf(stderr, "Failed to allocate memory for audio output device\n");
        return NULL;
    }

    memset(device, 0, sizeof(AudioOutputDevice));
    device->config = *config;
    device->state = 0; // 0 = stopped, 1 = running
    device->volume = 1.0f; // 默认音量100%
    pthread_mutex_init(&device->mutex, NULL);

    // 设置默认设备名称
    if (!device->config.device_name) {
        device->config.device_name = "default";
    }

    // 设置默认优先级
    if (device->config.priority <= 0 || device->config.priority > 99) {
        device->config.priority = 50;
    }

    return device;
}

// 销毁音频输出设备
void audio_output_destroy(AudioOutputDevice* device)
{
    if (!device) return;

    audio_output_close(device);
    pthread_mutex_destroy(&device->mutex);

    if (device->buffer && device->config.use_dma) {
        munmap(device->buffer, device->buffer_size_bytes);
    } else {
        free(device->buffer);
    }

    free(device);
}

// 打开音频输出设备
int audio_output_open(AudioOutputDevice* device)
{
    if (!device) return -EINVAL;

    int ret;
    snd_pcm_format_t format = audio_format_to_alsa(device->config.format);
    if (format == SND_PCM_FORMAT_UNKNOWN) {
        fprintf(stderr, "Unsupported audio format\n");
        return -EINVAL;
    }

    // 打开PCM设备
    ret = snd_pcm_open(&device->pcm_handle, device->config.device_name, SND_PCM_STREAM_PLAYBACK, 0);
    if (ret < 0) {
        fprintf(stderr, "Failed to open PCM device: %s\n", snd_strerror(ret));
        return ret;
    }

    // 分配硬件参数对象
    snd_pcm_hw_params_alloca(&device->hw_params);
    ret = snd_pcm_hw_params_any(device->pcm_handle, device->hw_params);
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize hw params: %s\n", snd_strerror(ret));
        snd_pcm_close(device->pcm_handle);
        return ret;
    }

    // 设置访问类型为交错模式
    ret = snd_pcm_hw_params_set_access(device->pcm_handle, device->hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (ret < 0) {
        fprintf(stderr, "Failed to set access type: %s\n", snd_strerror(ret));
        snd_pcm_close(device->pcm_handle);
        return ret;
    }

    // 设置音频格式
    ret = snd_pcm_hw_params_set_format(device->pcm_handle, device->hw_params, format);
    if (ret < 0) {
        fprintf(stderr, "Failed to set format: %s\n", snd_strerror(ret));
        snd_pcm_close(device->pcm_handle);
        return ret;
    }

    // 设置声道数
    ret = snd_pcm_hw_params_set_channels(device->pcm_handle, device->hw_params, device->config.channels);
    if (ret < 0) {
        fprintf(stderr, "Failed to set channels: %s\n", snd_strerror(ret));
        snd_pcm_close(device->pcm_handle);
        return ret;
    }

    // 设置采样率
    uint32_t sample_rate = device->config.sample_rate;
    ret = snd_pcm_hw_params_set_rate_near(device->pcm_handle, device->hw_params, &sample_rate, NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to set sample rate: %s\n", snd_strerror(ret));
        snd_pcm_close(device->pcm_handle);
        return ret;
    }

    if (sample_rate != device->config.sample_rate) {
        fprintf(stderr, "Warning: Sample rate adjusted from %u to %u\n", device->config.sample_rate, sample_rate);
        device->config.sample_rate = sample_rate;
    }

    // 设置缓冲区大小
    snd_pcm_uframes_t buffer_size = device->config.buffer_size;
    ret = snd_pcm_hw_params_set_buffer_size_near(device->pcm_handle, device->hw_params, &buffer_size);
    if (ret < 0) {
        fprintf(stderr, "Failed to set buffer size: %s\n", snd_strerror(ret));
        snd_pcm_close(device->pcm_handle);
        return ret;
    }

    // 设置周期大小
    if (device->config.period_size > 0) {
        snd_pcm_uframes_t period_size = device->config.period_size;
        ret = snd_pcm_hw_params_set_period_size_near(device->pcm_handle, device->hw_params, &period_size, NULL);
        if (ret < 0) {
            fprintf(stderr, "Failed to set period size: %s\n", snd_strerror(ret));
            snd_pcm_close(device->pcm_handle);
            return ret;
        }
    }

    // 应用硬件参数
    ret = snd_pcm_hw_params(device->pcm_handle, device->hw_params);
    if (ret < 0) {
        fprintf(stderr, "Failed to apply hw params: %s\n", snd_strerror(ret));
        snd_pcm_close(device->pcm_handle);
        return ret;
    }

    // 分配软件参数
    snd_pcm_sw_params_alloca(&device->sw_params);
    ret = snd_pcm_sw_params_current(device->pcm_handle, device->sw_params);
    if (ret < 0) {
        fprintf(stderr, "Failed to get sw params: %s\n", snd_strerror(ret));
        snd_pcm_close(device->pcm_handle);
        return ret;
    }

    // 设置唤醒阈值
    ret = snd_pcm_sw_params_set_avail_min(device->pcm_handle, device->sw_params, device->config.period_size);
    if (ret < 0) {
        fprintf(stderr, "Failed to set avail min: %s\n", snd_strerror(ret));
        snd_pcm_close(device->pcm_handle);
        return ret;
    }

    // 应用软件参数
    ret = snd_pcm_sw_params(device->pcm_handle, device->sw_params);
    if (ret < 0) {
        fprintf(stderr, "Failed to apply sw params: %s\n", snd_strerror(ret));
        snd_pcm_close(device->pcm_handle);
        return ret;
    }

    // 分配缓冲区
    size_t bytes_per_frame = format_to_bytes(device->config.format) * device->config.channels;
    device->buffer_size_bytes = buffer_size * bytes_per_frame;

    if (device->config.use_dma) {
        // DMA模式下使用mmap
        device->buffer = snd_pcm_mmap_begin(device->pcm_handle, &device->hw_params, &buffer_size, &device->buffer_size_bytes);
        if (!device->buffer) {
            fprintf(stderr, "Failed to mmap PCM buffer\n");
            snd_pcm_close(device->pcm_handle);
            return -ENOMEM;
        }
    } else {
        // 非DMA模式下使用常规内存分配
        device->buffer = malloc(device->buffer_size_bytes);
        if (!device->buffer) {
            fprintf(stderr, "Failed to allocate PCM buffer\n");
            snd_pcm_close(device->pcm_handle);
            return -ENOMEM;
        }
        memset(device->buffer, 0, device->buffer_size_bytes);
    }

    // 设置实时优先级
    if (device->config.priority > 0) {
        set_realtime_priority(device->config.priority);
    }

    return device;
}

// 关闭音频输出设备
void audio_output_close(AudioOutputDevice* device)
{
    if (!device) return;

    // 停止输出线程
    if (device->state == 1) {
        device->state = 0;
        pthread_join(device->thread, NULL);
    }

    // 关闭PCM设备
    if (device->pcm_handle) {
        snd_pcm_drain(device->pcm_handle);
        snd_pcm_close(device->pcm_handle);
        device->pcm_handle = NULL;
    }

    device->state = 0;
}

// 写入音频数据到输出设备
int audio_output_write(AudioOutputDevice* device, const void* data, uint32_t frames)
{
    if (!device || !data || frames == 0 || device->state != 1) {
        return -EINVAL;
    }

    pthread_mutex_lock(&device->mutex);

    size_t bytes_per_frame = format_to_bytes(device->config.format) * device->config.channels;
    size_t bytes_to_write = frames * bytes_per_frame;

    // 检查缓冲区大小
    if (bytes_to_write > device->buffer_size_bytes) {
        pthread_mutex_unlock(&device->mutex);
        return -ENOBUFS;
    }

    // 应用音量
    if (device->volume != 1.0f) {
        // 根据不同格式应用音量
        switch (device->config.format) {
            case AUDIO_FORMAT_S16_LE: {
                int16_t* src = (int16_t*)data;
                int16_t* dest = (int16_t*)device->buffer;
                for (uint32_t i = 0; i < frames * device->config.channels; i++) {
                    dest[i] = (int16_t)(src[i] * device->volume);
                }
                break;
            }
            case AUDIO_FORMAT_S32_LE: {
                int32_t* src = (int32_t*)data;
                int32_t* dest = (int32_t*)device->buffer;
                for (uint32_t i = 0; i < frames * device->config.channels; i++) {
                    dest[i] = (int32_t)(src[i] * device->volume);
                }
                break;
            }
            case AUDIO_FORMAT_FLOAT32_LE: {
                float* src = (float*)data;
                float* dest = (float*)device->buffer;
                for (uint32_t i = 0; i < frames * device->config.channels; i++) {
                    dest[i] = src[i] * device->volume;
                }
                break;
            }
            default:
                pthread_mutex_unlock(&device->mutex);
                return -EINVAL;
        }
    } else {
        // 无需音量调整，直接复制
        memcpy(device->buffer, data, bytes_to_write);
    }

    // 写入PCM设备
    snd_pcm_sframes_t written = snd_pcm_writei(device->pcm_handle, device->buffer, frames);
    if (written < 0) {
        // 尝试恢复
        written = snd_pcm_recover(device->pcm_handle, written, 0);
    }

    pthread_mutex_unlock(&device->mutex);

    return (written > 0) ? (int)written : (int)written;
}

// 获取音频输出设备的当前状态
int audio_output_get_state(AudioOutputDevice* device)
{
    if (!device) return -EINVAL;
    return device->state;
}

// 获取音频输出设备的缓冲信息
int audio_output_get_buffer_info(AudioOutputDevice* device, uint32_t* available, uint64_t* latency)
{
    if (!device || !available || !latency) return -EINVAL;

    snd_pcm_sframes_t avail = snd_pcm_avail(device->pcm_handle);
    if (avail < 0) return (int)avail;

    *available = (uint32_t)avail;

    // 计算延迟 (微秒)
    *latency = (uint64_t)((avail * 1000000LL) / device->config.sample_rate);

    return 0;
}

// 设置音频输出设备的音量
int audio_output_set_volume(AudioOutputDevice* device, float volume)
{
    if (!device || volume < 0.0f || volume > 1.0f) return -EINVAL;

    pthread_mutex_lock(&device->mutex);
    device->volume = volume;
    pthread_mutex_unlock(&device->mutex);

    return 0;
}

// 获取音频输出设备的音量
float audio_output_get_volume(AudioOutputDevice* device)
{
    if (!device) return 0.0f;

    pthread_mutex_lock(&device->mutex);
    float volume = device->volume;
    pthread_mutex_unlock(&device->mutex);

    return volume;
}

// 设置实时优先级
static int set_realtime_priority(int priority)
{
    struct sched_param param;
    memset(&param, 0, sizeof(param));
    param.sched_priority = priority;

    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        fprintf(stderr, "Warning: Failed to set realtime priority: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

// 音频格式转换为ALSA格式
static snd_pcm_format_t audio_format_to_alsa(enum AudioOutputFormat format)
{
    switch (format) {
        case AUDIO_FORMAT_S16_LE:
            return SND_PCM_FORMAT_S16_LE;
        case AUDIO_FORMAT_S32_LE:
            return SND_PCM_FORMAT_S32_LE;
        case AUDIO_FORMAT_FLOAT32_LE:
            return SND_PCM_FORMAT_FLOAT_LE;
        default:
            return SND_PCM_FORMAT_UNKNOWN;
    }
}

// 计算每个样本的字节数
static size_t format_to_bytes(enum AudioOutputFormat format)
{
    switch (format) {
        case AUDIO_FORMAT_S16_LE:
            return 2;
        case AUDIO_FORMAT_S32_LE:
            return 4;
        case AUDIO_FORMAT_FLOAT32_LE:
            return 4;
        default:
            return 0;
    }
}

// 输出线程
static void* output_thread(void* arg)
{
    AudioOutputDevice* device = (AudioOutputDevice*)arg;
    if (!device || !device->data_callback) return NULL;

    // 设置线程实时优先级
    if (device->config.priority > 0) {
        set_realtime_priority(device->config.priority);
    }

    size_t bytes_per_frame = format_to_bytes(device->config.format) * device->config.channels;
    uint32_t period_frames = device->config.period_size;
    size_t period_bytes = period_frames * bytes_per_frame;

    while (device->state == 1) {
        // 调用回调函数获取音频数据
        device->data_callback(device->user_data, device->buffer, period_frames);

        // 写入音频数据
        snd_pcm_sframes_t written = snd_pcm_writei(device->pcm_handle, device->buffer, period_frames);
        if (written < 0) {
            // 尝试恢复
            if (snd_pcm_recover(device->pcm_handle, written, 0) < 0) {
                fprintf(stderr, "Failed to recover PCM: %s\n", snd_strerror((int)written));
                break;
            }
        }

        // 短暂休眠以降低CPU占用
        usleep(1000);
    }

    return NULL;
}

// 辅助函数：启动输出线程
int audio_output_start(AudioOutputDevice* device, void (*data_callback)(void*, void*, size_t), void* user_data)
{
    if (!device || !data_callback || device->state == 1) {
        return -EINVAL;
    }

    device->data_callback = data_callback;
    device->user_data = user_data;
    device->state = 1;

    // 创建输出线程
    int ret = pthread_create(&device->thread, NULL, output_thread, device);
    if (ret != 0) {
        fprintf(stderr, "Failed to create output thread: %s\n", strerror(ret));
        device->state = 0;
        return ret;
    }

    return 0;
}