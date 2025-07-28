#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <jansson.h>
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

#include "connman.h"
#include "../../include/dbus_utils.h"

// 前向声明
static void* connman_worker_thread(void* data);
static int connman_setup_server(ConnManService* service);
static void connman_cleanup_connections(ConnManService* service);
static void connman_avahi_callback(AvahiClient* c, AvahiClientState state, void* userdata);
static void connman_create_avahi_service(ConnManService* service);
static int connman_handle_client(ConnManService* service);
static void connman_set_state(ConnManService* service, ConnManState state);

// 创建ConnMan服务
ConnManService* connman_create(struct pw_context* context, const ConnManConfig* config) {
    if (!context || !config) {
        fprintf(stderr, "Invalid parameters for connman_create\n");
        return NULL;
    }

    ConnManService* service = calloc(1, sizeof(ConnManService));
    if (!service) {
        fprintf(stderr, "Failed to allocate memory for ConnManService\n");
        return NULL;
    }

    // 初始化D-Bus
    if (!dbus_initialize()) {
        fprintf(stderr, "Failed to initialize D-Bus connection for ConnMan\n");
        // 继续初始化但记录错误
    }

    // 初始化互斥锁
    pthread_mutex_init(&service->mutex, NULL);

    // 设置上下文
    service->context = context;
    service->state = CONNMAN_STATE_DISCONNECTED;
    service->running = false;
    service->server_fd = -1;
    service->client_fd = -1;

    // 复制配置
    service->config = *config;
    if (service->config.port == 0) {
        service->config.port = 10000;
    }
    if (!service->config.device_name[0]) {
        snprintf(service->config.device_name, sizeof(service->config.device_name), "RealTimeConnMan");
    }
    if (!service->config.friendly_name[0]) {
        snprintf(service->config.friendly_name, sizeof(service->config.friendly_name), "RealTime ConnMan");
    }

    return service;
}

// 销毁ConnMan服务
void connman_destroy(ConnManService* service) {
    if (!service) return;

    connman_stop(service);

    // 清理Avahi资源
    if (service->avahi_group) {
        avahi_entry_group_free(service->avahi_group);
    }
    if (service->avahi_client) {
        avahi_client_free(service->avahi_client);
    }
    if (service->avahi_poll) {
        avahi_threaded_poll_stop(service->avahi_poll);
        avahi_threaded_poll_free(service->avahi_poll);
    }

    // 清理互斥锁
    pthread_mutex_destroy(&service->mutex);

    // 清理D-Bus
    dbus_cleanup();

    free(service);
}

