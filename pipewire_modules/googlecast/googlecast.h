#ifndef GOOGLE_CAST_H
#define GOOGLE_CAST_H

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <pthread.h>
#include <jansson.h>
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/thread-watch.h>
#include <openssl/ssl.h>
#include <libsrtp/srtp.h>

// Google Cast状态
typedef enum {
    GOOGLE_CAST_STATE_DISCONNECTED,  // 未连接
    GOOGLE_CAST_STATE_DISCOVERING,   // 发现中
    GOOGLE_CAST_STATE_CONNECTING,    // 连接中
    GOOGLE_CAST_STATE_CONNECTED,     // 已连接
    GOOGLE_CAST_STATE_PLAYING,       // 播放中
    GOOGLE_CAST_STATE_PAUSED,        // 已暂停
    GOOGLE_CAST_STATE_BUFFERING,     // 缓冲中
    GOOGLE_CAST_STATE_ERROR          // 错误状态
} GoogleCastState;

// Google Cast配置参数
typedef struct {
    char device_name[128];           // 设备名称
    char friendly_name[128];         // 友好名称
    char uuid[64];                   // 设备UUID
    int port;                        // 监听端口
    char manufacturer[64];           // 制造商名称
    char model_name[64];             // 型号名称
    char firmware_version[32];       // 固件版本
    bool enable_encryption;          // 是否启用加密
    char server_cert[1024];          // 服务器证书
    char private_key[1024];          // 私钥
    float initial_volume;            // 初始音量 (0.0-1.0)
} GoogleCastConfig;

// Google Cast媒体信息
typedef struct {
    char title[256];                 // 标题
    char artist[256];                // 艺术家
    char album[256];                 // 专辑
    char album_art_url[512];         // 专辑封面URL
    char content_id[512];            // 内容ID
    char stream_type[32];            // 流类型
    char mime_type[64];              // MIME类型
    uint64_t duration_ms;            // 时长(毫秒)
    uint64_t size_bytes;             // 大小(字节)
    struct spa_audio_info format;    // 音频格式
} GoogleCastMedia;

// Google Cast会话信息
typedef struct {
    GoogleCastMedia media;           // 当前媒体
    uint64_t position_ms;            // 当前位置(毫秒)
    float volume;                    // 当前音量(0.0-1.0)
    bool muted;                      // 是否静音
    char session_id[64];             // 会话ID
    char client_ip[48];              // 客户端IP
    char transport_id[64];           // 传输ID
    int client_port;                 // 客户端端口
} GoogleCastSession;

// Google Cast服务结构体
typedef struct {
    GoogleCastConfig config;         // 配置
    GoogleCastState state;           // 状态
    GoogleCastSession session;       // 会话信息
    struct pw_context* context;      // PipeWire上下文
    struct pw_stream* stream;        // 音频流
    pthread_t thread;                // 工作线程
    pthread_mutex_t mutex;           // 互斥锁
    bool running;                    // 是否运行中
    int server_fd;                   // 服务器套接字
    int client_fd;                   // 客户端套接字
    SSL_CTX* ssl_ctx;                // SSL上下文
    SSL* ssl;                        // SSL连接
    AvahiThreadedPoll* avahi_poll;   // Avahi线程池
    AvahiClient* avahi_client;       // Avahi客户端
    AvahiEntryGroup* avahi_group;    // Avahi服务组
    srtp_policy_t srtp_send_policy;  // SRTP发送策略
    srtp_policy_t srtp_recv_policy;  // SRTP接收策略
    bool srtp_initialized;           // SRTP是否初始化
    json_t* app_config;              // 应用配置
    char app_id[64];                 // 应用ID
} GoogleCastService;

// 创建Google Cast服务
GoogleCastService* google_cast_create(struct pw_context* context, const GoogleCastConfig* config);

// 销毁Google Cast服务
void google_cast_destroy(GoogleCastService* service);

// 启动Google Cast服务
int google_cast_start(GoogleCastService* service);

// 停止Google Cast服务
void google_cast_stop(GoogleCastService* service);

// 加载媒体
int google_cast_load_media(GoogleCastService* service, const char* url, const char* mime_type);

// 播放媒体
int google_cast_play(GoogleCastService* service);

// 暂停播放
int google_cast_pause(GoogleCastService* service);

// 停止播放
int google_cast_stop_media(GoogleCastService* service);

// 调整播放位置
int google_cast_seek(GoogleCastService* service, uint64_t position_ms);

// 设置音量
int google_cast_set_volume(GoogleCastService* service, float volume);

// 设置静音
int google_cast_set_mute(GoogleCastService* service, bool muted);

// 获取当前状态
GoogleCastState google_cast_get_state(GoogleCastService* service);

// 获取当前会话信息
const GoogleCastSession* google_cast_get_session(GoogleCastService* service);

#endif // GOOGLE_CAST_H