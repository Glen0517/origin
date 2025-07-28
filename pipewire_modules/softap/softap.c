#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <jansson.h>
#include <time.h>

#include "softap.h"
#include "../../include/dbus_utils.h"

// 前向声明
static void* softap_worker_thread(void* data);
static int softap_setup_server(SoftapService* service);
static void softap_cleanup_server(SoftapService* service);
static void softap_set_state(SoftapService* service, SoftapState state);
static void softap_update_uptime(SoftapService* service);

// 创建SoftAP服务
SoftapService* softap_create(struct pw_context* context, const SoftapConfig* config) {
    if (!context || !config) {
        fprintf(stderr, "Invalid parameters for softap_create\n");
        return NULL;
    }

    SoftapService* service = calloc(1, sizeof(SoftapService));
    if (!service) {
        fprintf(stderr, "Failed to allocate memory for SoftapService\n");
        return NULL;
    }

    // 初始化D-Bus
    if (!dbus_initialize()) {
        fprintf(stderr, "Failed to initialize D-Bus connection for SoftAP\n");
        // 继续初始化但记录错误
    }

    // 初始化互斥锁
    pthread_mutex_init(&service->mutex, NULL);

    // 设置上下文
    service->context = context;
    service->state = SOFTAP_STATE_DISABLED;
    service->running = false;
    service->server_fd = -1;
    service->session.client_count = 0;
    service->session.uptime = 0;
    service->session.tx_bytes = 0;
    service->session.rx_bytes = 0;

    // 复制配置
    service->config = *config;
    if (!service->config.interface[0]) {
        strncpy(service->config.interface, "wlan0", sizeof(service->config.interface)-1);
    }
    if (!service->config.ssid[0]) {
        snprintf(service->config.ssid, sizeof(service->config.ssid), "RealTimeSoftAP");
    }
    if (service->config.channel == 0) {
        service->config.channel = 6;
    }
    if (service->config.max_clients == 0) {
        service->config.max_clients = 10;
    }
    if (service->config.port == 0) {
        service->config.port = 10010;
    }

    return service;
}

// 销毁SoftAP服务
void softap_destroy(SoftapService* service) {
    if (!service) return;

    softap_stop(service);

    // 清理服务器
    softap_cleanup_server(service);

    // 清理互斥锁
    pthread_mutex_destroy(&service->mutex);

    // 清理D-Bus
    dbus_cleanup();

    free(service);
}

