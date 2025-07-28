#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <math.h>

#include "rca.h"
#include "../../include/dbus_utils.h"

// 前向声明
static void* rca_worker_thread(void* data);
static int rca_setup_alsa_device(RcaService* service);
static void rca_cleanup_alsa_device(RcaService* service);
static int rca_create_pipewire_stream(RcaService* service);
static void rca_set_state(RcaService* service, RcaState state);
static void rca_process_audio(RcaService* service);

// 创建RCA服务
RcaService* rca_create(struct pw_context* context, const RcaConfig* config) {
    if (!context || !config) {
        fprintf(stderr, "Invalid parameters for rca_create\n");
        return NULL;
    }

    RcaService* service = calloc(1, sizeof(RcaService));
    if (!service) {
        fprintf(stderr, "Failed to allocate memory for RcaService\n");
        return NULL;
    }

    // 初始化互斥锁
    pthread_mutex_init(&service->mutex, NULL);

    // 设置上下文
    service->context = context;
    service->state = RCA_STATE_DISABLED;
    service->running = false;
    service->stream = NULL;
    memset(&service->session, 0, sizeof(RcaSession));
    service->session.volume = 1.0f;
    service->session.muted = false;

    // 复制配置
    service->config = *config;
    if (!service->config.device_name[0]) {
        strncpy(service->config.device_name, "RCA", sizeof(service->config.device_name)-1);
    }
    if (!service->config.alsa_device[0]) {
        strncpy(service->config.alsa_device, "hw:0,1", sizeof(service->config.alsa_device)-1);
    }
    if (service->config.sample_rate == 0) {
        service->config.sample_rate = 48000;
    }
    if (service->config.channels == 0) {
        service->config.channels = 2;
    }
    if (service->config.bit_depth == 0) {
        service->config.bit_depth = 16;
    }
    service->config.volume = fmaxf(0.0f, fminf(1.0f, service->config.volume));
    if (service->config.port == 0) {
        service->config.port = 10031;
    }
    if (service->config.buffer_size == 0) {
        service->config.buffer_size = 4096;
    }
    if (service->config.period_size == 0) {
        service->config.period_size = 1024;
    }

    // 初始化D-Bus
    if (!dbus_initialize()) {
        fprintf(stderr, "Failed to initialize D-Bus connection for RCA\n");
        // 继续初始化但记录警告
    }

    return service;
}

// 销毁RCA服务
void rca_destroy(RcaService* service) {
    if (!service) return;

    rca_stop(service);

    // 清理ALSA设备
    rca_cleanup_alsa_device(service);

    // 清理互斥锁
    pthread_mutex_destroy(&service->mutex);

    // 清理D-Bus
    dbus_cleanup();

    free(service);
}

