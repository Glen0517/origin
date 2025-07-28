#include "audio_processing.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// 前置声明
static void* create_eq_effect(const AudioProcessingParams* params, uint32_t sample_rate, uint8_t channels);
static void destroy_eq_effect(void* data);
static int apply_eq_effect(void* data, const float* input, float* output, uint32_t frames, uint8_t channels);

static void* create_compressor_effect(const AudioProcessingParams* params, uint32_t sample_rate, uint8_t channels);
static void destroy_compressor_effect(void* data);
static int apply_compressor_effect(void* data, const float* input, float* output, uint32_t frames, uint8_t channels);

static void* create_reverb_effect(const AudioProcessingParams* params, uint32_t sample_rate, uint8_t channels);
static void destroy_reverb_effect(void* data);
static int apply_reverb_effect(void* data, const float* input, float* output, uint32_t frames, uint8_t channels);

// 效果器创建函数表
typedef struct {
    void* (*create)(const AudioProcessingParams*, uint32_t, uint8_t);
    void (*destroy)(void*);
    int (*apply)(void*, const float*, float*, uint32_t, uint8_t);
} EffectHandler;

static EffectHandler effect_handlers[] = {
    [AUDIO_EFFECT_NONE] = {NULL, NULL, NULL},
    [AUDIO_EFFECT_EQUALIZER] = {create_eq_effect, destroy_eq_effect, apply_eq_effect},
    [AUDIO_EFFECT_COMPRESSOR] = {create_compressor_effect, destroy_compressor_effect, apply_compressor_effect},
    [AUDIO_EFFECT_REVERB] = {create_reverb_effect, destroy_reverb_effect, apply_reverb_effect},
    // 其他效果器的处理函数将在这里添加
};

AudioProcessingChain* audio_processing_chain_create(uint32_t sample_rate, uint8_t channels)
{
    if (channels < 1 || channels > 8) return NULL;
    if (sample_rate < 8000 || sample_rate > 192000) return NULL;

    AudioProcessingChain* chain = malloc(sizeof(AudioProcessingChain));
    if (!chain) return NULL;

    memset(chain, 0, sizeof(AudioProcessingChain));
    chain->sample_rate = sample_rate;
    chain->channels = channels;

    return chain;
}

void audio_processing_chain_destroy(AudioProcessingChain* chain)
{
    if (!chain) return;

    audio_processing_chain_clear(chain);
    free(chain);
}

AudioProcessingChain* audio_processing_chain_copy(const AudioProcessingChain* chain)
{
    if (!chain) return NULL;

    AudioProcessingChain* new_chain = audio_processing_chain_create(chain->sample_rate, chain->channels);
    if (!new_chain) return NULL;

    // 复制所有节点
    AudioProcessingNode* current = chain->head;
    while (current) {
        audio_processing_chain_add_node(new_chain, current->type, &current->params);
        current = current->next;
    }

    return new_chain;
}

int audio_processing_chain_add_node(AudioProcessingChain* chain, AudioEffectType type, const AudioProcessingParams* params)
{
    if (!chain || type <= AUDIO_EFFECT_NONE || type >= sizeof(effect_handlers)/sizeof(effect_handlers[0]) || !effect_handlers[type].create) {
        return -EINVAL;
    }

    // 创建处理节点
    AudioProcessingNode* node = malloc(sizeof(AudioProcessingNode));
    if (!node) return -ENOMEM;

    memset(node, 0, sizeof(AudioProcessingNode));
    node->type = type;
    node->params = *params;

    // 创建效果器数据
    node->effect_data = effect_handlers[type].create(params, chain->sample_rate, chain->channels);
    if (!node->effect_data) {
        free(node);
        return -ENOMEM;
    }

    // 添加到处理链
    if (!chain->head) {
        chain->head = node;
        chain->tail = node;
    } else {
        chain->tail->next = node;
        chain->tail = node;
    }

    chain->node_count++;
    return 0;
}

int audio_processing_chain_remove_node(AudioProcessingChain* chain, uint32_t index)
{
    if (!chain || index >= chain->node_count) return -EINVAL;

    AudioProcessingNode* current = chain->head;
    AudioProcessingNode* prev = NULL;
    uint32_t i = 0;

    // 找到要删除的节点
    while (current && i < index) {
        prev = current;
        current = current->next;
        i++;
    }

    if (!current) return -ENOENT;

    // 从链表中移除节点
    if (prev) {
        prev->next = current->next;
    } else {
        chain->head = current->next;
    }

    if (!current->next) {
        chain->tail = prev;
    }

    // 销毁效果器数据
    if (current->effect_data && effect_handlers[current->type].destroy) {
        effect_handlers[current->type].destroy(current->effect_data);
    }

    free(current);
    chain->node_count--;
    return 0;
}

AudioProcessingNode* audio_processing_chain_get_node(AudioProcessingChain* chain, uint32_t index)
{
    if (!chain || index >= chain->node_count) return NULL;

    AudioProcessingNode* current = chain->head;
    uint32_t i = 0;

    while (current && i < index) {
        current = current->next;
        i++;
    }

    return current;
}