// 启动SoftAP服务
int softap_start(SoftapService* service) {
    if (!service || service->running) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 设置状态为启用中
    softap_set_state(service, SOFTAP_STATE_ENABLING);

    // 创建服务器套接字
    if (softap_setup_server(service) < 0) {
        fprintf(stderr, "Failed to setup SoftAP server\n");
        softap_set_state(service, SOFTAP_STATE_ERROR);
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 设置运行标志
    service->running = true;

    // 记录启动时间
    service->session.uptime = time(NULL);

    // 创建工作线程
    if (pthread_create(&service->thread, NULL, softap_worker_thread, service) != 0) {
        fprintf(stderr, "Failed to create SoftAP worker thread\n");
        service->running = false;
        softap_cleanup_server(service);
        softap_set_state(service, SOFTAP_STATE_ERROR);
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 设置状态为已启用
    softap_set_state(service, SOFTAP_STATE_ENABLED);

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 停止SoftAP服务
void softap_stop(SoftapService* service) {
    if (!service || !service->running) {
        return;
    }

    pthread_mutex_lock(&service->mutex);
    service->running = false;
    pthread_mutex_unlock(&service->mutex);

    // 等待线程结束
    pthread_join(service->thread, NULL);

    // 清理服务器
    softap_cleanup_server(service);

    // 重置会话信息
    service->session.client_count = 0;
    memset(service->session.clients, 0, sizeof(service->session.clients));
    service->session.uptime = 0;
    service->session.tx_bytes = 0;
    service->session.rx_bytes = 0;

    // 设置状态为已禁用
    softap_set_state(service, SOFTAP_STATE_DISABLED);
}

// 添加客户端
int softap_add_client(SoftapService* service, const char* mac_addr, const char* ip_addr) {
    if (!service || !mac_addr || !ip_addr || service->state != SOFTAP_STATE_ENABLED) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 检查是否已达到最大客户端数量
    if (service->session.client_count >= service->config.max_clients) {
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 检查客户端是否已存在
    for (int i = 0; i < service->session.client_count; i++) {
        if (strcmp(service->session.clients[i].mac_address, mac_addr) == 0) {
            // 更新IP地址和连接时间
            strncpy(service->session.clients[i].ip_address, ip_addr, sizeof(service->session.clients[i].ip_address)-1);
            service->session.clients[i].connect_time = time(NULL);
            pthread_mutex_unlock(&service->mutex);
            return 0;
        }
    }

    // 添加新客户端
    int index = service->session.client_count;
    strncpy(service->session.clients[index].mac_address, mac_addr, sizeof(service->session.clients[index].mac_address)-1);
    strncpy(service->session.clients[index].ip_address, ip_addr, sizeof(service->session.clients[index].ip_address)-1);
    service->session.clients[index].connect_time = time(NULL);
    service->session.client_count++;

    pthread_mutex_unlock(&service->mutex);

    // 发送客户端连接D-Bus信号
    json_t* details = json_object();
    json_object_set_new(details, "mac_address", json_string(mac_addr));
    json_object_set_new(details, "ip_address", json_string(ip_addr));
    json_object_set_new(details, "client_count", json_integer(service->session.client_count));
    json_object_set_new(details, "timestamp", json_integer(time(NULL)));

    const char* json_str = json_dumps(details, JSON_COMPACT);
    if (json_str) {
        dbus_emit_signal("com.realtimeaudio.SoftAP", DBUS_SIGNAL_TYPE_CLIENT_CONNECTED, json_str);
        free((void*)json_str);
    }
    json_decref(details);

    return 0;
}

// 移除客户端
int softap_remove_client(SoftapService* service, const char* mac_addr) {
    if (!service || !mac_addr || service->state != SOFTAP_STATE_ENABLED) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 查找客户端
    int found = -1;
    for (int i = 0; i < service->session.client_count; i++) {
        if (strcmp(service->session.clients[i].mac_address, mac_addr) == 0) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 保存要移除的客户端信息
    char ip_address[46];
    strncpy(ip_address, service->session.clients[found].ip_address, sizeof(ip_address)-1);

    // 移除客户端（将最后一个客户端移到当前位置）
    service->session.client_count--;
    if (found < service->session.client_count) {
        service->session.clients[found] = service->session.clients[service->session.client_count];
    }
    memset(&service->session.clients[service->session.client_count], 0, sizeof(SoftapClient));

    pthread_mutex_unlock(&service->mutex);

    // 发送客户端断开连接D-Bus信号
    json_t* details = json_object();
    json_object_set_new(details, "mac_address", json_string(mac_addr));
    json_object_set_new(details, "ip_address", json_string(ip_address));
    json_object_set_new(details, "client_count", json_integer(service->session.client_count));
    json_object_set_new(details, "timestamp", json_integer(time(NULL)));

    const char* json_str = json_dumps(details, JSON_COMPACT);
    if (json_str) {
        dbus_emit_signal("com.realtimeaudio.SoftAP", DBUS_SIGNAL_TYPE_CLIENT_DISCONNECTED, json_str);
        free((void*)json_str);
    }
    json_decref(details);

    return 0;
}

// 获取当前状态
SoftapState softap_get_state(SoftapService* service) {
    if (!service) {
        return SOFTAP_STATE_ERROR;
    }

    SoftapState state;
    pthread_mutex_lock(&service->mutex);
    state = service->state;
    pthread_mutex_unlock(&service->mutex);
    return state;
}

// 获取会话信息
const SoftapSession* softap_get_session(SoftapService* service) {
    if (!service || service->state != SOFTAP_STATE_ENABLED) {
        return NULL;
    }

    return &service->session;
}

// 获取错误信息
const char* softap_get_error(SoftapService* service) {
    if (!service) {
        return "Invalid service pointer";
    }

    return service->error_msg;
}

// 设置状态
static void softap_set_state(SoftapService* service, SoftapState state) {
    if (!service) return;

    pthread_mutex_lock(&service->mutex);
    SoftapState old_state = service->state;
    service->state = state;
    pthread_mutex_unlock(&service->mutex);

    // 只有状态实际改变时才发送信号
    if (old_state != state) {
        // 创建JSON payload
        json_t* details = json_object();
        json_object_set_new(details, "old_state", json_integer(old_state));
        json_object_set_new(details, "new_state", json_integer(state));
        json_object_set_new(details, "ssid", json_string(service->config.ssid));
        json_object_set_new(details, "client_count", json_integer(service->session.client_count));
        json_object_set_new(details, "timestamp", json_integer(time(NULL)));

        const char* json_str = json_dumps(details, JSON_COMPACT);
        if (json_str) {
            // 发送D-Bus信号
            dbus_emit_signal("com.realtimeaudio.SoftAP", DBUS_SIGNAL_TYPE_STATE_CHANGED, json_str);
            free((void*)json_str);
        }
        json_decref(details);

        // 日志记录
        printf("SoftAP state changed from %d to %d\n", old_state, state);
    }
}

// 更新运行时间
static void softap_update_uptime(SoftapService* service) {
    if (!service || service->state != SOFTAP_STATE_ENABLED) {
        return;
    }

    pthread_mutex_lock(&service->mutex);
    service->session.uptime = time(NULL) - service->session.uptime;
    pthread_mutex_unlock(&service->mutex);
}

// 工作线程函数
static void* softap_worker_thread(void* data) {
    SoftapService* service = (SoftapService*)data;
    fd_set read_fds;
    struct timeval timeout;
    int max_fd;

    while (1) {
        pthread_mutex_lock(&service->mutex);
        bool running = service->running;
        pthread_mutex_unlock(&service->mutex);

        if (!running) {
            break;
        }

        // 更新运行时间
        softap_update_uptime(service);

        // 设置文件描述符集
        FD_ZERO(&read_fds);
        max_fd = service->server_fd;

        if (service->server_fd >= 0) {
            FD_SET(service->server_fd, &read_fds);
        }

        // 设置超时
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // 等待活动
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0 && errno != EINTR) {
            fprintf(stderr, "SoftAP select error: %s\n", strerror(errno));
            break;
        } else if (activity > 0) {
            // 服务器套接字有活动
            if (service->server_fd >= 0 && FD_ISSET(service->server_fd, &read_fds)) {
                struct sockaddr_in address;
                socklen_t addrlen = sizeof(address);
                int new_socket = accept(service->server_fd, (struct sockaddr*)&address, &addrlen);

                if (new_socket < 0) {
                    perror("accept");
                    continue;
                }

                // 模拟客户端连接
                char mac_addr[18];
                snprintf(mac_addr, sizeof(mac_addr), "%02x:%02x:%02x:%02x:%02x:%02x",
                         rand() % 256, rand() % 256, rand() % 256,
                         rand() % 256, rand() % 256, rand() % 256);
                char ip_addr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(address.sin_addr), ip_addr, INET_ADDRSTRLEN);

                // 添加客户端
                softap_add_client(service, mac_addr, ip_addr);

                close(new_socket);
            }
        }

        // 模拟流量统计
        pthread_mutex_lock(&service->mutex);
        service->session.tx_bytes += 1024; // 模拟发送字节数
        service->session.rx_bytes += 512;  // 模拟接收字节数
        pthread_mutex_unlock(&service->mutex);

        // 每秒检查一次
        sleep(1);
    }

    return NULL;
}

// 设置服务器套接字
static int softap_setup_server(SoftapService* service) {
    struct sockaddr_in address;
    int opt = 1;

    // 创建套接字文件描述符
    if ((service->server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        return -1;
    }

    // 设置套接字选项
    if (setsockopt(service->server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(service->server_fd);
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(service->config.port);

    // 绑定套接字到端口
    if (bind(service->server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(service->server_fd);
        return -1;
    }

    // 监听连接
    if (listen(service->server_fd, service->config.max_clients) < 0) {
        perror("listen");
        close(service->server_fd);
        return -1;
    }

    return 0;
}

// 清理服务器
static void softap_cleanup_server(SoftapService* service) {
    if (service->server_fd >= 0) {
        close(service->server_fd);
        service->server_fd = -1;
    }
}