#ifndef SPOTIFY_CONNECT_H
#define SPOTIFY_CONNECT_H

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <pthread.h>
#include <libspotify/api.h>
#include <string.h>
#include <stdbool.h>

// Spotify Connect状态
typedef enum {
    SPOTIFY_STATE_DISCONNECTED,  // 未连接
    SPOTIFY_STATE_CONNECTING,    // 连接中
    SPOTIFY_STATE_CONNECTED,     // 已连接
    SPOTIFY_STATE_PLAYING,       // 播放中
    SPOTIFY_STATE_PAUSED,        // 已暂停
    SPOTIFY_STATE_LOADING,       // 加载中
    SPOTIFY_STATE_ERROR          // 错误状态
} SpotifyState;

// Spotify配置参数
typedef struct {
    char device_name[128];        // 设备名称
    char client_id[64];           // 客户端ID
    char client_secret[64];       // 客户端密钥
    char cache_path[256];         // 缓存路径
    char settings_path[256];      // 设置路径
    float initial_volume;         // 初始音量 (0.0-1.0)
    bool enable_bitrate_control;  // 是否启用比特率控制
    int preferred_bitrate;        // 首选比特率 (kbps)
    bool enable_audio_normalization; // 是否启用音频归一化
} SpotifyConfig;

// Spotify曲目信息
typedef struct {
    char uri[256];                // 曲目URI
    char title[256];              // 标题
    char artist[256];             // 艺术家
    char album[256];              // 专辑
    char album_art_url[512];      // 专辑封面URL
    int duration_ms;              // 时长(毫秒)
    int track_number;             // 曲目编号
    int disc_number;              // 碟片编号
    bool is_playable;             // 是否可播放
} SpotifyTrack;

// Spotify会话信息
typedef struct {
    SpotifyTrack current_track;   // 当前曲目
    uint64_t position_ms;         // 当前位置(毫秒)
    float volume;                 // 当前音量(0.0-1.0)
    bool shuffle;                 // 是否随机播放
    bool repeat;                  // 是否重复播放
    int play_queue_length;        // 播放队列长度
    char username[128];           // 当前用户名
    struct spa_audio_info format; // 音频格式
} SpotifySession;

// Spotify Connect服务结构体
typedef struct {
    SpotifyConfig config;         // 配置
    SpotifyState state;           // 状态
    SpotifySession session;       // 会话信息
    struct pw_context* context;   // PipeWire上下文
    struct pw_stream* stream;     // 音频流
    pthread_t thread;             // 工作线程
    pthread_mutex_t mutex;        // 互斥锁
    bool running;                 // 是否运行中
    sp_session* spotify_session;  // libspotify会话
    sp_audio_driver* audio_driver;// 音频驱动
    uint32_t next_timeout;        // 下一个超时时间
    char access_token[256];       // 访问令牌
    char refresh_token[256];      // 刷新令牌
    time_t token_expires;         // 令牌过期时间
} SpotifyConnectService;

// 创建Spotify Connect服务
SpotifyConnectService* spotify_connect_create(struct pw_context* context, const SpotifyConfig* config);

// 销毁Spotify Connect服务
void spotify_connect_destroy(SpotifyConnectService* service);

// 启动Spotify Connect服务
int spotify_connect_start(SpotifyConnectService* service);

// 停止Spotify Connect服务
void spotify_connect_stop(SpotifyConnectService* service);

// 播放Spotify曲目
int spotify_connect_play(SpotifyConnectService* service, const char* uri);

// 暂停播放
int spotify_connect_pause(SpotifyConnectService* service);

// 恢复播放
int spotify_connect_resume(SpotifyConnectService* service);

// 跳过到下一曲目
int spotify_connect_next(SpotifyConnectService* service);

// 返回到上一曲目
int spotify_connect_prev(SpotifyConnectService* service);

// 设置音量
int spotify_connect_set_volume(SpotifyConnectService* service, float volume);

// 设置播放位置
int spotify_connect_seek(SpotifyConnectService* service, uint64_t position_ms);

// 设置随机播放
int spotify_connect_set_shuffle(SpotifyConnectService* service, bool shuffle);

// 设置重复播放
int spotify_connect_set_repeat(SpotifyConnectService* service, bool repeat);

// 获取当前状态
SpotifyState spotify_connect_get_state(SpotifyConnectService* service);

// 获取当前会话信息
const SpotifySession* spotify_connect_get_session(SpotifyConnectService* service);

#endif // SPOTIFY_CONNECT_H