// 启动ConnMan服务
int connman_start(ConnManService* service) {
    if (!service || service->running) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 设置运行标志
    service->running = true;

    // 创建服务器套接字
    if (connman_setup_server(service) < 0) {
        fprintf(stderr, "Failed to setup ConnMan server\n");
        service->running = false;
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 初始化Avahi
    if (service->config.enable_discovery) {
        service->avahi_poll = avahi_threaded_poll_new();
        if (!service->avahi_poll) {
            fprintf(stderr, "Failed to create Avahi threaded poll\n");
            connman_cleanup_connections(service);
            service->running = false;
            pthread_mutex_unlock(&service->mutex);
            return -1;
        }

        // 创建Avahi客户端
        int error;
        service->avahi_client = avahi_client_new(avahi_threaded_poll_get(service->avahi_poll), AVAHI_CLIENT_NO_FAIL, connman_avahi_callback, service, &error);
        if (!service->avahi_client) {
            fprintf(stderr, "Failed to create Avahi client: %s\n", avahi_strerror(error));
            avahi_threaded_poll_free(service->avahi_poll);
            service->avahi_poll = NULL;
            connman_cleanup_connections(service);
            service->running = false;
            pthread_mutex_unlock(&service->mutex);
            return -1;
        }

        // 启动Avahi线程池
        avahi_threaded_poll_start(service->avahi_poll);
    }

    // 创建工作线程
    if (pthread_create(&service->thread, NULL, connman_worker_thread, service) != 0) {
        fprintf(stderr, "Failed to create ConnMan worker thread\n");
        if (service->config.enable_discovery) {
            avahi_threaded_poll_stop(service->avahi_poll);
            avahi_client_free(service->avahi_client);
            avahi_threaded_poll_free(service->avahi_poll);
            service->avahi_poll = NULL;
            service->avahi_client = NULL;
        }
        connman_cleanup_connections(service);
        service->running = false;
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 设置状态为发现中
    connman_set_state(service, CONNMAN_STATE_DISCONNECTED);

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 停止ConnMan服务
void connman_stop(ConnManService* service) {
    if (!service || !service->running) {
        return;
    }

    pthread_mutex_lock(&service->mutex);
    service->running = false;
    pthread_mutex_unlock(&service->mutex);

    // 等待线程结束
    pthread_join(service->thread, NULL);

    // 清理连接
    connman_cleanup_connections(service);

    // 设置状态为未连接
    connman_set_state(service, CONNMAN_STATE_DISCONNECTED);
}

// 连接到网络
int connman_connect(ConnManService* service, const char* ssid, const char* password) {
    if (!service || !ssid || service->state != CONNMAN_STATE_DISCONNECTED) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 模拟连接过程
    connman_set_state(service, CONNMAN_STATE_CONNECTING);

    // 在实际实现中，这里应该调用wpa_supplicant API来建立连接
    // 简化处理，直接设置为已连接状态
    strncpy(service->session.ssid, ssid, sizeof(service->session.ssid)-1);
    service->session.signal_strength = 85; // 模拟信号强度
    service->session.connection_time = time(NULL);
    strncpy(service->session.ip_address, "192.168.1.100", sizeof(service->session.ip_address)-1);
    strncpy(service->session.mac_address, "aa:bb:cc:dd:ee:ff", sizeof(service->session.mac_address)-1);

    connman_set_state(service, CONNMAN_STATE_CONNECTED);

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 断开网络连接
int connman_disconnect(ConnManService* service) {
    if (!service || service->state != CONNMAN_STATE_CONNECTED) {
        return -1;
    }

    connman_set_state(service, CONNMAN_STATE_DISCONNECTED);
    memset(&service->session, 0, sizeof(ConnManSession));
    return 0;
}

// 获取当前状态
ConnManState connman_get_state(ConnManService* service) {
    if (!service) {
        return CONNMAN_STATE_ERROR;
    }

    ConnManState state;
    pthread_mutex_lock(&service->mutex);
    state = service->state;
    pthread_mutex_unlock(&service->mutex);
    return state;
}

// 获取会话信息
const ConnManSession* connman_get_session(ConnManService* service) {
    if (!service || service->state != CONNMAN_STATE_CONNECTED) {
        return NULL;
    }

    return &service->session;
}

// 获取错误信息
const char* connman_get_error(ConnManService* service) {
    if (!service) {
        return "Invalid service pointer";
    }

    return service->error_msg;
}

// 设置状态
static void connman_set_state(ConnManService* service, ConnManState state) {
    if (!service) return;

    pthread_mutex_lock(&service->mutex);
    ConnManState old_state = service->state;
    service->state = state;
    pthread_mutex_unlock(&service->mutex);

    // 只有状态实际改变时才发送信号
    if (old_state != state) {
        // 创建JSON payload
        json_t* details = json_object();
        json_object_set_new(details, "old_state", json_integer(old_state));
        json_object_set_new(details, "new_state", json_integer(state));
        json_object_set_new(details, "ssid", json_string(service->session.ssid));
        json_object_set_new(details, "ip_address", json_string(service->session.ip_address));
        json_object_set_new(details, "timestamp", json_integer(time(NULL)));

        const char* json_str = json_dumps(details, JSON_COMPACT);
        if (json_str) {
            // 发送D-Bus信号
            dbus_emit_signal("com.realtimeaudio.ConnMan", DBUS_SIGNAL_TYPE_STATE_CHANGED, json_str);
            free((void*)json_str);
        }
        json_decref(details);

        // 日志记录
        printf("ConnMan state changed from %d to %d\n", old_state, state);
    }
}

// 工作线程函数
static void* connman_worker_thread(void* data) {
    ConnManService* service = (ConnManService*)data;
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

        // 设置文件描述符集
        FD_ZERO(&read_fds);
        max_fd = service->server_fd;

        if (service->server_fd >= 0) {
            FD_SET(service->server_fd, &read_fds);
        }

        if (service->client_fd >= 0) {
            FD_SET(service->client_fd, &read_fds);
            max_fd = fmax(max_fd, service->client_fd);
        }

        // 设置超时
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // 等待活动
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0 && errno != EINTR) {
            fprintf(stderr, "ConnMan select error: %s\n", strerror(errno));
            break;
        } else if (activity > 0) {
            // 服务器套接字有活动
            if (service->server_fd >= 0 && FD_ISSET(service->server_fd, &read_fds)) {
                connman_handle_client(service);
            }

            // 客户端套接字有活动
            if (service->client_fd >= 0 && FD_ISSET(service->client_fd, &read_fds)) {
                // 处理客户端数据
            }
        }

        // 更新连接时长
        if (service->state == CONNMAN_STATE_CONNECTED) {
            pthread_mutex_lock(&service->mutex);
            service->session.connection_time = time(NULL);
            pthread_mutex_unlock(&service->mutex);
        }
    }

    return NULL;
}

// 设置服务器套接字
static int connman_setup_server(ConnManService* service) {
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
    if (listen(service->server_fd, 3) < 0) {
        perror("listen");
        close(service->server_fd);
        return -1;
    }

    return 0;
}

// 清理连接
static void connman_cleanup_connections(ConnManService* service) {
    if (service->client_fd >= 0) {
        close(service->client_fd);
        service->client_fd = -1;
    }

    if (service->server_fd >= 0) {
        close(service->server_fd);
        service->server_fd = -1;
    }
}

// 处理客户端连接
static int connman_handle_client(ConnManService* service) {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int new_socket;

    if ((new_socket = accept(service->server_fd, (struct sockaddr*)&address, &addrlen)) < 0) {
        perror("accept");
        return -1;
    }

    // 如果已有客户端连接，先关闭
    if (service->client_fd >= 0) {
        close(service->client_fd);
    }

    service->client_fd = new_socket;
    return 0;
}

// Avahi回调函数
static void connman_avahi_callback(AvahiClient* c, AvahiClientState state, void* userdata) {
    ConnManService* service = userdata;

    if (state == AVAHI_CLIENT_S_RUNNING) {
        connman_create_avahi_service(service);
    } else if (state == AVAHI_CLIENT_FAILURE) {
        fprintf(stderr, "Avahi client failure: %s\n", avahi_strerror(avahi_client_errno(c)));
    }
}

// 创建Avahi服务
static void connman_create_avahi_service(ConnManService* service) {
    // 实现mDNS服务发现
    // 在实际实现中，这里应该发布ConnMan服务信息
}