#ifndef CONNMAN_H
#define CONNMAN_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <pipewire/pipewire.h>
#include "../../include/dbus_utils.h"

// ConnMan状态枚举
typedef enum {
    CONNMAN_STATE_DISCONNECTED,
    CONNMAN_STATE_CONNECTING,
    CONNMAN_STATE_CONNECTED,
    CONNMAN_STATE_ERROR
} ConnManState;

// ConnMan配置结构体
typedef struct {
    char device_name[128];
    char friendly_name[128];
    bool enable_discovery;
    uint16_t port;
} ConnManConfig;

// ConnMan会话信息
typedef struct {
    char interface[32];
    char ip_address[46];  // 支持IPv6
    char mac_address[18];
    char ssid[32];
    int signal_strength;  // 0-100%
    uint64_t connection_time;  // 连接时长(秒)
} ConnManSession;

// ConnMan服务结构体
typedef struct {
    struct pw_context *context;
    ConnManConfig config;
    ConnManState state;
    ConnManSession session;
    bool running;
    pthread_t thread;
    pthread_mutex_t mutex;
    int server_fd;
    int client_fd;
    char error_msg[256];
} ConnManService;

// API函数声明
ConnManService* connman_create(struct pw_context *context, const ConnManConfig *config);
void connman_destroy(ConnManService *service);
int connman_start(ConnManService *service);
void connman_stop(ConnManService *service);
ConnManState connman_get_state(ConnManService *service);
const ConnManSession* connman_get_session(ConnManService *service);
int connman_connect(ConnManService *service, const char *ssid, const char *password);
int connman_disconnect(ConnManService *service);
const char* connman_get_error(ConnManService *service);

#endif // CONNMAN_H