// 启动RCA服务
int rca_start(RcaService* service) {
    if (!service || service->running) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 设置状态为启用中
    rca_set_state(service, RCA_STATE_ENABLED);

    // 设置ALSA设备
    if (rca_setup_alsa_device(service) < 0) {
        fprintf(stderr, "Failed to setup ALSA device\n");
        rca_set_state(service, RCA_STATE_ERROR);
        snprintf(service->error_msg, sizeof(service->error_msg), "ALSA device initialization failed");
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 创建PipeWire流
    if (rca_create_pipewire_stream(service) < 0) {
        fprintf(stderr, "Failed to create PipeWire stream\n");
        rca_cleanup_alsa_device(service);
        rca_set_state(service, RCA_STATE_ERROR);
        snprintf(service->error_msg, sizeof(service->error_msg), "PipeWire stream creation failed");
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 设置运行标志
    service->running = true;

    // 创建工作线程
    if (pthread_create(&service->thread, NULL, rca_worker_thread, service) != 0) {
        fprintf(stderr, "Failed to create RCA worker thread\n");
        service->running = false;
        rca_cleanup_alsa_device(service);
        if (service->stream) {
            pw_stream_destroy(service->stream);
            service->stream = NULL;
        }
        rca_set_state(service, RCA_STATE_ERROR);
        snprintf(service->error_msg, sizeof(service->error_msg), "Thread creation failed");
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 停止RCA服务
void rca_stop(RcaService* service) {
    if (!service || !service->running) {
        return;
    }

    pthread_mutex_lock(&service->mutex);
    service->running = false;
    pthread_mutex_unlock(&service->mutex);

    // 等待线程结束
    pthread_join(service->thread, NULL);

    // 清理资源
    rca_cleanup_alsa_device(service);

    if (service->stream) {
        pw_stream_destroy(service->stream);
        service->stream = NULL;
    }

    // 设置状态为禁用
    rca_set_state(service, RCA_STATE_DISABLED);
}

// 设置音量
int rca_set_volume(RcaService* service, float volume) {
    if (!service) {
        return -1;
    }

    volume = fmaxf(0.0f, fminf(1.0f, volume));

    pthread_mutex_lock(&service->mutex);
    service->session.volume = volume;
    pthread_mutex_unlock(&service->mutex);

    // 发送音量变化D-Bus信号
    json_t* details = json_object();
    json_object_set_new(details, "volume", json_real(volume));
    json_object_set_new(details, "muted", json_boolean(service->session.muted));
    json_object_set_new(details, "timestamp", json_integer(time(NULL)));

    const char* json_str = json_dumps(details, JSON_COMPACT);
    if (json_str) {
        dbus_emit_signal("com.realtimeaudio.RCA", DBUS_SIGNAL_TYPE_VOLUME_CHANGED, json_str);
        free((void*)json_str);
    }
    json_decref(details);

    return 0;
}

// 设置静音
int rca_set_mute(RcaService* service, bool muted) {
    if (!service) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);
    service->session.muted = muted;
    pthread_mutex_unlock(&service->mutex);

    // 发送静音变化D-Bus信号
    json_t* details = json_object();
    json_object_set_new(details, "muted", json_boolean(muted));
    json_object_set_new(details, "volume", json_real(service->session.volume));
    json_object_set_new(details, "timestamp", json_integer(time(NULL)));

    const char* json_str = json_dumps(details, JSON_COMPACT);
    if (json_str) {
        dbus_emit_signal("com.realtimeaudio.RCA", DBUS_SIGNAL_TYPE_MUTE_CHANGED, json_str);
        free((void*)json_str);
    }
    json_decref(details);

    return 0;
}

// 获取当前状态
RcaState rca_get_state(RcaService* service) {
    if (!service) {
        return RCA_STATE_ERROR;
    }

    RcaState state;
    pthread_mutex_lock(&service->mutex);
    state = service->state;
    pthread_mutex_unlock(&service->mutex);
    return state;
}

// 获取会话信息
const RcaSession* rca_get_session(RcaService* service) {
    if (!service || service->state == RCA_STATE_DISABLED || service->state == RCA_STATE_ERROR) {
        return NULL;
    }

    return &service->session;
}

// 获取错误信息
const char* rca_get_error(RcaService* service) {
    if (!service) {
        return "Invalid service pointer";
    }

    return service->error_msg;
}

// 设置状态
static void rca_set_state(RcaService* service, RcaState state) {
    if (!service) return;

    pthread_mutex_lock(&service->mutex);
    RcaState old_state = service->state;
    service->state = state;
    service->session.state = state;
    if (state == RCA_STATE_ACTIVE && old_state != RCA_STATE_ACTIVE) {
        service->session.active_time = time(NULL);
    }
    pthread_mutex_unlock(&service->mutex);

    // 只有状态实际改变时才发送信号
    if (old_state != state) {
        // 创建JSON payload
        json_t* details = json_object();
        json_object_set_new(details, "old_state", json_integer(old_state));
        json_object_set_new(details, "new_state", json_integer(state));
        json_object_set_new(details, "device_name", json_string(service->config.device_name));
        json_object_set_new(details, "alsa_device", json_string(service->config.alsa_device));
        json_object_set_new(details, "timestamp", json_integer(time(NULL)));

        const char* json_str = json_dumps(details, JSON_COMPACT);
        if (json_str) {
            // 发送D-Bus信号
            dbus_emit_signal("com.realtimeaudio.RCA", DBUS_SIGNAL_TYPE_STATE_CHANGED, json_str);
            free((void*)json_str);
        }
        json_decref(details);

        // 日志记录
        printf("RCA state changed from %d to %d\n", old_state, state);
    }
}

// 工作线程函数
static void* rca_worker_thread(void* data) {
    RcaService* service = (RcaService*)data;
    struct timespec timeout;

    while (1) {
        pthread_mutex_lock(&service->mutex);
        bool running = service->running;
        pthread_mutex_unlock(&service->mutex);

        if (!running) {
            break;
        }

        // 处理音频
        rca_process_audio(service);

        // 短暂休眠
        timeout.tv_sec = 0;
        timeout.tv_nsec = 10000000; // 10ms
        nanosleep(&timeout, NULL);
    }

    return NULL;
}

// 设置ALSA设备
static int rca_setup_alsa_device(RcaService* service) {
    int ret;
    snd_pcm_hw_params_t* params;
    unsigned int sample_rate = service->config.sample_rate;
    snd_pcm_uframes_t frames = service->config.period_size;

    // 打开PCM设备
    ret = snd_pcm_open(&service->session.pcm_handle, service->config.alsa_device, SND_PCM_STREAM_CAPTURE, 0);
    if (ret < 0) {
        fprintf(stderr, "Failed to open ALSA device %s: %s\n", service->config.alsa_device, snd_strerror(ret));
        return ret;
    }

    // 分配硬件参数对象
    snd_pcm_hw_params_alloca(&params);

    // 填充默认参数
    ret = snd_pcm_hw_params_any(service->session.pcm_handle, params);
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize ALSA hw params: %s\n", snd_strerror(ret));
        goto error;
    }

    // 设置交错模式
    ret = snd_pcm_hw_params_set_access(service->session.pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (ret < 0) {
        fprintf(stderr, "Failed to set interleaved mode: %s\n", snd_strerror(ret));
        goto error;
    }

    // 设置格式
    snd_pcm_format_t format;
    switch (service->config.bit_depth) {
        case 8: format = SND_PCM_FORMAT_U8; break;
        case 16: format = SND_PCM_FORMAT_S16_LE; break;
        case 24: format = SND_PCM_FORMAT_S24_LE; break;
        case 32: format = SND_PCM_FORMAT_S32_LE; break;
        default: format = SND_PCM_FORMAT_S16_LE; service->config.bit_depth = 16;
    }

    ret = snd_pcm_hw_params_set_format(service->session.pcm_handle, params, format);
    if (ret < 0) {
        fprintf(stderr, "Failed to set format: %s\n", snd_strerror(ret));
        goto error;
    }

    // 设置声道数
    ret = snd_pcm_hw_params_set_channels(service->session.pcm_handle, params, service->config.channels);
    if (ret < 0) {
        fprintf(stderr, "Failed to set channels: %s\n", snd_strerror(ret));
        goto error;
    }

    // 设置采样率
    ret = snd_pcm_hw_params_set_rate_near(service->session.pcm_handle, params, &sample_rate, NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to set sample rate: %s\n", snd_strerror(ret));
        goto error;
    }
    service->config.sample_rate = sample_rate;

    // 设置缓冲区大小
    ret = snd_pcm_hw_params_set_buffer_size_near(service->session.pcm_handle, params, &service->config.buffer_size);
    if (ret < 0) {
        fprintf(stderr, "Failed to set buffer size: %s\n", snd_strerror(ret));
        goto error;
    }

    // 设置周期大小
    ret = snd_pcm_hw_params_set_period_size_near(service->session.pcm_handle, params, &frames, NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to set period size: %s\n", snd_strerror(ret));
        goto error;
    }
    service->config.period_size = frames;

    // 应用参数
    ret = snd_pcm_hw_params(service->session.pcm_handle, params);
    if (ret < 0) {
        fprintf(stderr, "Failed to apply hw params: %s\n", snd_strerror(ret));
        goto error;
    }

    // 准备接口
    ret = snd_pcm_prepare(service->session.pcm_handle);
    if (ret < 0) {
        fprintf(stderr, "Failed to prepare PCM interface: %s\n", snd_strerror(ret));
        goto error;
    }

    // 配置音频格式信息
    service->session.format.format = format;
    service->session.format.channels = service->config.channels;
    service->session.format.rate = sample_rate;

    return 0;

error:
    snd_pcm_close(service->session.pcm_handle);
    service->session.pcm_handle = NULL;
    return ret;
}

// 清理ALSA设备
static void rca_cleanup_alsa_device(RcaService* service) {
    if (!service) return;

    pthread_mutex_lock(&service->mutex);
    if (service->session.pcm_handle) {
        snd_pcm_drain(service->session.pcm_handle);
        snd_pcm_close(service->session.pcm_handle);
        service->session.pcm_handle = NULL;
    }
    pthread_mutex_unlock(&service->mutex);
}

// 创建PipeWire流
static int rca_create_pipewire_stream(RcaService* service) {
    struct pw_properties* props;
    struct spa_audio_info_raw info = {
        .format = SPA_AUDIO_FORMAT_F32,
        .channels = service->config.channels,
        .rate = service->config.sample_rate,
    };
    const struct spa_pod* params[1];
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    // 创建流属性
    props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_STREAM_NAME, service->config.device_name,
        PW_KEY_DEVICE, service->config.alsa_device,
        NULL
    );

    if (!props) {
        fprintf(stderr, "Failed to create properties for RCA stream\n");
        return -1;
    }

    // 创建格式参数
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

    // 创建流
    service->stream = pw_stream_new_simple(
        service->context,
        "rca-stream",
        props,
        params
    );

    if (!service->stream) {
        fprintf(stderr, "Failed to create RCA stream\n");
        pw_properties_free(props);
        return -1;
    }

    return 0;
}

// 处理音频
static void rca_process_audio(RcaService* service) {
    if (!service || service->state != RCA_STATE_ENABLED || !service->session.pcm_handle || !service->stream) {
        return;
    }

    // 读取ALSA数据
    char buffer[service->config.buffer_size];
    snd_pcm_uframes_t frames = service->config.period_size;
    int ret = snd_pcm_readi(service->session.pcm_handle, buffer, frames);

    if (ret < 0) {
        fprintf(stderr, "ALSA read error: %s\n", snd_strerror(ret));
        // 尝试恢复
        if (ret == -EPIPE) {
            snd_pcm_prepare(service->session.pcm_handle);
        }
        return;
    }

    if (ret == 0) {
        // 没有数据
        return;
    }

    // 如果有数据且之前未激活，则更新状态
    if (service->state != RCA_STATE_ACTIVE) {
        rca_set_state(service, RCA_STATE_ACTIVE);
    }

    // 应用音量和静音
    pthread_mutex_lock(&service->mutex);
    float volume = service->session.volume;
    bool muted = service->session.muted;
    pthread_mutex_unlock(&service->mutex);

    if (muted) {
        volume = 0.0f;
    }

    // 应用音量（简化实现）
    if (volume != 1.0f) {
        // 根据位深度处理不同格式
        if (service->config.bit_depth == 16) {
            int16_t* samples = (int16_t*)buffer;
            for (int i = 0; i < ret * service->config.channels; i++) {
                samples[i] = (int16_t)(samples[i] * volume);
            }
        } else if (service->config.bit_depth == 32 && service->session.format.format == SND_PCM_FORMAT_S32_LE) {
            int32_t* samples = (int32_t*)buffer;
            for (int i = 0; i < ret * service->config.channels; i++) {
                samples[i] = (int32_t)(samples[i] * volume);
            }
        }
    }

    // 将数据写入PipeWire流
    struct pw_buffer* pw_buf = pw_stream_dequeue_buffer(service->stream);
    if (!pw_buf) {
        fprintf(stderr, "Failed to dequeue buffer\n");
        return;
    }

    struct spa_buffer* spa_buf = pw_buf->buffer;
    if (spa_buf->datas[0].data && spa_buf->datas[0].maxsize >= ret * service->config.channels * (service->config.bit_depth / 8)) {
        memcpy(spa_buf->datas[0].data, buffer, ret * service->config.channels * (service->config.bit_depth / 8));
        spa_buf->datas[0].chunk->offset = 0;
        spa_buf->datas[0].chunk->size = ret * service->config.channels * (service->config.bit_depth / 8);
        service->session.total_frames += ret;
    }

    pw_stream_queue_buffer(service->stream, pw_buf);
}