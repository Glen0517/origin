#ifndef AUDIO_PROCESSING_H
#define AUDIO_PROCESSING_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/**
 * 音频处理效果类型
 */
typedef enum {
    AUDIO_EFFECT_NONE,        // 无效果
    AUDIO_EFFECT_EQUALIZER,   // 均衡器
    AUDIO_EFFECT_COMPRESSOR,  // 压缩器
    AUDIO_EFFECT_REVERB,      // 混响
    AUDIO_EFFECT_DISTORTION,  // 失真
    AUDIO_EFFECT_CHORUS,      // 合唱
    AUDIO_EFFECT_PITCH_SHIFT  // 音调偏移
} AudioEffectType;

/**
 * 音频处理参数
 */
typedef struct {
    float gain;               // 增益 (0.0 - 2.0)
    uint32_t sample_rate;     // 采样率
    uint8_t channels;         // 声道数
    union {
        // 均衡器参数
        struct {
            float bands[10];  // 10段均衡器 (Hz: 31, 62, 125, 250, 500, 1k, 2k, 4k, 8k, 16k)
        } eq;
        // 压缩器参数
        struct {
            float threshold;  // 阈值 (-40.0 - 0.0 dB)
            float ratio;      // 比率 (1.0 - 20.0)
            float attack;     // 攻击时间 (0.1 - 100 ms)
            float release;    // 释放时间 (10 - 1000 ms)
        } compressor;
        // 混响参数
        struct {
            float room_size;  // 房间大小 (0.0 - 1.0)
            float damp;       // 阻尼 (0.0 - 1.0)
            float wet;        // 湿信号比例 (0.0 - 1.0)
            float dry;        // 干信号比例 (0.0 - 1.0)
        } reverb;
        // 失真参数
        struct {
            float drive;      // 驱动 (0.0 - 1.0)
            float tone;       // 音色 (0.0 - 1.0)
        } distortion;
        // 合唱参数
        struct {
            float rate;       // 速率 (0.1 - 10.0 Hz)
            float depth;      // 深度 (0.0 - 1.0)
            float feedback;   // 反馈 (0.0 - 0.9)
        } chorus;
        // 音调偏移参数
        struct {
            float shift;      // 偏移量 (-12.0 - 12.0 半音)
        } pitch_shift;
    } params;
} AudioProcessingParams;

/**
 * 音频处理节点
 */
typedef struct AudioProcessingNode {
    AudioEffectType type;               // 效果类型
    AudioProcessingParams params;       // 参数
    void* effect_data;                  // 效果器内部数据
    struct AudioProcessingNode* next;   // 下一个处理节点
} AudioProcessingNode;

/**
 * 音频处理链
 */
typedef struct {
    AudioProcessingNode* head;          // 处理链头部
    AudioProcessingNode* tail;          // 处理链尾部
    uint32_t node_count;                // 节点数量
    uint32_t sample_rate;               // 采样率
    uint8_t channels;                   // 声道数
} AudioProcessingChain;

/**
 * 创建音频处理链
 * 
 * @param sample_rate 采样率
 * @param channels 声道数
 * @return 音频处理链指针，失败返回NULL
 */
AudioProcessingChain* audio_processing_chain_create(uint32_t sample_rate, uint8_t channels);

/**
 * 销毁音频处理链
 * 
 * @param chain 音频处理链
 */
void audio_processing_chain_destroy(AudioProcessingChain* chain);

/**
 * 复制音频处理链
 * 
 * @param chain 要复制的音频处理链
 * @return 新的音频处理链，失败返回NULL
 */
AudioProcessingChain* audio_processing_chain_copy(const AudioProcessingChain* chain);

/**
 * 向处理链添加效果器节点
 * 
 * @param chain 音频处理链
 * @param type 效果类型
 * @param params 效果参数
 * @return 成功返回0，失败返回负数错误码
 */
int audio_processing_chain_add_node(AudioProcessingChain* chain, AudioEffectType type, const AudioProcessingParams* params);

/**
 * 从处理链移除效果器节点
 * 
 * @param chain 音频处理链
 * @param index 节点索引
 * @return 成功返回0，失败返回负数错误码
 */
int audio_processing_chain_remove_node(AudioProcessingChain* chain, uint32_t index);

/**
 * 获取处理链中的节点
 * 
 * @param chain 音频处理链
 * @param index 节点索引
 * @return 节点指针，失败返回NULL
 */
AudioProcessingNode* audio_processing_chain_get_node(AudioProcessingChain* chain, uint32_t index);

/**
 * 清空处理链
 * 
 * @param chain 音频处理链
 */
void audio_processing_chain_clear(AudioProcessingChain* chain);

/**
 * 应用音频处理链到音频缓冲区
 * 
 * @param chain 音频处理链
 * @param input 输入缓冲区
 * @param output 输出缓冲区
 * @param frames 帧数
 * @param format 音频格式 (0: 16位整数, 1: 32位整数, 2: 32位浮点数)
 * @return 成功返回处理的帧数，失败返回负数错误码
 */
int audio_processing_apply(AudioProcessingChain* chain, const void* input, void* output, uint32_t frames, int format);

/**
 * 更新音频处理节点参数
 * 
 * @param node 音频处理节点
 * @param params 新的参数
 * @return 成功返回0，失败返回负数错误码
 */
int audio_processing_update_params(AudioProcessingNode* node, const AudioProcessingParams* params);

#endif // AUDIO_PROCESSING_H