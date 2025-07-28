#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>

#include "flowdsp.h"

// 前向声明
static int flow_dsp_node_init(FlowDspNode* node, FlowDspEffectType type, const char* name,
                             const FlowDspEffectParams* params, struct pw_context* context);
static void flow_dsp_node_destroy(FlowDspNode* node);
static void flow_dsp_process_equalizer(FlowDspNode* node, float* input, float* output, int n_samples);
static void flow_dsp_process_compressor(FlowDspNode* node, float* input, float* output, int n_samples);
static void flow_dsp_process_reverb(FlowDspNode* node, float* input, float* output, int n_samples);
static void flow_dsp_process_distortion(FlowDspNode* node, float* input, float* output, int n_samples);
static void flow_dsp_process_chorus(FlowDspNode* node, float* input, float* output, int n_samples);
static void flow_dsp_process_pitch_shift(FlowDspNode* node, float* input, float* output, int n_samples);

// 创建DSP处理链
FlowDspChain* flow_dsp_chain_create(struct pw_context* context, const struct spa_audio_info* format) {
    if (!context || !format) {
        fprintf(stderr, "Invalid parameters for flow_dsp_chain_create\n");
        return NULL;
    }

    FlowDspChain* chain = calloc(1, sizeof(FlowDspChain));
    if (!chain) {
        fprintf(stderr, "Failed to allocate memory for FlowDspChain\n");
        return NULL;
    }

    // 初始化互斥锁
    pthread_mutex_init(&chain->mutex, NULL);

    // 初始化钩子列表
    spa_hook_list_init(&chain->hooks);

    // 设置上下文和格式
    chain->context = context;
    chain->format = *format;
    chain->node_count = 0;

    return chain;
}

// 销毁DSP处理链
void flow_dsp_chain_destroy(FlowDspChain* chain) {
    if (!chain) return;

    pthread_mutex_lock(&chain->mutex);

    // 销毁所有节点
    for (int i = 0; i < chain->node_count; i++) {
        flow_dsp_node_destroy(chain->nodes[i]);
    }

    // 销毁互斥锁
    pthread_mutex_unlock(&chain->mutex);
    pthread_mutex_destroy(&chain->mutex);

    free(chain);
}

// 添加效果节点到处理链
int flow_dsp_chain_add_node(FlowDspChain* chain, FlowDspEffectType type, const char* name,
                           const FlowDspEffectParams* params) {
    if (!chain || type >= FLOW_DSP_MAX || !name || !params) {
        return -EINVAL;
    }

    pthread_mutex_lock(&chain->mutex);

    // 检查节点数量限制
    if (chain->node_count >= 16) {
        pthread_mutex_unlock(&chain->mutex);
        return -ENOSPC;
    }

    // 创建节点
    FlowDspNode* node = calloc(1, sizeof(FlowDspNode));
    if (!node) {
        pthread_mutex_unlock(&chain->mutex);
        return -ENOMEM;
    }

    // 初始化节点
    int ret = flow_dsp_node_init(node, type, name, params, chain->context);
    if (ret != 0) {
        free(node);
        pthread_mutex_unlock(&chain->mutex);
        return ret;
    }

    // 添加到链
    chain->nodes[chain->node_count++] = node;

    pthread_mutex_unlock(&chain->mutex);
    return 0;
}

// 从处理链移除效果节点
int flow_dsp_chain_remove_node(FlowDspChain* chain, int index) {
    if (!chain || index < 0 || index >= chain->node_count) {
        return -EINVAL;
    }

    pthread_mutex_lock(&chain->mutex);

    // 销毁节点
    flow_dsp_node_destroy(chain->nodes[index]);

    // 移动后续节点
    for (int i = index; i < chain->node_count - 1; i++) {
        chain->nodes[i] = chain->nodes[i + 1];
    }

    chain->node_count--;

    pthread_mutex_unlock(&chain->mutex);
    return 0;
}

