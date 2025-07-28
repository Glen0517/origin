#ifndef __RCA_H__
#define __RCA_H__

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <alsa/asoundlib.h>
#include <pipewire/pipewire.h>

/**
 * RCA服务状态枚举
 */
typedef enum {
    RCA_STATE_DISABLED,    // 禁用状态
    RCA_STATE_ENABLED,     // 启用状态
    RCA_STATE_ACTIVE,      // 活动状态（有音频输入）
    RCA_STATE_ERROR        // 错误状态
} RcaState;

/**
 * RCA音频格式结构
 */
typedef struct {
    snd_pcm_format_t format;  // 音频格式
    uint32_t channels;        // 声道数
    uint32_t rate;            // 采样率
} RcaAudioFormat;

/**
 * RCA配置结构
 */
typedef struct {
    char device_name[64];     // 设备名称
    char alsa_device[64];     // ALSA设备名称
    uint32_t sample_rate;     // 采样率
    uint32_t channels;        // 声道数
    uint32_t bit_depth;       // 位深度
    float volume;             // 音量 (0.0-1.0)
    uint16_t port;            // 端口号
    uint32_t buffer_size;     // 缓冲区大小
    uint32_t period_size;     // 周期大小
} RcaConfig;

/**
 * RCA会话信息结构
 */
typedef struct {
    RcaState state;                   // 当前状态
    RcaAudioFormat format;            // 音频格式
    time_t active_time;               // 活动开始时间
    uint64_t total_frames;            // 总帧数
    float volume;                     // 当前音量
    bool muted;                       // 静音状态
    snd_pcm_t* pcm_handle;            // ALSA PCM句柄
} RcaSession;

/**
 * RCA服务结构
 */
typedef struct {
    struct pw_context* context;       // PipeWire上下文
    RcaState state;                   // 服务状态
    RcaConfig config;                 // 配置
    RcaSession session;               // 会话信息
    struct pw_stream* stream;         // PipeWire流
    pthread_t thread;                 // 工作线程
    pthread_mutex_t mutex;            // 互斥锁
    bool running;                     // 运行标志
    char error_msg[256];              // 错误信息
} RcaService;

/**
 * 创建RCA服务
 * @param context PipeWire上下文
 * @param config RCA配置
 * @return 新创建的RCA服务实例，失败则返回NULL
 */
RcaService* rca_create(struct pw_context* context, const RcaConfig* config);

/**
 * 销毁RCA服务
 * @param service RCA服务实例
 */
void rca_destroy(RcaService* service);

/**
 * 启动RCA服务
 * @param service RCA服务实例
 * @return 成功返回0，失败返回负数
 */
int rca_start(RcaService* service);

/**
 * 停止RCA服务
 * @param service RCA服务实例
 */
void rca_stop(RcaService* service);

/**
 * 设置RCA音量
 * @param service RCA服务实例
 * @param volume 音量值 (0.0-1.0)
 * @return 成功返回0，失败返回负数
 */
int rca_set_volume(RcaService* service, float volume);

/**
 * 设置RCA静音状态
 * @param service RCA服务实例
 * @param muted 静音标志
 * @return 成功返回0，失败返回负数
 */
int rca_set_mute(RcaService* service, bool muted);

/**
 * 获取当前RCA状态
 * @param service RCA服务实例
 * @return 当前状态
 */
RcaState rca_get_state(RcaService* service);

/**
 * 获取RCA会话信息
 * @param service RCA服务实例
 * @return 会话信息指针，失败返回NULL
 */
const RcaSession* rca_get_session(RcaService* service);

/**
 * 获取错误信息
 * @param service RCA服务实例
 * @return 错误信息字符串
 */
const char* rca_get_error(RcaService* service);

#endif // __RCA_H__