#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <math.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <avahi-common/alternative.h>
#include <avahi-common/strlst.h>
#include <avahi-common/timeval.h>

#include "airplay2.h"
#include "../alsa/alsa_plugin.h"
#include "../../audio_processing/audio_processing.h"

// 前向声明
static void* airplay2_thread_func(void* data);
static int airplay2_setup_server(AirPlay2Service* service);
static void airplay2_cleanup_connections(AirPlay2Service* service);
static void airplay2_avahi_callback(AvahiClient* c, AvahiClientState state, void* userdata);
static void airplay2_create_avahi_service(AirPlay2Service* service);
static int airplay2_handle_client(AirPlay2Service* service);
static int airplay2_process_audio(AirPlay2Service* service, const uint8_t* data, size_t len);
static void airplay2_set_state(AirPlay2Service* service, AirPlay2State state);

// 创建AirPlay 2服务
AirPlay2Service* airplay2_create(struct pw_context* context, const AirPlay2Config* config) {
    if (!context || !config) {
        fprintf(stderr, "Invalid parameters for airplay2_create\n");
        return NULL;
    }

    AirPlay2Service* service = calloc(1, sizeof(AirPlay2Service));
    if (!service) {
        fprintf(stderr, "Failed to allocate memory for AirPlay2Service\n");
        return NULL;
    }

    // 初始化互斥锁
    pthread_mutex_init(&service->mutex, NULL);

    // 设置上下文
    service->context = context;
    service->core = pw_context_connect(context, NULL, 0);
    if (!service->core) {
        fprintf(stderr, "Failed to connect to PipeWire core\n");
        pthread_mutex_destroy(&service->mutex);
        free(service);
        return NULL;
    }

    // 复制配置
    service->config = *config;
    if (service->config.port == 0) {
        service->config.port = 7000; // 默认端口
    }
    if (strlen(service->config.device_name) == 0) {
        snprintf(service->config.device_name, sizeof(service->config.device_name), "RealTimeAudioAirPlay");
    }
    if (strlen(service->config.device_id) == 0) {
        // 生成简单的设备ID
        snprintf(service->config.device_id, sizeof(service->config.device_id), "RTAP-%08X", rand());
    }
    service->config.volume = fmaxf(0.0f, fminf(1.0f, service->config.volume));

    // 初始状态
    service->state = AIRPLAY2_STATE_DISCONNECTED;
    service->running = false;
    service->server_fd = -1;
    service->client_fd = -1;

    return service;
}

// 销毁AirPlay 2服务
void airplay2_destroy(AirPlay2Service* service) {
    if (!service) return;

    airplay2_stop(service);

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

    // 清理PipeWire资源
    if (service->stream) {
        pw_stream_destroy(service->stream);
    }
    if (service->core) {
        pw_core_disconnect(service->core);
    }

    // 清理互斥锁
    pthread_mutex_destroy(&service->mutex);

    free(service);
}

