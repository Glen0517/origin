#ifndef SOFTAP_H
#define SOFTAP_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <pipewire/pipewire.h>
#include "../../include/dbus_utils.h"

// SoftAP状态枚举
typedef enum {
    SOFTAP_STATE_DISABLED,
    SOFTAP_STATE_ENABLING,
    SOFTAP_STATE_ENABLED,
    SOFTAP_STATE_ERROR
} SoftapState;

// 加密类型枚举
typedef enum {
    SOFTAP_SECURITY_NONE,
    SOFTAP_SECURITY_WPA2,
    SOFTAP_SECURITY_WPA3
} SoftapSecurity;

// SoftAP配置结构体
typedef struct {
    char ssid[32];               // 网络名称
    char password[64];           // 密码
    SoftapSecurity security;     // 安全类型
    uint16_t channel;            // 信道(1-14)
    uint8_t max_clients;         // 最大客户端数量
    char interface[32];          // 网络接口
    uint16_t port;               // 服务端口
} SoftapConfig;

// 客户端信息结构体
typedef struct {
    char mac_address[18];        // MAC地址
    char ip_address[46];         // IP地址
    time_t connect_time;         // 连接时间
} SoftapClient;

// SoftAP会话信息
typedef struct {
    SoftapClient clients[10];    // 连接的客户端
    uint8_t client_count;        // 客户端数量
    uint64_t uptime;             // 运行时间(秒)
    uint32_t tx_bytes;           // 发送字节数
    uint32_t rx_bytes;           // 接收字节数
} SoftapSession;

// SoftAP服务结构体
typedef struct {
    struct pw_context *context;  // PipeWire上下文
    SoftapConfig config;         // 配置信息
    SoftapState state;           // 当前状态
    SoftapSession session;       // 会话信息
    bool running;                // 运行标志
    pthread_t thread;            // 工作线程
    pthread_mutex_t mutex;       // 互斥锁
    int server_fd;               // 服务器套接字
    char error_msg[256];         // 错误信息
} SoftapService;

// API函数声明
SoftapService* softap_create(struct pw_context *context, const SoftapConfig *config);
void softap_destroy(SoftapService *service);
int softap_start(SoftapService *service);
void softap_stop(SoftapService *service);
SoftapState softap_get_state(SoftapService *service);
int softap_add_client(SoftapService *service, const char *mac_addr, const char *ip_addr);
int softap_remove_client(SoftapService *service, const char *mac_addr);
const SoftapSession* softap_get_session(SoftapService *service);
const char* softap_get_error(SoftapService *service);

#endif // SOFTAP_H