// 获取效果节点
FlowDspNode* flow_dsp_chain_get_node(FlowDspChain* chain, int index) {
    if (!chain || index < 0 || index >= chain->node_count) {
        return NULL;
    }

    return chain->nodes[index];
}

// 初始化DSP节点
static int flow_dsp_node_init(FlowDspNode* node, FlowDspEffectType type, const char* name,
                             const FlowDspEffectParams* params, struct pw_context* context) {
    int ret = 0;

    // 初始化互斥锁
    pthread_mutex_init(&node->mutex, NULL);

    // 设置基本信息
    node->type = type;
    node->active = true;
    strncpy(node->name, name, sizeof(node->name) - 1);
    node->name[sizeof(node->name) - 1] = '\0';

    // 复制参数
    node->params = *params;

    // 创建SPA节点 (简化实现)
    node->node = NULL; // 实际实现中应创建真实的SPA节点
    node->props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Filter",
        PW_KEY_MEDIA_ROLE, "DSP",
        PW_KEY_NODE_NAME, node->name,
        NULL);

    if (!node->props) {
        ret = -ENOMEM;
        goto error;
    }

    return 0;

error:
    pthread_mutex_destroy(&node->mutex);
    if (node->props) {
        pw_properties_free(node->props);
    }
    return ret;
}

// 销毁DSP节点
static void flow_dsp_node_destroy(FlowDspNode* node) {
    if (!node) return;

    pthread_mutex_lock(&node->mutex);

    if (node->props) {
        pw_properties_free(node->props);
    }

    if (node->node) {
        // 实际实现中应销毁SPA节点
    }

    pthread_mutex_unlock(&node->mutex);
    pthread_mutex_destroy(&node->mutex);
    free(node);
}

// 更新效果节点参数
int flow_dsp_node_update_params(FlowDspNode* node, const FlowDspEffectParams* params) {
    if (!node || !params) {
        return -EINVAL;
    }

    pthread_mutex_lock(&node->mutex);
    node->params = *params;
    pthread_mutex_unlock(&node->mutex);

    return 0;
}

// 激活/禁用效果节点
int flow_dsp_node_set_active(FlowDspNode* node, bool active) {
    if (!node) {
        return -EINVAL;
    }

    pthread_mutex_lock(&node->mutex);
    node->active = active;
    pthread_mutex_unlock(&node->mutex);

    return 0;
}

// 连接DSP处理链到PipeWire流
int flow_dsp_chain_connect(FlowDspChain* chain, struct pw_stream* stream) {
    if (!chain || !stream) {
        return -EINVAL;
    }

    // 实际实现中应连接DSP链到流
    return 0;
}

// 断开DSP处理链连接
void flow_dsp_chain_disconnect(FlowDspChain* chain) {
    if (!chain) return;

    // 实际实现中应断开连接
}

