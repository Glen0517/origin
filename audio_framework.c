#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

// 框架组件头文件
#include "pipewire_modules/stream_input.h"
#include "routing/routing_manager.h"
#include "audio_processing/audio_processing.h"
#include "audio_output/audio_output.h"

// 音频框架状态枚举
typedef enum {
    FRAMEWORK_STOPPED,
    FRAMEWORK_INITIALIZING,
    FRAMEWORK_RUNNING,
    FRAMEWORK_ERROR
} FrameworkState;

// 音频框架配置结构体
typedef struct {
    // 输入配置
    const char* input_device;
    uint32_t sample_rate;
    uint8_t channels;
    uint32_t buffer_size;
    // 输出配置
    const char* output_device;
    enum AudioOutputFormat output_format;
    int rt_priority;
    // 处理配置
    bool enable_processing;
} AudioFrameworkConfig;

// 音频框架结构体
typedef struct {
    FrameworkState state;
    AudioFrameworkConfig config;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    
    // 组件实例
    PipeWireStream* input_stream;
    RoutingManager* routing_manager;
    AudioProcessingChain* processing_chain;
    AudioOutputDevice* output_device;
    
    // 音频缓冲区
    void* audio_buffer;
    size_t buffer_size_bytes;
} AudioFramework;

// 全局框架实例
static AudioFramework* framework = NULL;
static volatile bool keep_running = true;

// 前向声明
static void handle_signal(int sig);
static void audio_data_callback(void* user_data, void* output_buffer, size_t frames);
static int initialize_components(AudioFramework* fw);
static void cleanup_components(AudioFramework* fw);

// 创建音频框架
AudioFramework* audio_framework_create(const AudioFrameworkConfig* config)
{
    if (!config || config->sample_rate == 0 || config->channels == 0 || config->buffer_size == 0) {
        fprintf(stderr, "Invalid framework configuration\n");
        return NULL;
    }

    // 初始化信号处理
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // 创建框架实例
    AudioFramework* fw = malloc(sizeof(AudioFramework));
    if (!fw) {
        fprintf(stderr, "Failed to allocate memory for framework\n");
        return NULL;
    }

    memset(fw, 0, sizeof(AudioFramework));
    fw->config = *config;
    fw->state = FRAMEWORK_STOPPED;
    pthread_mutex_init(&fw->mutex, NULL);
    pthread_cond_init(&fw->cond, NULL);

    // 计算缓冲区大小
    size_t bytes_per_sample = 0;
    switch (fw->config.output_format) {
        case AUDIO_FORMAT_S16_LE: bytes_per_sample = 2; break;
        case AUDIO_FORMAT_S32_LE: bytes_per_sample = 4; break;
        case AUDIO_FORMAT_FLOAT32_LE: bytes_per_sample = 4; break;
        default: bytes_per_sample = 2; break; // 默认16位
    }
    fw->buffer_size_bytes = config->buffer_size * config->channels * bytes_per_sample;

    // 分配音频缓冲区
    fw->audio_buffer = malloc(fw->buffer_size_bytes);
    if (!fw->audio_buffer) {
        fprintf(stderr, "Failed to allocate audio buffer\n");
        free(fw);
        return NULL;
    }

    framework = fw;
    return fw;
}