void audio_processing_chain_clear(AudioProcessingChain* chain)
{
    if (!chain) return;

    AudioProcessingNode* current = chain->head;
    while (current) {
        AudioProcessingNode* next = current->next;

        // 销毁效果器数据
        if (current->effect_data && effect_handlers[current->type].destroy) {
            effect_handlers[current->type].destroy(current->effect_data);
        }

        free(current);
        current = next;
    }

    chain->head = NULL;
    chain->tail = NULL;
    chain->node_count = 0;
}

int audio_processing_apply(AudioProcessingChain* chain, const void* input, void* output, uint32_t frames, int format)
{
    if (!chain || !input || !output || frames == 0 || chain->node_count == 0) {
        return -EINVAL;
    }

    // 转换为浮点数格式进行处理
    float* float_input = malloc(frames * chain->channels * sizeof(float));
    float* float_output = malloc(frames * chain->channels * sizeof(float));
    if (!float_input || !float_output) {
        free(float_input);
        free(float_output);
        return -ENOMEM;
    }

    // 输入格式转换
    switch (format) {
        case 0: { // 16位整数
            const int16_t* in = (const int16_t*)input;
            for (uint32_t i = 0; i < frames * chain->channels; i++) {
                float_input[i] = (float)in[i] / INT16_MAX;
            }
            break;
        }
        case 1: { // 32位整数
            const int32_t* in = (const int32_t*)input;
            for (uint32_t i = 0; i < frames * chain->channels; i++) {
                float_input[i] = (float)in[i] / INT32_MAX;
            }
            break;
        }
        case 2: { // 32位浮点数
            memcpy(float_input, input, frames * chain->channels * sizeof(float));
            break;
        }
        default:
            free(float_input);
            free(float_output);
            return -EINVAL;
    }

    // 应用处理链
    float* current_input = float_input;
    float* current_output = float_output;

    AudioProcessingNode* node = chain->head;
    while (node) {
        if (effect_handlers[node->type].apply) {
            int ret = effect_handlers[node->type].apply(node->effect_data, current_input, current_output, frames, chain->channels);
            if (ret < 0) {
                free(float_input);
                free(float_output);
                return ret;
            }
        }

        // 交换输入输出缓冲区
        float* temp = current_input;
        current_input = current_output;
        current_output = temp;

        node = node->next;
    }

    // 如果节点数为奇数，结果在float_output中，否则在float_input中
    float* final_result = (chain->node_count % 2 == 0) ? float_input : float_output;

    // 应用主增益
    for (uint32_t i = 0; i < frames * chain->channels; i++) {
        final_result[i] *= chain->head->params.gain;
        // 防止削波
        if (final_result[i] > 1.0f) final_result[i] = 1.0f;
        if (final_result[i] < -1.0f) final_result[i] = -1.0f;
    }

    // 转换回输出格式
    switch (format) {
        case 0: { // 16位整数
            int16_t* out = (int16_t*)output;
            for (uint32_t i = 0; i < frames * chain->channels; i++) {
                out[i] = (int16_t)(final_result[i] * INT16_MAX);
            }
            break;
        }
        case 1: { // 32位整数
            int32_t* out = (int32_t*)output;
            for (uint32_t i = 0; i < frames * chain->channels; i++) {
                out[i] = (int32_t)(final_result[i] * INT32_MAX);
            }
            break;
        }
        case 2: { // 32位浮点数
            memcpy(output, final_result, frames * chain->channels * sizeof(float));
            break;
        }
    }

    free(float_input);
    free(float_output);
    return frames;
}

int audio_processing_update_params(AudioProcessingNode* node, const AudioProcessingParams* params)
{
    if (!node || !params) return -EINVAL;

    // 更新参数
    node->params = *params;

    // TODO: 根据效果器类型更新内部参数
    return 0;
}

// 均衡器效果实现
typedef struct {
    float bands[10];
    float* coefficients[10];
    uint32_t sample_rate;
    uint8_t channels;
} EQEffect;

static void* create_eq_effect(const AudioProcessingParams* params, uint32_t sample_rate, uint8_t channels)
{
    EQEffect* eq = malloc(sizeof(EQEffect));
    if (!eq) return NULL;

    memset(eq, 0, sizeof(EQEffect));
    memcpy(eq->bands, params->eq.bands, sizeof(eq->bands));
    eq->sample_rate = sample_rate;
    eq->channels = channels;

    // 初始化滤波器系数
    // TODO: 实现滤波器系数计算

    return eq;
}

static void destroy_eq_effect(void* data)
{
    if (!data) return;

    EQEffect* eq = (EQEffect*)data;
    for (int i = 0; i < 10; i++) {
        free(eq->coefficients[i]);
    }
    free(eq);
}

static int apply_eq_effect(void* data, const float* input, float* output, uint32_t frames, uint8_t channels)
{
    if (!data || !input || !output || frames == 0 || channels == 0) return -EINVAL;

    EQEffect* eq = (EQEffect*)data;
    memcpy(output, input, frames * channels * sizeof(float));

    // TODO: 实现均衡器处理算法

    return frames;
}