// 处理音频数据通过DSP链
void flow_dsp_chain_process(FlowDspChain* chain, float* input, float* output, int n_samples) {
    if (!chain || !input || !output || n_samples <= 0) {
        return;
    }

    // 如果没有节点，直接复制输入到输出
    if (chain->node_count == 0) {
        memcpy(output, input, n_samples * sizeof(float));
        return;
    }

    pthread_mutex_lock(&chain->mutex);

    // 创建临时缓冲区
    float* temp_in = malloc(n_samples * sizeof(float));
    float* temp_out = malloc(n_samples * sizeof(float));
    if (!temp_in || !temp_out) {
        pthread_mutex_unlock(&chain->mutex);
        return;
    }

    // 初始输入是原始输入
    memcpy(temp_in, input, n_samples * sizeof(float));

    // 通过每个激活的节点处理音频
    for (int i = 0; i < chain->node_count; i++) {
        FlowDspNode* node = chain->nodes[i];

        pthread_mutex_lock(&node->mutex);

        if (node->active) {
            // 根据效果类型处理
            switch (node->type) {
                case FLOW_DSP_EQUALIZER:
                    flow_dsp_process_equalizer(node, temp_in, temp_out, n_samples);
                    break;
                case FLOW_DSP_COMPRESSOR:
                    flow_dsp_process_compressor(node, temp_in, temp_out, n_samples);
                    break;
                case FLOW_DSP_REVERB:
                    flow_dsp_process_reverb(node, temp_in, temp_out, n_samples);
                    break;
                case FLOW_DSP_DISTORTION:
                    flow_dsp_process_distortion(node, temp_in, temp_out, n_samples);
                    break;
                case FLOW_DSP_CHORUS:
                    flow_dsp_process_chorus(node, temp_in, temp_out, n_samples);
                    break;
                case FLOW_DSP_PITCH_SHIFT:
                    flow_dsp_process_pitch_shift(node, temp_in, temp_out, n_samples);
                    break;
                default:
                    memcpy(temp_out, temp_in, n_samples * sizeof(float));
                    break;
            }

            // 交换缓冲区
            float* swap = temp_in;
            temp_in = temp_out;
            temp_out = swap;
        }

        pthread_mutex_unlock(&node->mutex);
    }

    // 将最终结果复制到输出
    memcpy(output, temp_in, n_samples * sizeof(float));

    // 清理
    free(temp_in);
    free(temp_out);

    pthread_mutex_unlock(&chain->mutex);
}

// 均衡器处理
static void flow_dsp_process_equalizer(FlowDspNode* node, float* input, float* output, int n_samples) {
    // 简化实现：应用增益
    pthread_mutex_lock(&node->mutex);
    FlowDspEqualizerParams* params = &node->params.eq;

    // 将dB转换为线性增益
    float gains[10];
    for (int i = 0; i < 10; i++) {
        gains[i] = powf(10.0f, params->eq.bands[i] / 20.0f);
    }

    // 简化实现：应用平均增益
    float avg_gain = 1.0f;
    for (int i = 0; i < 10; i++) {
        avg_gain += gains[i];
    }
    avg_gain /= 10.0f;

    // 应用增益到所有样本
    for (int i = 0; i < n_samples; i++) {
        output[i] = input[i] * avg_gain;
    }

    pthread_mutex_unlock(&node->mutex);
}

// 压缩器处理
static void flow_dsp_process_compressor(FlowDspNode* node, float* input, float* output, int n_samples) {
    pthread_mutex_lock(&node->mutex);
    FlowDspCompressorParams* params = &node->params.compressor;

    // 简化实现：硬拐点压缩
    const float threshold = powf(10.0f, params->threshold / 20.0f);
    const float ratio = params->ratio;
    const float makeup_gain = powf(10.0f, params->makeup_gain / 20.0f);

    for (int i = 0; i < n_samples; i++) {
        float sample = input[i];
        float abs_sample = fabsf(sample);

        // 如果超过阈值，应用压缩
        if (abs_sample > threshold) {
            float gain = threshold * powf(abs_sample / threshold, 1.0f / ratio - 1.0f);
            sample = sample > 0 ? gain : -gain;
        }

        // 应用makeup增益
        output[i] = sample * makeup_gain;
    }

    pthread_mutex_unlock(&node->mutex);
}

// 混响处理
static void flow_dsp_process_reverb(FlowDspNode* node, float* input, float* output, int n_samples) {
    pthread_mutex_lock(&node->mutex);
    FlowDspReverbParams* params = &node->params.reverb;

    // 简化实现：基本混响效果
    const float wet = params->wet;
    const float dry = params->dry;

    // 简单的延迟线混响
    static float reverb_buffer[44100] = {0};
    static int reverb_index = 0;
    const int delay_samples = (int)(params->room_size * 0.1f * 44100); // 简化：假设44100采样率

    for (int i = 0; i < n_samples; i++) {
        // 保存当前样本
        float current = input[i];

        // 获取延迟样本
        int delay_index = (reverb_index + delay_samples) % 44100;
        float delayed = reverb_buffer[delay_index] * 0.5f; // 衰减

        // 输出是干信号+湿信号
        output[i] = current * dry + delayed * wet;

        // 更新缓冲区
        reverb_buffer[reverb_index] = current + delayed * 0.3f; // 反馈
        reverb_index = (reverb_index + 1) % 44100;
    }

    pthread_mutex_unlock(&node->mutex);
}

