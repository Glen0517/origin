#ifndef FLOW_DSP_H
#define FLOW_DSP_H

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/param.h>
#include <spa/support/type-map.h>
#include <spa/node/node.h>
#include <spa/debug/types.h>
#include <pthread.h>

// DSP效果类型枚举
typedef enum {
    FLOW_DSP_EQUALIZER,    // 均衡器
    FLOW_DSP_COMPRESSOR,   // 压缩器
    FLOW_DSP_REVERB,       // 混响
    FLOW_DSP_DISTORTION,   // 失真
    FLOW_DSP_CHORUS,       // 合唱
    FLOW_DSP_PITCH_SHIFT,  // 音调偏移
    FLOW_DSP_MAX           // 效果类型数量
} FlowDspEffectType;

// 均衡器参数
typedef struct {
    float bands[10];       // 10段均衡器频率增益 (dB)
    float sample_rate;     // 采样率
} FlowDspEqualizerParams;

// 压缩器参数
typedef struct {
    float threshold;       // 阈值 (-60.0 to 0.0 dB)
    float ratio;           // 比率 (1.0 to 20.0)
    float attack;          // 攻击时间 (0.1 to 500 ms)
    float release;         // 释放时间 (10 to 2000 ms)
    float knee;            // 拐点 (0.0 to 24.0 dB)
    float makeup_gain;     //  Makeup增益 (-20.0 to 20.0 dB)
} FlowDspCompressorParams;

// 混响参数
typedef struct {
    float room_size;       // 房间大小 (0.0 to 1.0)
    float damp;            // 阻尼 (0.0 to 1.0)
    float wet;             // 湿信号比例 (0.0 to 1.0)
    float dry;             // 干信号比例 (0.0 to 1.0)
    float width;           // 立体声宽度 (0.0 to 1.0)
} FlowDspReverbParams;

// 失真参数
typedef struct {
    float drive;           // 驱动量 (0.0 to 1.0)
    float tone;            // 音调 (0.0 to 1.0)
    float mix;             // 混合比例 (0.0 to 1.0)
} FlowDspDistortionParams;

// 合唱参数
typedef struct {
    float rate;            // 速率 (0.1 to 10.0 Hz)
    float depth;           // 深度 (0.0 to 1.0)
    float feedback;        // 反馈 (0.0 to 0.9)
    float mix;             // 混合比例 (0.0 to 1.0)
} FlowDspChorusParams;

// 音调偏移参数
typedef struct {
    float shift;           // 偏移量 (-12.0 to 12.0 semitones)
    int quality;           // 质量 (0:低, 1:中, 2:高)
} FlowDspPitchShiftParams;

// DSP效果参数联合
typedef union {
    FlowDspEqualizerParams eq;
    FlowDspCompressorParams compressor;
    FlowDspReverbParams reverb;
    FlowDspDistortionParams distortion;
    FlowDspChorusParams chorus;
    FlowDspPitchShiftParams pitch_shift;
} FlowDspEffectParams;

// DSP效果节点
typedef struct {
    FlowDspEffectType type;        // 效果类型
    char name[64];                 // 节点名称
    struct spa_node* node;         // SPA节点
    struct pw_properties* props;   // 属性
    FlowDspEffectParams params;    // 参数
    bool active;                   // 是否激活
    pthread_mutex_t mutex;         // 互斥锁
} FlowDspNode;

// DSP处理链
typedef struct {
    FlowDspNode* nodes[16];        // 节点列表
    int node_count;                // 节点数量
    struct spa_hook_list hooks;    // 钩子列表
    struct pw_context* context;    // PipeWire上下文
    struct spa_audio_info format;  // 音频格式
    pthread_mutex_t mutex;         // 互斥锁
} FlowDspChain;

// 创建DSP处理链
FlowDspChain* flow_dsp_chain_create(struct pw_context* context, const struct spa_audio_info* format);

// 销毁DSP处理链
void flow_dsp_chain_destroy(FlowDspChain* chain);

// 添加效果节点到处理链
int flow_dsp_chain_add_node(FlowDspChain* chain, FlowDspEffectType type, const char* name, const FlowDspEffectParams* params);

// 从处理链移除效果节点
int flow_dsp_chain_remove_node(FlowDspChain* chain, int index);

// 获取效果节点
FlowDspNode* flow_dsp_chain_get_node(FlowDspChain* chain, int index);

// 更新效果节点参数
int flow_dsp_node_update_params(FlowDspNode* node, const FlowDspEffectParams* params);

// 激活/禁用效果节点
int flow_dsp_node_set_active(FlowDspNode* node, bool active);

// 连接DSP处理链到PipeWire流
int flow_dsp_chain_connect(FlowDspChain* chain, struct pw_stream* stream);

// 断开DSP处理链连接
void flow_dsp_chain_disconnect(FlowDspChain* chain);

#endif // FLOW_DSP_H