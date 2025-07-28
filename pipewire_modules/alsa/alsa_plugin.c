#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <pipewire/pipewire.h>
#include <spa/utils/result.h>
#include <spa/param/audio/format-utils.h>
#include <pthread.h>

#include "alsa_plugin.h"

// ALSA设备结构体
typedef struct {
    char name[128];            // 设备名称
    char card_name[256];       // 声卡名称
    char device_id[32];        // 设备ID
    int card;                  // 卡片索引
    int device;                // 设备索引
    bool is_capture;           // 是否为捕获设备
    snd_pcm_t* handle;         // PCM句柄
    struct spa_audio_info format; // 音频格式
    struct pw_stream* stream;  // PipeWire流
    pthread_mutex_t mutex;     // 互斥锁
} AlsaDevice;

// ALSA插件结构体
typedef struct {
    AlsaDevice* devices[32];   // 设备列表
    int device_count;          // 设备数量
    pthread_mutex_t mutex;     // 互斥锁
    bool initialized;          // 是否已初始化
} AlsaPlugin;

// 全局插件实例
static AlsaPlugin plugin = {
    .device_count = 0,
    .initialized = false
};

// 前向声明
static int alsa_enumerate_devices(AlsaPlugin* plugin);
static int alsa_open_device(AlsaDevice* device);
static void alsa_close_device(AlsaDevice* device);
static struct pw_stream* alsa_create_pipewire_stream(AlsaDevice* device);
static void alsa_device_destroy(AlsaDevice* device);

// 初始化ALSA插件
int alsa_plugin_init() {
    int ret = 0;

    // 初始化互斥锁
    pthread_mutex_init(&plugin.mutex, NULL);

    // 枚举ALSA设备
    ret = alsa_enumerate_devices(&plugin);
    if (ret < 0) {
        fprintf(stderr, "Failed to enumerate ALSA devices\n");
        pthread_mutex_destroy(&plugin.mutex);
        return -1;
    }

    // 初始化PipeWire
    pw_init(NULL, NULL);

    plugin.initialized = true;
    printf("ALSA plugin initialized with %d devices\n", plugin.device_count);
    return 0;
}

// 枚举ALSA设备
static int alsa_enumerate_devices(AlsaPlugin* plugin) {
    int card = -1;
    int err;

    // 遍历所有声卡
    while (snd_card_next(&card) >= 0 && card >= 0) {
        char card_name[128];
        char card_path[256];

        // 获取声卡名称
        err = snd_card_get_name(card, card_name, sizeof(card_name));
        if (err < 0) {
            snprintf(card_name, sizeof(card_name), "hw:%d", card);
        }

        // 遍历声卡上的设备
        int device = -1;
        while (snd_device_next(card, &device) >= 0 && device >= 0) {
            // 只处理PCM设备
            if (snd_device_get_type(card, device) != SND_DEVICE_TYPE_PCM) {
                continue;
            }

            // 创建ALSA设备结构体
            AlsaDevice* alsa_device = malloc(sizeof(AlsaDevice));
            if (!alsa_device) {
                fprintf(stderr, "Failed to allocate memory for ALSA device\n");
                continue;
            }

            memset(alsa_device, 0, sizeof(AlsaDevice));
            alsa_device->card = card;
            alsa_device->device = device;
            pthread_mutex_init(&alsa_device->mutex, NULL);

            // 获取设备信息
            snprintf(alsa_device->device_id, sizeof(alsa_device->device_id), "hw:%d,%d", card, device);
            snprintf(alsa_device->card_name, sizeof(alsa_device->card_name), "%s", card_name);

            // 检查设备类型（播放/捕获）
            snd_pcm_stream_t stream_types[] = {SND_PCM_STREAM_PLAYBACK, SND_PCM_STREAM_CAPTURE};
            for (int i = 0; i < 2; i++) {
                snd_pcm_t* handle;
                err = snd_pcm_open(&handle, alsa_device->device_id, stream_types[i], SND_PCM_NONBLOCK);
                if (err >= 0) {
                    alsa_device->is_capture = (stream_types[i] == SND_PCM_STREAM_CAPTURE);
                    snd_pcm_close(handle);

                    // 设置设备名称
                    const char* type_str = alsa_device->is_capture ? "Capture" : "Playback";
                    snprintf(alsa_device->name, sizeof(alsa_device->name), "%s - %s (%s)",
                             card_name, type_str, alsa_device->device_id);

                    // 添加到设备列表
                    pthread_mutex_lock(&plugin->mutex);
                    if (plugin->device_count < 32) {
                        plugin->devices[plugin->device_count++] = alsa_device;
                        printf("Found ALSA device: %s\n", alsa_device->name);
                    } else {
                        fprintf(stderr, "Too many ALSA devices, skipping\n");
                        alsa_device_destroy(alsa_device);
                    }
                    pthread_mutex_unlock(&plugin->mutex);

                    break;
                }
            }
        }
    }

    snd_config_update_free_global();
    return plugin->device_count > 0 ? 0 : -1;
}

// 打开ALSA设备
static int alsa_open_device(AlsaDevice* device) {
    int ret = 0;
    snd_pcm_stream_t stream = device->is_capture ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK;

    pthread_mutex_lock(&device->mutex);

    // 打开PCM设备
    ret = snd_pcm_open(&device->handle, device->device_id, stream, 0);
    if (ret < 0) {
        fprintf(stderr, "Failed to open ALSA device %s: %s\n", device->device_id, snd_strerror(ret));
        pthread_mutex_unlock(&device->mutex);
        return ret;
    }

    // 创建PipeWire流
    device->stream = alsa_create_pipewire_stream(device);
    if (!device->stream) {
        fprintf(stderr, "Failed to create PipeWire stream for ALSA device\n");
        snd_pcm_close(device->handle);
        device->handle = NULL;
        pthread_mutex_unlock(&device->mutex);
        return -1;
    }

    pthread_mutex_unlock(&device->mutex);
    return 0;
}

