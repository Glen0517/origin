#ifndef __AUXIN_H__
#define __AUXIN_H__

#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <pipewire/pipewire.h>
#include <alsa/asoundlib.h>

// AUX-in服务状态枚举
typedef enum {
    AUXIN_STATE_DISABLED,
    AUXIN_STATE_ENABLED,
    AUXIN_STATE_ACTIVE,
    AUXIN_STATE_ERROR
} AuxinState;

// AUX-in配置结构
typedef struct {
    char device_name[64];          // 设备名称
    char alsa_device[32];          // ALSA设备名称
    uint32_t sample_rate;          // 采样率
    uint8_t channels;              // 声道数
    uint8_t bit_depth;             // 位深度
    float volume;                  // 音量(0.0-1.0)
    bool auto_gain;                // 自动增益控制
    uint16_t port;                 // 监听端口
} AuxinConfig;

// AUX-in会话信息
typedef struct {
    AuxinState state;              // 当前状态
    snd_pcm_t* pcm_handle;         // ALSA PCM句柄
    struct spa_audio_info format;  // 音频格式
    time_t active_time;            // 活动时间
    uint64_t total_frames;         // 总帧数
    float current_gain;            // 当前增益
    bool muted;                    // 静音状态
} AuxinSession;

// AUX-in服务结构
typedef struct {
    struct pw_context* context;    // PipeWire上下文
    struct pw_stream* stream;      // PipeWire流
    AuxinConfig config;            // 配置
    AuxinSession session;          // 会话信息
    pthread_t thread;              // 工作线程
    pthread_mutex_t mutex;         // 互斥锁
    bool running;                  // 运行标志
    char error_msg[256];           // 错误信息
} AuxinService;

// API函数声明
// 创建AUX-in服务
AuxinService* auxin_create(struct pw_context* context, const AuxinConfig* config);

// 销毁AUX-in服务
void auxin_destroy(AuxinService* service);

// 启动AUX-in服务
int auxin_start(AuxinService* service);

// 停止AUX-in服务
void auxin_stop(AuxinService* service);

// 设置音量
int auxin_set_volume(AuxinService* service, float volume);

// 设置静音
int auxin_set_mute(AuxinService* service, bool muted);

// 获取当前状态
AuxinState auxin_get_state(AuxinService* service);

// 获取会话信息
const AuxinSession* auxin_get_session(AuxinService* service);

// 获取错误信息
const char* auxin_get_error(AuxinService* service);

#endif // __AUXIN_H__