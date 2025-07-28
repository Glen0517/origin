#ifndef TIDALCONNECT_H
#define TIDALCONNECT_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

// 连接状态枚举
typedef enum {
    TIDAL_CONNECT_STATE_DISCONNECTED,
    TIDAL_CONNECT_STATE_DISCOVERING,
    TIDAL_CONNECT_STATE_CONNECTING,
    TIDAL_CONNECT_STATE_CONNECTED,
    TIDAL_CONNECT_STATE_AUTHENTICATING,
    TIDAL_CONNECT_STATE_AUTHENTICATED,
    TIDAL_CONNECT_STATE_PLAYING,
    TIDAL_CONNECT_STATE_PAUSED,
    TIDAL_CONNECT_STATE_BUFFERING,
    TIDAL_CONNECT_STATE_ERROR
} TidalConnectState;

// 音频质量选项
typedef enum {
    TIDAL_QUALITY_LOW,
    TIDAL_QUALITY_NORMAL,
    TIDAL_QUALITY_HIGH,
    TIDAL_QUALITY_LOSSLESS,
    TIDAL_QUALITY_HI_RES
} TidalAudioQuality;

// 配置结构
typedef struct {
    char device_name[128];          // 设备名称
    char friendly_name[128];        // 显示名称
    char device_id[64];             // 设备唯一ID
    char app_key[64];               // Tidal应用密钥
    char app_secret[64];            // Tidal应用密钥
    uint16_t port;                  // 监听端口
    TidalAudioQuality audio_quality;// 音频质量
    bool enable_encryption;         // 是否启用加密
    float initial_volume;           // 初始音量 (0.0-1.0)
    char cache_dir[256];            // 缓存目录
    bool enable_discovery;          // 是否启用设备发现
} TidalConnectConfig;

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
} TidalTrack;

// 会话信息
typedef struct {
    char session_id[64];            // 会话ID
    char user_id[64];               // 用户ID
    char access_token[256];         // 访问令牌
    uint64_t token_expiry;          // 令牌过期时间戳
    TidalTrack current_track;       // 当前曲目
    uint64_t position_ms;           // 当前播放位置(毫秒)
    float volume;                   // 当前音量(0.0-1.0)
    bool muted;                     // 是否静音
    bool shuffle;                   // 是否随机播放
    int repeat_mode;                // 重复模式(0:关闭,1:单曲,2:全部)
    char client_ip[48];             // 客户端IP
    uint16_t client_port;           // 客户端端口
    spa_audio_info format;          // 音频格式信息
} TidalConnectSession;

// Tidal Connect服务结构
typedef struct {
    struct pw_context* context;     // PipeWire上下文
    struct pw_stream* stream;       // PipeWire流
    TidalConnectConfig config;      // 配置
    TidalConnectSession session;    // 会话信息
    TidalConnectState state;        // 当前状态
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
} TidalConnectService;

// API函数声明
TidalConnectService* tidal_connect_create(struct pw_context* context, const TidalConnectConfig* config);
void tidal_connect_destroy(TidalConnectService* service);
int tidal_connect_start(TidalConnectService* service);
void tidal_connect_stop(TidalConnectService* service);
int tidal_connect_play(TidalConnectService* service);
int tidal_connect_pause(TidalConnectService* service);
int tidal_connect_stop_playback(TidalConnectService* service);
int tidal_connect_next_track(TidalConnectService* service);
int tidal_connect_previous_track(TidalConnectService* service);
int tidal_connect_seek(TidalConnectService* service, uint64_t position_ms);
int tidal_connect_set_volume(TidalConnectService* service, float volume);
int tidal_connect_set_mute(TidalConnectService* service, bool muted);
int tidal_connect_set_quality(TidalConnectService* service, TidalAudioQuality quality);
int tidal_connect_set_shuffle(TidalConnectService* service, bool shuffle);
int tidal_connect_set_repeat(TidalConnectService* service, int repeat_mode);
TidalConnectState tidal_connect_get_state(TidalConnectService* service);
const TidalConnectSession* tidal_connect_get_session(TidalConnectService* service);
const char* tidal_connect_get_error(TidalConnectService* service);

#endif // TIDALCONNECT_H