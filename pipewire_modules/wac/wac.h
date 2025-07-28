#ifndef __WAC_H__
#define __WAC_H__

#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <pipewire/pipewire.h>

// WAC服务状态枚举
typedef enum {
    WAC_STATE_DISABLED,
    WAC_STATE_ENABLED,
    WAC_STATE_PAIRING,
    WAC_STATE_PAIRED,
    WAC_STATE_ERROR
} WacState;

// 配件类型枚举
typedef enum {
    WAC_ACCESSORY_TYPE_SPEAKER,
    WAC_ACCESSORY_TYPE_HEADPHONES,
    WAC_ACCESSORY_TYPE_MICROPHONE,
    WAC_ACCESSORY_TYPE_REMOTE,
    WAC_ACCESSORY_TYPE_OTHER
} WacAccessoryType;

// 安全级别枚举
typedef enum {
    WAC_SECURITY_LEVEL_NONE,
    WAC_SECURITY_LEVEL_ENCRYPTED,
    WAC_SECURITY_LEVEL_AUTHENTICATED
} WacSecurityLevel;

// WAC配置结构
typedef struct {
    char device_name[64];          // 设备名称
    uint16_t port;                 // 监听端口
    WacSecurityLevel security;     // 安全级别
    uint32_t pairing_timeout;      // 配对超时(秒)
    uint8_t max_accessories;       // 最大配件数量
    bool auto_accept;              // 是否自动接受配对
} WacConfig;

// 配件信息结构
typedef struct {
    char identifier[32];           // 配件唯一标识符
    char name[64];                 // 配件名称
    WacAccessoryType type;         // 配件类型
    char ip_address[46];           // IP地址
    uint16_t port;                 // 端口
    time_t paired_time;            // 配对时间
    bool connected;                // 连接状态
    uint32_t last_seen;            // 最后活动时间戳
} WacAccessory;

// WAC会话信息
typedef struct {
    uint8_t accessory_count;       // 当前配对的配件数量
    WacAccessory accessories[10];  // 配件列表
    time_t uptime;                 // 服务运行时间
    uint32_t pairing_attempts;     // 配对尝试次数
    uint32_t successful_pairings;  // 成功配对次数
} WacSession;

// WAC服务结构
typedef struct {
    struct pw_context* context;    // PipeWire上下文
    WacState state;                // 当前状态
    WacConfig config;              // 配置
    WacSession session;            // 会话信息
    pthread_t thread;              // 工作线程
    pthread_mutex_t mutex;         // 互斥锁
    bool running;                  // 运行标志
    int server_fd;                 // 服务器套接字
    char error_msg[256];           // 错误信息
} WacService;

// API函数声明
// 创建WAC服务
WacService* wac_create(struct pw_context* context, const WacConfig* config);

// 销毁WAC服务
void wac_destroy(WacService* service);

// 启动WAC服务
int wac_start(WacService* service);

// 停止WAC服务
void wac_stop(WacService* service);

// 开始配对过程
int wac_start_pairing(WacService* service);

// 结束配对过程
void wac_stop_pairing(WacService* service);

// 配对指定配件
int wac_pair_accessory(WacService* service, const char* identifier, const char* ip_address, uint16_t port);

// 解除与配件的配对
int wac_unpair_accessory(WacService* service, const char* identifier);

// 获取当前状态
WacState wac_get_state(WacService* service);

// 获取会话信息
const WacSession* wac_get_session(WacService* service);

// 获取指定配件信息
const WacAccessory* wac_get_accessory(WacService* service, const char* identifier);

// 获取错误信息
const char* wac_get_error(WacService* service);

#endif // __WAC_H__