// 启动AirPlay 2服务
int airplay2_start(AirPlay2Service* service) {
    if (!service || service->running) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 设置运行标志
    service->running = true;

    // 创建服务器套接字
    if (airplay2_setup_server(service) < 0) {
        fprintf(stderr, "Failed to setup AirPlay 2 server\n");
        service->running = false;
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 初始化Avahi
    service->avahi_poll = avahi_threaded_poll_new();
    if (!service->avahi_poll) {
        fprintf(stderr, "Failed to create Avahi threaded poll\n");
        airplay2_cleanup_connections(service);
        service->running = false;
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 创建Avahi客户端
    int error;
    service->avahi_client = avahi_client_new(avahi_threaded_poll_get(service->avahi_poll), AVAHI_CLIENT_NO_FAIL, airplay2_avahi_callback, service, &error);
    if (!service->avahi_client) {
        fprintf(stderr, "Failed to create Avahi client: %s\n", avahi_strerror(error));
        avahi_threaded_poll_free(service->avahi_poll);
        service->avahi_poll = NULL;
        airplay2_cleanup_connections(service);
        service->running = false;
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 启动Avahi线程池
    avahi_threaded_poll_start(service->avahi_poll);

    // 创建工作线程
    if (pthread_create(&service->thread, NULL, airplay2_thread_func, service) != 0) {
        fprintf(stderr, "Failed to create AirPlay 2 thread\n");
        avahi_threaded_poll_stop(service->avahi_poll);
        avahi_client_free(service->avahi_client);
        avahi_threaded_poll_free(service->avahi_poll);
        service->avahi_poll = NULL;
        service->avahi_client = NULL;
        airplay2_cleanup_connections(service);
        service->running = false;
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 停止AirPlay 2服务
void airplay2_stop(AirPlay2Service* service) {
    if (!service || !service->running) {
        return;
    }

    pthread_mutex_lock(&service->mutex);

    // 设置停止标志
    service->running = false;

    // 清理连接
    airplay2_cleanup_connections(service);

    pthread_mutex_unlock(&service->mutex);

    // 等待线程结束
    if (pthread_join(service->thread, NULL) != 0) {
        fprintf(stderr, "Failed to join AirPlay 2 thread\n");
    }

    // 更新状态
    airplay2_set_state(service, AIRPLAY2_STATE_DISCONNECTED);
}

// 设置AirPlay 2音量
int airplay2_set_volume(AirPlay2Service* service, float volume) {
    if (!service) {
        return -1;
    }

    volume = fmaxf(0.0f, fminf(1.0f, volume));

    pthread_mutex_lock(&service->mutex);
    service->config.volume = volume;
    pthread_mutex_unlock(&service->mutex);

    return 0;
}

// 获取AirPlay 2状态
AirPlay2State airplay2_get_state(AirPlay2Service* service) {
    if (!service) {
        return AIRPLAY2_STATE_ERROR;
    }

    AirPlay2State state;
    pthread_mutex_lock(&service->mutex);
    state = service->state;
    pthread_mutex_unlock(&service->mutex);
    return state;
}

// 获取当前会话信息
const AirPlay2Session* airplay2_get_session(AirPlay2Service* service) {
    if (!service || service->state < AIRPLAY2_STATE_CONNECTED) {
        return NULL;
    }

    return &service->session;
}

// 工作线程函数
static void* airplay2_thread_func(void* data) {
    AirPlay2Service* service = (AirPlay2Service*)data;
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

        FD_SET(service->server_fd, &read_fds);

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
            fprintf(stderr, "AirPlay 2 select error: %s\n", strerror(errno));
            break;
        } else if (activity > 0) {
            // 服务器套接字有活动
            if (FD_ISSET(service->server_fd, &read_fds)) {
                airplay2_handle_client(service);
            }

            // 客户端套接字有活动
            if (service->client_fd >= 0 && FD_ISSET(service->client_fd, &read_fds)) {
                uint8_t buffer[4096];
                ssize_t bytes_read = recv(service->client_fd, buffer, sizeof(buffer), 0);

                if (bytes_read <= 0) {
                    // 连接关闭或出错
                    fprintf(stderr, "AirPlay 2 client disconnected\n");
                    airplay2_cleanup_connections(service);
                    airplay2_set_state(service, AIRPLAY2_STATE_DISCONNECTED);
                } else {
                    // 处理音频数据
                    airplay2_process_audio(service, buffer, bytes_read);
                }
            }
        }
    }

    return NULL;
}

// 设置服务器套接字
static int airplay2_setup_server(AirPlay2Service* service) {
    struct sockaddr_in server_addr;

    // 创建TCP套接字
    service->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (service->server_fd < 0) {
        fprintf(stderr, "Failed to create AirPlay 2 socket: %s\n", strerror(errno));
        return -1;
    }

    // 设置套接字选项
    int opt = 1;
    if (setsockopt(service->server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        fprintf(stderr, "Failed to set socket options: %s\n", strerror(errno));
        close(service->server_fd);
        service->server_fd = -1;
        return -1;
    }

    // 绑定地址和端口
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(service->config.port);

    if (bind(service->server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Failed to bind AirPlay 2 socket: %s\n", strerror(errno));
        close(service->server_fd);
        service->server_fd = -1;
        return -1;
    }

    // 监听连接
    if (listen(service->server_fd, 5) < 0) {
        fprintf(stderr, "Failed to listen on AirPlay 2 socket: %s\n", strerror(errno));
        close(service->server_fd);
        service->server_fd = -1;
        return -1;
    }

    return 0;
}

// 清理连接
static void airplay2_cleanup_connections(AirPlay2Service* service) {
    if (service->client_fd >= 0) {
        close(service->client_fd);
        service->client_fd = -1;
    }

    // 重置会话
    memset(&service->session, 0, sizeof(AirPlay2Session));
}

// Avahi回调函数
static void airplay2_avahi_callback(AvahiClient* c, AvahiClientState state, void* userdata) {
    AirPlay2Service* service = (AirPlay2Service*)userdata;

    if (!service->running) {
        return;
    }

    switch (state) {
        case AVAHI_CLIENT_S_RUNNING:
            // 客户端运行中，创建服务
            if (!service->avahi_group) {
                airplay2_create_avahi_service(service);
            }
            break;

        case AVAHI_CLIENT_S_COLLISION:
            // 发生冲突，需要重新创建服务
            if (service->avahi_group) {
                avahi_entry_group_reset(service->avahi_group);
                airplay2_create_avahi_service(service);
            }
            break;

        case AVAHI_CLIENT_FAILURE:
            fprintf(stderr, "Avahi client failure: %s\n", avahi_strerror(avahi_client_errno(c)));
            break;

        default:
            break;
    }
}

// 创建Avahi服务
static void airplay2_create_avahi_service(AirPlay2Service* service) {
    char service_name[256];
    char port_str[8];
    AvahiStringList* txt = NULL;
    int ret;

    // 格式化服务名称
    snprintf(service_name, sizeof(service_name), "%s@AirPlay", service->config.device_name);

    // 格式化端口
    snprintf(port_str, sizeof(port_str), "%d", service->config.port);

    // 创建TXT记录
    txt = avahi_string_list_add_printf(txt, "txtvers=1");
    txt = avahi_string_list_add_printf(txt, "features=0x5A7FFFF7");
    txt = avahi_string_list_add_printf(txt, "model=AirPort");
    txt = avahi_string_list_add_printf(txt, "pw=%s", service->config.require_password ? service->config.password : "");
    txt = avahi_string_list_add_printf(txt, "sr=%u", 44100);
    txt = avahi_string_list_add_printf(txt, "ss=%u", 16);
    txt = avahi_string_list_add_printf(txt, "ch=%u", 2);
    txt = avahi_string_list_add_printf(txt, "cn=0,1");
    txt = avahi_string_list_add_printf(txt, "et=0,3");
    txt = avahi_string_list_add_printf(txt, "sv=AirPlay");
    txt = avahi_string_list_add_printf(txt, "ek=1");

    // 创建服务组
    if (!service->avahi_group) {
        service->avahi_group = avahi_entry_group_new(service->avahi_client, NULL);
        if (!service->avahi_group) {
            fprintf(stderr, "Failed to create Avahi entry group: %s\n", avahi_strerror(avahi_client_errno(service->avahi_client)));
            avahi_string_list_free(txt);
            return;
        }
    }

    // 添加服务
    ret = avahi_entry_group_add_service_strlst(
        service->avahi_group,
        AVAHI_IF_UNSPEC,
        AVAHI_PROTO_UNSPEC,
        0,
        service_name,
        "_airplay._tcp",
        NULL,
        NULL,
        service->config.port,
        txt
    );

    // 释放TXT记录
    avahi_string_list_free(txt);

    if (ret < 0) {
        fprintf(stderr, "Failed to add AirPlay 2 service: %s\n", avahi_strerror(ret));
        avahi_entry_group_reset(service->avahi_group);
        return;
    }

    // 提交服务组
    ret = avahi_entry_group_commit(service->avahi_group);
    if (ret < 0) {
        fprintf(stderr, "Failed to commit AirPlay 2 service group: %s\n", avahi_strerror(ret));
        avahi_entry_group_reset(service->avahi_group);
        return;
    }

    printf("AirPlay 2 service published as '%s' on port %d\n", service_name, service->config.port);
}

// 处理客户端连接
static int airplay2_handle_client(AirPlay2Service* service) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int new_socket = accept(service->server_fd, (struct sockaddr*)&client_addr, &client_len);

    if (new_socket < 0) {
        fprintf(stderr, "Failed to accept AirPlay 2 connection: %s\n", strerror(errno));
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 关闭现有连接
    if (service->client_fd >= 0) {
        close(service->client_fd);
    }

    // 设置新客户端
    service->client_fd = new_socket;
    strncpy(service->session.client_ip, inet_ntoa(client_addr.sin_addr), sizeof(service->session.client_ip) - 1);

    // 设置状态
    airplay2_set_state(service, AIRPLAY2_STATE_CONNECTED);

    printf("AirPlay 2 client connected from %s\n", service->session.client_ip);

    pthread_mutex_unlock(&service->mutex);

    return 0;
}

// 处理音频数据
static int airplay2_process_audio(AirPlay2Service* service, const uint8_t* data, size_t len) {
    if (!service || service->state < AIRPLAY2_STATE_CONNECTED) {
        return -1;
    }

    // 如果还没有开始流媒体，设置状态
    if (service->state == AIRPLAY2_STATE_CONNECTED) {
        airplay2_set_state(service, AIRPLAY2_STATE_STREAMING);

        // 初始化音频格式
        service->session.format.format = SPA_AUDIO_FORMAT_F32;
        service->session.format.channels = 2;
        service->session.format.rate = 44100;

        // 创建PipeWire流
        if (!service->stream) {
            struct pw_properties* props = pw_properties_new(
                PW_KEY_MEDIA_TYPE, "Audio",
                PW_KEY_MEDIA_CATEGORY, "Playback",
                PW_KEY_MEDIA_ROLE, "Music",
                PW_KEY_STREAM_NAME, "AirPlay 2 Stream",
                NULL
            );

            if (props) {
                service->stream = pw_stream_new_simple(
                    service->context,
                    "airplay2-stream",
                    props,
                    NULL
                );
            }
        }
    }

    // 转换音频格式并应用音量
    // 简化实现：假设数据是PCM格式
    float* audio_data = (float*)data;
    size_t num_samples = len / sizeof(float);

    // 应用音量
    for (size_t i = 0; i < num_samples; i++) {
        audio_data[i] *= service->config.volume;
    }

    // 将音频数据发送到PipeWire流
    if (service->stream) {
        // 实际实现中应使用pw_stream_write
    }

    return 0;
}

// 设置状态
static void airplay2_set_state(AirPlay2Service* service, AirPlay2State state) {
    if (!service || service->state == state) return;

    AirPlay2State old_state = service->state;
    char details[512];
    snprintf(details, sizeof(details), "{\"event\":\"connection_state_changed\",\"old_state\":%d,\"new_state\":%d,\"client_ip\":\"%s\",\"timestamp\":%lld}",
             old_state, state, service->session.client_ip, (long long)time(NULL));

    pthread_mutex_lock(&service->mutex);
    service->state = state;
    pthread_mutex_unlock(&service->mutex);

    dbus_emit_signal("AirPlay2", DBUS_SIGNAL_CONNECTION_STATE_CHANGED, details);
    printf("AirPlay 2 state changed from %d to %d\n", old_state, state);
}