// 初始化框架组件
static int initialize_components(AudioFramework* fw)
{
    int ret = 0;

    // 1. 初始化路由管理器
    fw->routing_manager = routing_manager_create();
    if (!fw->routing_manager) {
        fprintf(stderr, "Failed to create routing manager\n");
        return -1;
    }

    // 2. 初始化音频处理链
    if (fw->config.enable_processing) {
        fw->processing_chain = audio_processing_chain_create(fw->config.sample_rate, fw->config.channels);
        if (!fw->processing_chain) {
            fprintf(stderr, "Failed to create processing chain\n");
            ret = -1;
            goto cleanup;
        }

        // 添加默认效果器 - 可以根据需要修改
        AudioProcessingParams params = {0};
        
        // 添加压缩器
        params.compressor.threshold = -18.0f; // -18dB
        params.compressor.ratio = 4.0f;       // 4:1
        params.compressor.attack = 10.0f;     // 10ms
        params.compressor.release = 100.0f;   // 100ms
        ret = audio_processing_chain_add_node(fw->processing_chain, AUDIO_EFFECT_COMPRESSOR, &params);
        if (ret < 0) {
            fprintf(stderr, "Failed to add compressor to processing chain\n");
            goto cleanup;
        }

        // 添加混响
        params.reverb.room_size = 0.5f;       // 50%
        params.reverb.damp = 0.5f;            // 50%
        params.reverb.wet = 0.3f;             // 30% wet
        params.reverb.dry = 0.7f;             // 70% dry
        ret = audio_processing_chain_add_node(fw->processing_chain, AUDIO_EFFECT_REVERB, &params);
        if (ret < 0) {
            fprintf(stderr, "Failed to add reverb to processing chain\n");
            goto cleanup;
        }
    }

    // 3. 初始化音频输出
    AudioOutputConfig output_config = {
        .device_name = fw->config.output_device,
        .format = fw->config.output_format,
        .sample_rate = fw->config.sample_rate,
        .channels = fw->config.channels,
        .buffer_size = fw->config.buffer_size,
        .period_size = fw->config.buffer_size / 4,
        .use_dma = true,
        .priority = fw->config.rt_priority
    };

    fw->output_device = audio_output_create(&output_config);
    if (!fw->output_device) {
        fprintf(stderr, "Failed to create audio output device\n");
        ret = -1;
        goto cleanup;
    }

    ret = audio_output_open(fw->output_device);
    if (ret < 0) {
        fprintf(stderr, "Failed to open audio output device\n");
        goto cleanup;
    }

    // 4. 初始化PipeWire输入流
    fw->input_stream = pipewire_stream_create(fw->config.sample_rate, fw->config.channels, fw->config.buffer_size);
    if (!fw->input_stream) {
        fprintf(stderr, "Failed to create PipeWire stream\n");
        ret = -1;
        goto cleanup;
    }

    ret = pipewire_stream_connect(fw->input_stream, fw->config.input_device);
    if (ret < 0) {
        fprintf(stderr, "Failed to connect PipeWire stream\n");
        goto cleanup;
    }

    return 0;

cleanup:
    if (fw->processing_chain) {
        audio_processing_chain_destroy(fw->processing_chain);
        fw->processing_chain = NULL;
    }
    if (fw->output_device) {
        audio_output_destroy(fw->output_device);
        fw->output_device = NULL;
    }
    if (fw->routing_manager) {
        routing_manager_destroy(fw->routing_manager);
        fw->routing_manager = NULL;
    }
    return ret;
}

// 清理框架组件
static void cleanup_components(AudioFramework* fw)
{
    if (!fw) return;

    // 停止输入流
    if (fw->input_stream) {
        pipewire_stream_disconnect(fw->input_stream);
        pipewire_stream_destroy(fw->input_stream);
        fw->input_stream = NULL;
    }

    // 停止输出
    if (fw->output_device) {
        audio_output_close(fw->output_device);
        audio_output_destroy(fw->output_device);
        fw->output_device = NULL;
    }

    // 销毁处理链
    if (fw->processing_chain) {
        audio_processing_chain_destroy(fw->processing_chain);
        fw->processing_chain = NULL;
    }

    // 销毁路由管理器
    if (fw->routing_manager) {
        routing_manager_destroy(fw->routing_manager);
        fw->routing_manager = NULL;
    }

    // 释放缓冲区
    if (fw->audio_buffer) {
        free(fw->audio_buffer);
        fw->audio_buffer = NULL;
    }

    pthread_mutex_destroy(&fw->mutex);
    pthread_cond_destroy(&fw->cond);
    free(fw);
    framework = NULL;
}

