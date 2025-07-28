#ifndef AIRPLAY2_H
#define AIRPLAY2_H

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <pthread.h>
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/thread-watch.h>

// AirPlay 2设备状态
typedef enum {
    AIRPLAY2_STATE_DISCONNECTED,  // 未连接
    AIRPLAY2_STATE_CONNECTING,    // 连接中
    AIRPLAY2_STATE_CONNECTED,     // 已连接
    AIRPLAY2_STATE_STREAMING,     // 流媒体播放中
    AIRPLAY2_STATE_ERROR          // 错误状态
} AirPlay2State;

// AirPlay 2配置参数
typedef struct {
    char device_name[128];        // 设备名称
    char device_id[64];           // 设备唯一ID
    char password[64];            // 访问密码 (可选)
    int port;                     // 监听端口
    bool enable_encryption;       // 是否启用加密
    bool require_password;        // 是否需要密码
    float volume;                 // 初始音量 (0.0-1.0)
} AirPlay2Config;

// AirPlay 2会话信息
typedef struct {
    char session_id[64];          // 会话ID
    char client_name[128];        // 客户端名称
    char client_ip[48];           // 客户端IP地址
    struct spa_audio_info format; // 音频格式
    uint64_t timestamp;           // 时间戳
    uint32_t sequence;            // 序列号
} AirPlay2Session;

// AirPlay 2服务结构体
typedef struct {
    AirPlay2Config config;        // 配置
    AirPlay2State state;          // 状态
    AirPlay2Session session;      // 当前会话
    struct pw_context* context;   // PipeWire上下文
    struct pw_core* core;         // PipeWire核心
    struct pw_stream* stream;     // 音频流
    pthread_t thread;             // 工作线程
    pthread_mutex_t mutex;        // 互斥锁
    bool running;                 // 是否运行中
    AvahiThreadedPoll* avahi_poll;// Avahi线程池
    AvahiClient* avahi_client;    // Avahi客户端
    AvahiEntryGroup* avahi_group; // Avahi服务组
    int server_fd;                // 服务器套接字
    int client_fd;                // 客户端套接字
} AirPlay2Service;

// 创建AirPlay 2服务
AirPlay2Service* airplay2_create(struct pw_context* context, const AirPlay2Config* config);

// 销毁AirPlay 2服务
void airplay2_destroy(AirPlay2Service* service);

// 启动AirPlay 2服务
int airplay2_start(AirPlay2Service* service);

// 停止AirPlay 2服务
void airplay2_stop(AirPlay2Service* service);

// 设置AirPlay 2音量
int airplay2_set_volume(AirPlay2Service* service, float volume);

// 获取AirPlay 2状态
AirPlay2State airplay2_get_state(AirPlay2Service* service);

// 获取当前会话信息
const AirPlay2Session* airplay2_get_session(AirPlay2Service* service);

#endif // AIRPLAY2_H