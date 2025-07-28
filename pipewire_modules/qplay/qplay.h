#ifndef QPLAY_H
#define QPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

// QPlay连接状态枚举
typedef enum {
    QPLAY_STATE_DISCONNECTED,
    QPLAY_STATE_DISCOVERING,
    QPLAY_STATE_CONNECTING,
    QPLAY_STATE_CONNECTED,
    QPLAY_STATE_AUTHENTICATING,
    QPLAY_STATE_AUTHENTICATED,
    QPLAY_STATE_PLAYING,
    QPLAY_STATE_PAUSED,
    QPLAY_STATE_BUFFERING,
    QPLAY_STATE_ERROR
} QPlayState;

// 音频质量选项
typedef enum {
    QPLAY_QUALITY_STANDARD,
    QPLAY_QUALITY_HIGH,
    QPLAY_QUALITY_LOSSLESS
} QPlayAudioQuality;

// QPlay配置结构
typedef struct {
    char device_name[128];          // 设备名称
    char friendly_name[128];        // 显示名称
    char device_id[64];             // 设备唯一ID
    char app_id[64];                // QPlay应用ID
    char app_key[64];               // QPlay应用密钥
    uint16_t port;                  // 监听端口
    QPlayAudioQuality audio_quality;// 音频质量
    bool enable_encryption;         // 是否启用加密
    float initial_volume;           // 初始音量 (0.0-1.0)
    char cache_dir[256];            // 缓存目录
    bool enable_discovery;          // 是否启用设备发现
} QPlayConfig;

// 当前播放的曲目信息
typedef struct {
    char track_id[64];              // 曲目ID
    char title[256];                // 标题
    char artist[256];               // 艺术家
    char album[256];                // 专辑
    char album_art_url[512];        // 专辑封面URL
    uint64_t duration_ms;           // 时长(毫秒)
    uint32_t sample_rate;           // 采样率
    uint8_t channels;               // 声道数
    uint8_t bit_depth;              // 位深度
    char audio_format[32];          // 音频格式
} QPlayTrack;

// QPlay会话信息
typedef struct {
    char session_id[64];            // 会话ID
    char user_id[64];               // 用户ID
    char access_token[256];         // 访问令牌
    uint64_t token_expiry;          // 令牌过期时间戳
    QPlayTrack current_track;       // 当前曲目
    uint64_t position_ms;           // 当前播放位置(毫秒)
    float volume;                   // 当前音量(0.0-1.0)
    bool muted;                     // 是否静音
    bool shuffle;                   // 是否随机播放
    int repeat_mode;                // 重复模式(0:关闭,1:单曲,2:全部)
    char client_ip[48];             // 客户端IP
    uint16_t client_port;           // 客户端端口
    spa_audio_info format;          // 音频格式信息
} QPlaySession;

// QPlay服务结构
typedef struct {
    struct pw_context* context;     // PipeWire上下文
    struct pw_stream* stream;       // PipeWire流
    QPlayConfig config;             // 配置
    QPlaySession session;           // 会话信息
    QPlayState state;               // 当前状态
    pthread_t thread;               // 工作线程
    pthread_mutex_t mutex;          // 互斥锁
    bool running;                   // 运行标志
    int server_fd;                  // 服务器套接字
    int client_fd;                  // 客户端套接字
    void* avahi_client;             // Avahi客户端
    void* avahi_group;              // Avahi服务组
    void* avahi_poll;               // Avahi轮询
    char error_msg[256];            // 错误消息
    // 其他私有成员...
} QPlayService;

// API函数声明
QPlayService* qplay_create(struct pw_context* context, const QPlayConfig* config);
void qplay_destroy(QPlayService* service);
int qplay_start(QPlayService* service);
void qplay_stop(QPlayService* service);
int qplay_play(QPlayService* service);
int qplay_pause(QPlayService* service);
int qplay_stop_playback(QPlayService* service);
int qplay_next_track(QPlayService* service);
int qplay_previous_track(QPlayService* service);
int qplay_seek(QPlayService* service, uint64_t position_ms);
int qplay_set_volume(QPlayService* service, float volume);
int qplay_set_mute(QPlayService* service, bool muted);
int qplay_set_quality(QPlayService* service, QPlayAudioQuality quality);
int qplay_set_shuffle(QPlayService* service, bool shuffle);
int qplay_set_repeat(QPlayService* service, int repeat_mode);
QPlayState qplay_get_state(QPlayService* service);
const QPlaySession* qplay_get_session(QPlayService* service);
const char* qplay_get_error(QPlayService* service);

#endif // QPLAY_H