// 音频数据回调函数
static void audio_data_callback(void* user_data, void* output_buffer, size_t frames)
{
    AudioFramework* fw = (AudioFramework*)user_data;
    if (!fw || fw->state != FRAMEWORK_RUNNING) return;

    pthread_mutex_lock(&fw->mutex);

    // 1. 从输入流获取音频数据
    int ret = pipewire_stream_read(fw->input_stream, fw->audio_buffer, frames);
    if (ret < 0) {
        fprintf(stderr, "Failed to read from input stream: %d\n", ret);
        pthread_mutex_unlock(&fw->mutex);
        return;
    }

    // 2. 应用路由逻辑
    // 这里简化处理，实际应用中应根据路由规则处理
    AudioBuffer input_buffer = {
        .data = fw->audio_buffer,
        .frames = frames,
        .channels = fw->config.channels,
        .format = fw->config.output_format,
        .sample_rate = fw->config.sample_rate
    };

    AudioBuffer output_buffer = {
        .data = fw->audio_buffer,
        .frames = frames,
        .channels = fw->config.channels,
        .format = fw->config.output_format,
        .sample_rate = fw->config.sample_rate
    };

    routing_manager_route_audio(fw->routing_manager, &input_buffer, &output_buffer);

    // 3. 应用音频处理
    if (fw->config.enable_processing && fw->processing_chain) {
        int format = 0;
        switch (fw->config.output_format) {
            case AUDIO_FORMAT_S16_LE: format = 0; break;
            case AUDIO_FORMAT_S32_LE: format = 1; break;
            case AUDIO_FORMAT_FLOAT32_LE: format = 2; break;
            default: format = 0; break;
        }

        ret = audio_processing_apply(fw->processing_chain, fw->audio_buffer, output_buffer.data, frames, format);
        if (ret < 0) {
            fprintf(stderr, "Failed to apply audio processing: %d\n", ret);
            pthread_mutex_unlock(&fw->mutex);
            return;
        }
    }

    // 4. 复制到输出缓冲区
    memcpy(output_buffer, fw->audio_buffer, frames * fw->config.channels * format_to_bytes(fw->config.output_format));

    pthread_mutex_unlock(&fw->mutex);
}

// 启动音频框架
int audio_framework_start(AudioFramework* fw)
{
    if (!fw || fw->state != FRAMEWORK_STOPPED) {
        return -EINVAL;
    }

    fw->state = FRAMEWORK_INITIALIZING;

    // 初始化组件
    int ret = initialize_components(fw);
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize components\n");
        fw->state = FRAMEWORK_ERROR;
        cleanup_components(fw);
        return ret;
    }

    // 启动输出设备
    ret = audio_output_start(fw->output_device, audio_data_callback, fw);
    if (ret < 0) {
        fprintf(stderr, "Failed to start audio output\n");
        fw->state = FRAMEWORK_ERROR;
        cleanup_components(fw);
        return ret;
    }

    fw->state = FRAMEWORK_RUNNING;
    printf("Audio framework started successfully\n");

    // 等待退出信号
    while (keep_running) {
        sleep(1);
    }

    // 停止框架
    fw->state = FRAMEWORK_STOPPED;
    cleanup_components(fw);
    printf("Audio framework stopped\n");

    return 0;
}

// 销毁音频框架
void audio_framework_destroy(AudioFramework* fw)
{
    if (!fw) return;

    if (fw->state == FRAMEWORK_RUNNING) {
        fw->state = FRAMEWORK_STOPPED;
    }

    cleanup_components(fw);
}

// 信号处理函数
static void handle_signal(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        printf("Received stop signal, shutting down...\n");
        keep_running = false;
    }
}

// 主函数示例
int main(int argc, char* argv[])
{
    // 默认配置
    AudioFrameworkConfig config = {
        .input_device = NULL,          // 使用默认输入设备
        .sample_rate = 48000,          // 48kHz
        .channels = 2,                 // 立体声
        .buffer_size = 1024,           // 缓冲区大小
        .output_device = NULL,         // 使用默认输出设备
        .output_format = AUDIO_FORMAT_S16_LE, // 16位整数
        .rt_priority = 80,             // 实时优先级
        .enable_processing = true      // 启用音频处理
    };

    // 创建并启动框架
    AudioFramework* framework = audio_framework_create(&config);
    if (!framework) {
        fprintf(stderr, "Failed to create audio framework\n");
        return 1;
    }

    int ret = audio_framework_start(framework);
    if (ret < 0) {
        fprintf(stderr, "Failed to start audio framework\n");
        audio_framework_destroy(framework);
        return 1;
    }

    return 0;
}