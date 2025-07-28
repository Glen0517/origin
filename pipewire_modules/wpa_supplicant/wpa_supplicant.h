#ifndef WPA_SUPPLICANT_H
#define WPA_SUPPLICANT_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <pipewire/pipewire.h>
#include "../../include/dbus_utils.h"

// WPA Supplicant状态枚举
typedef enum {
    WPA_STATE_DISCONNECTED,
    WPA_STATE_SCANNING,
    WPA_STATE_AUTHENTICATING,
    WPA_STATE_ASSOCIATING,
    WPA_STATE_ASSOCIATED,
    WPA_STATE_CONNECTED,
    WPA_STATE_ERROR
} WpaState;

// 加密类型枚举
typedef enum {
    WPA_SECURITY_NONE,
    WPA_SECURITY_WEP,
    WPA_SECURITY_WPA,
    WPA_SECURITY_WPA2,
    WPA_SECURITY_WPA3
} WpaSecurity;

// WPA配置结构体
typedef struct {
    char interface[32];          // 网络接口名称
    char config_path[256];       // 配置文件路径
    int scan_interval;           // 扫描间隔(秒)
    bool auto_connect;           // 是否自动连接
    int max_retries;             // 最大重试次数
} WpaConfig;

// WPA网络信息
typedef struct {
    char ssid[32];               // 网络名称
    WpaSecurity security;        // 安全类型
    int signal_strength;         // 信号强度(0-100)
    uint32_t frequency;          // 频率(MHz)
    char bssid[18];              // BSSID
} WpaNetwork;

// WPA会话信息
typedef struct {
    WpaNetwork current_network;  // 当前连接的网络
    uint64_t connection_time;    // 连接时间戳
    char ip_address[46];         // IP地址
    char gateway[46];            // 网关地址
    char dns_servers[256];       // DNS服务器
} WpaSession;

// WPA Supplicant服务结构体
typedef struct {
    struct pw_context *context;  // PipeWire上下文
    WpaConfig config;            // 配置信息
    WpaState state;              // 当前状态
    WpaSession session;          // 会话信息
    bool running;                // 运行标志
    pthread_t thread;            // 工作线程
    pthread_mutex_t mutex;       // 互斥锁
    int ctrl_fd;                 // 控制接口文件描述符
    char error_msg[256];         // 错误信息
    // wpa_supplicant相关数据结构
    void *wpa_handle;            // wpa_supplicant句柄
} WpaSupplicantService;

// API函数声明
WpaSupplicantService* wpa_supplicant_create(struct pw_context *context, const WpaConfig *config);
void wpa_supplicant_destroy(WpaSupplicantService *service);
int wpa_supplicant_start(WpaSupplicantService *service);
void wpa_supplicant_stop(WpaSupplicantService *service);
WpaState wpa_supplicant_get_state(WpaSupplicantService *service);
int wpa_supplicant_connect(WpaSupplicantService *service, const char *ssid, const char *password, WpaSecurity security);
int wpa_supplicant_disconnect(WpaSupplicantService *service);
int wpa_supplicant_scan_networks(WpaSupplicantService *service);
const WpaSession* wpa_supplicant_get_session(WpaSupplicantService *service);
const char* wpa_supplicant_get_error(WpaSupplicantService *service);

#endif // WPA_SUPPLICANT_H