// 创建PipeWire流
static struct pw_stream* alsa_create_pipewire_stream(AlsaDevice* device) {
    struct pw_properties* props;
    const char* media_type = "Audio";
    const char* media_category = device->is_capture ? "Capture" : "Playback";
    const char* media_role = device->is_capture ? "Capture" : "Music";

    // 创建流属性
    props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, media_type,
        PW_KEY_MEDIA_CATEGORY, media_category,
        PW_KEY_MEDIA_ROLE, media_role,
        PW_KEY_DEVICE, device->device_id,
        PW_KEY_STREAM_NAME, device->name,
        NULL
    );

    if (!props) {
        return NULL;
    }

    // 创建简单流
    struct pw_stream* stream = pw_stream_new_simple(
        pw_context_new(NULL, NULL),
        device->is_capture ? "alsa-capture-stream" : "alsa-playback-stream",
        props,
        NULL
    );

    if (!stream) {
        pw_properties_free(props);
        return NULL;
    }

    return stream;
}

// 关闭ALSA设备
static void alsa_close_device(AlsaDevice* device) {
    if (!device) return;

    pthread_mutex_lock(&device->mutex);

    if (device->stream) {
        pw_stream_destroy(device->stream);
        device->stream = NULL;
    }

    if (device->handle) {
        snd_pcm_close(device->handle);
        device->handle = NULL;
    }

    pthread_mutex_unlock(&device->mutex);
}

// 获取设备列表
int alsa_plugin_get_devices(AlsaDevice** devices, int max_devices) {
    if (!plugin.initialized || !devices || max_devices <= 0) {
        return -1;
    }

    pthread_mutex_lock(&plugin.mutex);
    int count = min(plugin.device_count, max_devices);

    for (int i = 0; i < count; i++) {
        devices[i] = plugin.devices[i];
    }

    pthread_mutex_unlock(&plugin.mutex);
    return count;
}

// 打开指定设备
int alsa_plugin_open_device(int index) {
    if (!plugin.initialized || index < 0 || index >= plugin.device_count) {
        return -1;
    }

    AlsaDevice* device = plugin.devices[index];
    return alsa_open_device(device);
}

// 关闭指定设备
void alsa_plugin_close_device(int index) {
    if (!plugin.initialized || index < 0 || index >= plugin.device_count) {
        return;
    }

    AlsaDevice* device = plugin.devices[index];
    alsa_close_device(device);
}

// 写入音频数据到播放设备
int alsa_plugin_write(int index, const void* data, size_t size) {
    if (!plugin.initialized || index < 0 || index >= plugin.device_count || !data || size == 0) {
        return -1;
    }

    AlsaDevice* device = plugin.devices[index];
    if (device->is_capture || !device->handle || !device->stream) {
        return -1;
    }

    pthread_mutex_lock(&device->mutex);

    // 计算帧数
    size_t frame_size = spa_audio_info_frame_size(&device->format);
    if (frame_size == 0) {
        pthread_mutex_unlock(&device->mutex);
        return -1;
    }

    snd_pcm_sframes_t frames = size / frame_size;
    snd_pcm_sframes_t written = snd_pcm_writei(device->handle, data, frames);

    if (written < 0) {
        fprintf(stderr, "ALSA write error: %s\n", snd_strerror((int)written));
        // 尝试恢复
        written = snd_pcm_recover(device->handle, written, 0);
    }

    pthread_mutex_unlock(&device->mutex);
    return written > 0 ? (int)written * frame_size : (int)written;
}

// 从捕获设备读取音频数据
int alsa_plugin_read(int index, void* data, size_t size) {
    if (!plugin.initialized || index < 0 || index >= plugin.device_count || !data || size == 0) {
        return -1;
    }

    AlsaDevice* device = plugin.devices[index];
    if (!device->is_capture || !device->handle || !device->stream) {
        return -1;
    }

    pthread_mutex_lock(&device->mutex);

    // 计算帧数
    size_t frame_size = spa_audio_info_frame_size(&device->format);
    if (frame_size == 0) {
        pthread_mutex_unlock(&device->mutex);
        return -1;
    }

    snd_pcm_sframes_t frames = size / frame_size;
    snd_pcm_sframes_t read = snd_pcm_readi(device->handle, data, frames);

    if (read < 0) {
        fprintf(stderr, "ALSA read error: %s\n", snd_strerror((int)read));
        // 尝试恢复
        read = snd_pcm_recover(device->handle, read, 0);
    }

    pthread_mutex_unlock(&device->mutex);
    return read > 0 ? (int)read * frame_size : (int)read;
}

// 销毁ALSA设备
static void alsa_device_destroy(AlsaDevice* device) {
    if (!device) return;

    alsa_close_device(device);
    pthread_mutex_destroy(&device->mutex);
    free(device);
}

// 销毁ALSA插件
void alsa_plugin_destroy() {
    if (!plugin.initialized) return;

    pthread_mutex_lock(&plugin.mutex);

    // 销毁所有设备
    for (int i = 0; i < plugin.device_count; i++) {
        alsa_device_destroy(plugin.devices[i]);
        plugin.devices[i] = NULL;
    }

    plugin.device_count = 0;
    plugin.initialized = false;

    pthread_mutex_unlock(&plugin.mutex);
    pthread_mutex_destroy(&plugin.mutex);

    printf("ALSA plugin destroyed\n");
}