// 失真处理
static void flow_dsp_process_distortion(FlowDspNode* node, float* input, float* output, int n_samples) {
    pthread_mutex_lock(&node->mutex);
    FlowDspDistortionParams* params = &node->params.distortion;

    const float drive = params->drive * 10.0f; // 驱动量
    const float tone = params->tone;
    const float mix = params->mix;

    for (int i = 0; i < n_samples; i++) {
        float sample = input[i] * drive;

        // 硬削波失真
        if (sample > 0.5f) sample = 0.5f;
        else if (sample < -0.5f) sample = -0.5f;

        // 简单的音调控制
        sample = sample * (0.5f + tone * 0.5f);

        // 混合干/湿信号
        output[i] = input[i] * (1.0f - mix) + sample * mix;
    }

    pthread_mutex_unlock(&node->mutex);
}

// 合唱处理
static void flow_dsp_process_chorus(FlowDspNode* node, float* input, float* output, int n_samples) {
    pthread_mutex_lock(&node->mutex);
    FlowDspChorusParams* params = &node->params.chorus;

    const float rate = params->rate;
    const float depth = params->depth * 0.01f;
    const float feedback = params->feedback;
    const float mix = params->mix;

    // 简化实现：基本合唱效果
    static float chorus_buffer[2][8820] = {{0}}; // 两个延迟线
    static int chorus_index = 0;
    static float lfo_phase = 0.0f;

    for (int i = 0; i < n_samples; i++) {
        // LFO计算
        lfo_phase += 2.0f * M_PI * rate / 44100.0f; // 简化：假设44100采样率
        if (lfo_phase >= 2.0f * M_PI) lfo_phase -= 2.0f * M_PI;

        // 两个延迟线的LFO偏移
        float lfo1 = sinf(lfo_phase) * depth;
        float lfo2 = sinf(lfo_phase + M_PI) * depth;

        // 延迟样本计算
        int delay1 = (int)((0.01f + lfo1) * 44100);
        int delay2 = (int)((0.012f + lfo2) * 44100);

        // 获取延迟样本
        int index1 = (chorus_index + delay1) % 8820;
        int index2 = (chorus_index + delay2) % 8820;

        float delayed1 = chorus_buffer[0][index1] * 0.7f;
        float delayed2 = chorus_buffer[1][index2] * 0.7f;

        // 反馈
        float feedback_sample = input[i] + (delayed1 + delayed2) * feedback;

        // 写入缓冲区
        chorus_buffer[0][chorus_index] = feedback_sample;
        chorus_buffer[1][chorus_index] = feedback_sample;

        // 混合
        output[i] = input[i] * (1.0f - mix) + (delayed1 + delayed2) * mix * 0.5f;

        chorus_index = (chorus_index + 1) % 8820;
    }

    pthread_mutex_unlock(&node->mutex);
}

// 音调偏移处理
static void flow_dsp_process_pitch_shift(FlowDspNode* node, float* input, float* output, int n_samples) {
    pthread_mutex_lock(&node->mutex);
    FlowDspPitchShiftParams* params = &node->params.pitch_shift;

    // 简化实现：基本音调偏移（仅概念验证）
    const float shift = params->shift;
    const float pitch_ratio = powf(2.0f, shift / 12.0f);

    // 简单的重采样（质量低，仅作示例）
    for (int i = 0; i < n_samples; i++) {
        float read_index = i * pitch_ratio;
        int index = (int)read_index;
        float frac = read_index - index;

        if (index + 1 < n_samples) {
            // 线性插值
            output[i] = input[index] * (1.0f - frac) + input[index + 1] * frac;
        } else {
            output[i] = input[index];
        }
    }

    pthread_mutex_unlock(&node->mutex);
}