// 压缩器效果实现
typedef struct {
    float threshold; // dB
    float ratio;
    float attack;    // ms
    float release;   // ms
    float gain;      // 增益
    float env;       // 当前包络
    float attack_coeff;
    float release_coeff;
} CompressorEffect;

static void* create_compressor_effect(const AudioProcessingParams* params, uint32_t sample_rate, uint8_t channels)
{
    CompressorEffect* compressor = malloc(sizeof(CompressorEffect));
    if (!compressor) return NULL;

    memset(compressor, 0, sizeof(CompressorEffect));
    compressor->threshold = params->compressor.threshold;
    compressor->ratio = params->compressor.ratio;
    compressor->attack = params->compressor.attack;
    compressor->release = params->compressor.release;
    compressor->env = 0.0f;

    // 计算攻击和释放系数
    compressor->attack_coeff = expf(-1000.0f / (compressor->attack * sample_rate));
    compressor->release_coeff = expf(-1000.0f / (compressor->release * sample_rate));

    return compressor;
}

static void destroy_compressor_effect(void* data)
{
    free(data);
}

static int apply_compressor_effect(void* data, const float* input, float* output, uint32_t frames, uint8_t channels)
{
    if (!data || !input || !output || frames == 0 || channels == 0) return -EINVAL;

    CompressorEffect* compressor = (CompressorEffect*)data;

    for (uint32_t i = 0; i < frames; i++) {
        for (uint8_t c = 0; c < channels; c++) {
            const float sample = input[i * channels + c];
            float env_sample = fabsf(sample);

            // 转换为dB
            if (env_sample < 1e-6f) env_sample = -120.0f;
            else env_sample = 20.0f * log10f(env_sample);

            // 包络检测
            if (env_sample > compressor->env) {
                compressor->env = compressor->attack_coeff * compressor->env + (1.0f - compressor->attack_coeff) * env_sample;
            } else {
                compressor->env = compressor->release_coeff * compressor->env + (1.0f - compressor->release_coeff) * env_sample;
            }

            // 计算增益减少
            float gain_reduction = 0.0f;
            if (compressor->env > compressor->threshold) {
                gain_reduction = (compressor->env - compressor->threshold) * (1.0f / compressor->ratio - 1.0f);
            }

            // 应用增益
            float gain = powf(10.0f, -gain_reduction / 20.0f);
            output[i * channels + c] = sample * gain;
        }
    }

    return frames;
}

// 混响效果实现
typedef struct {
    float room_size;
    float damp;
    float wet;
    float dry;
    // 混响缓冲区和参数
    float* reverb_buf;
    uint32_t buf_size;
    uint32_t buf_idx;
} ReverbEffect;

static void* create_reverb_effect(const AudioProcessingParams* params, uint32_t sample_rate, uint8_t channels)
{
    ReverbEffect* reverb = malloc(sizeof(ReverbEffect));
    if (!reverb) return NULL;

    memset(reverb, 0, sizeof(ReverbEffect));
    reverb->room_size = params->reverb.room_size;
    reverb->damp = params->reverb.damp;
    reverb->wet = params->reverb.wet;
    reverb->dry = params->reverb.dry;

    // 计算混响缓冲区大小 (基于房间大小)
    reverb->buf_size = (uint32_t)(sample_rate * 2.0f * reverb->room_size); // 最大2秒
    reverb->reverb_buf = malloc(reverb->buf_size * sizeof(float));
    if (!reverb->reverb_buf) {
        free(reverb);
        return NULL;
    }
    memset(reverb->reverb_buf, 0, reverb->buf_size * sizeof(float));

    return reverb;
}

static void destroy_reverb_effect(void* data)
{
    if (!data) return;

    ReverbEffect* reverb = (ReverbEffect*)data;
    free(reverb->reverb_buf);
    free(reverb);
}

static int apply_reverb_effect(void* data, const float* input, float* output, uint32_t frames, uint8_t channels)
{
    if (!data || !input || !output || frames == 0 || channels == 0) return -EINVAL;

    ReverbEffect* reverb = (ReverbEffect*)data;

    // 简单的混响实现 (实际应用中应该使用更复杂的算法)
    for (uint32_t i = 0; i < frames; i++) {
        for (uint8_t c = 0; c < channels; c++) {
            const float sample = input[i * channels + c];
            uint32_t delay_idx = (reverb->buf_idx + reverb->buf_size - (uint32_t)(reverb->buf_size * 0.5f)) % reverb->buf_size;

            // 应用混响
            float reverb_sample = reverb->reverb_buf[delay_idx] * reverb->damp;
            reverb->reverb_buf[reverb->buf_idx] = sample + reverb_sample;
            reverb->buf_idx = (reverb->buf_idx + 1) % reverb->buf_size;

            // 混合干湿信号
            output[i * channels + c] = sample * reverb->dry + reverb_sample * reverb->wet;
        }
    }

    return frames;
}