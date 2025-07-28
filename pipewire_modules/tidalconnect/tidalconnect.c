#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <curl/curl.h>
#include <jansson.h>
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "tidalconnect.h"
#include "../../audio_processing/audio_processing.h"
#include "../alsa/alsa_plugin.h"
#include "../flowdsp/flowdsp.h"
#include "../../include/dbus_utils.h"

// 前向声明
static void* tidal_connect_worker_thread(void* data);
static int tidal_connect_setup_server(TidalConnectService* service);
static void tidal_connect_cleanup_connections(TidalConnectService* service);
static void tidal_connect_avahi_callback(AvahiClient* c, AvahiClientState state, void* userdata);
static void tidal_connect_create_avahi_service(TidalConnectService* service);
static int tidal_connect_handle_client(TidalConnectService* service);
static int tidal_connect_authenticate(TidalConnectService* service);
static int tidal_connect_refresh_token(TidalConnectService* service);
static int tidal_connect_fetch_track_info(TidalConnectService* service, const char* track_id);
static int tidal_connect_setup_audio_stream(TidalConnectService* service);
static void tidal_connect_set_state(TidalConnectService* service, TidalConnectState state);
static size_t tidal_connect_curl_write_callback(void* contents, size_t size, size_t nmemb, void* userp);
static int tidal_connect_parse_track_response(TidalConnectService* service, const char* response);

// 创建Tidal Connect服务
TidalConnectService* tidal_connect_create(struct pw_context* context, const TidalConnectConfig* config) {
    if (!dbus_initialize()) {
        fprintf(stderr, "Failed to initialize D-Bus connection for Tidal Connect\n");
        // Continue without D-Bus for now, but log the error
    }
    if (!context || !config) {
        fprintf(stderr, "Invalid parameters for tidal_connect_create\n");
        return NULL;
    }

    TidalConnectService* service = calloc(1, sizeof(TidalConnectService));
    if (!service) {
        fprintf(stderr, "Failed to allocate memory for TidalConnectService\n");
        return NULL;
    }

    // 初始化互斥锁
    pthread_mutex_init(&service->mutex, NULL);

    // 设置上下文
    service->context = context;
    service->state = TIDAL_CONNECT_STATE_DISCONNECTED;
    service->running = false;
    service->server_fd = -1;
    service->client_fd = -1;

    // 复制配置
    service->config = *config;
    if (service->config.port == 0) {
        service->config.port = 6510;
    }
    if (!service->config.device_name[0]) {
        snprintf(service->config.device_name, sizeof(service->config.device_name), "RealTimeTidal");
    }
    if (!service->config.friendly_name[0]) {
        snprintf(service->config.friendly_name, sizeof(service->config.friendly_name), "RealTime Tidal Connect");
    }
    if (!service->config.device_id[0]) {
        // 生成设备ID
        snprintf(service->config.device_id, sizeof(service->config.device_id), "RT-%08X%08X", rand(), rand());
    }
    service->config.initial_volume = fmaxf(0.0f, fminf(1.0f, service->config.initial_volume));
    service->session.volume = service->config.initial_volume;

    return service;
}

// 销毁Tidal Connect服务
void tidal_connect_destroy(TidalConnectService* service) {
    if (!service) return;

    tidal_connect_stop(service);

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
        service->stream = NULL;
    }

    // 清理互斥锁
    pthread_mutex_destroy(&service->mutex);

    free(service);
}

// 启动Tidal Connect服务
int tidal_connect_start(TidalConnectService* service) {
    if (!service || service->running) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 设置运行标志
    service->running = true;

    // 创建服务器套接字
    if (tidal_connect_setup_server(service) < 0) {
        fprintf(stderr, "Failed to setup Tidal Connect server\n");
        service->running = false;
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 初始化Avahi
    if (service->config.enable_discovery) {
        service->avahi_poll = avahi_threaded_poll_new();
        if (!service->avahi_poll) {
            fprintf(stderr, "Failed to create Avahi threaded poll\n");
            tidal_connect_cleanup_connections(service);
            service->running = false;
            pthread_mutex_unlock(&service->mutex);
            return -1;
        }

        // 创建Avahi客户端
        int error;
        service->avahi_client = avahi_client_new(avahi_threaded_poll_get(service->avahi_poll), AVAHI_CLIENT_NO_FAIL, tidal_connect_avahi_callback, service, &error);
        if (!service->avahi_client) {
            fprintf(stderr, "Failed to create Avahi client: %s\n", avahi_strerror(error));
            avahi_threaded_poll_free(service->avahi_poll);
            service->avahi_poll = NULL;
            tidal_connect_cleanup_connections(service);
            service->running = false;
            pthread_mutex_unlock(&service->mutex);
            return -1;
        }

        // 启动Avahi线程池
        avahi_threaded_poll_start(service->avahi_poll);
    }

    // 创建工作线程
    if (pthread_create(&service->thread, NULL, tidal_connect_worker_thread, service) != 0) {
        fprintf(stderr, "Failed to create Tidal Connect worker thread\n");
        if (service->config.enable_discovery) {
            avahi_threaded_poll_stop(service->avahi_poll);
            avahi_client_free(service->avahi_client);
            avahi_threaded_poll_free(service->avahi_poll);
            service->avahi_poll = NULL;
            service->avahi_client = NULL;
        }
        tidal_connect_cleanup_connections(service);
        service->running = false;
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 设置状态为发现中
    tidal_connect_set_state(service, TIDAL_CONNECT_STATE_DISCOVERING);

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 停止Tidal Connect服务
void tidal_connect_stop(TidalConnectService* service) {
    if (!service || !service->running) {
        return;
    }

    pthread_mutex_lock(&service->mutex);
    service->running = false;
    pthread_mutex_unlock(&service->mutex);

    // 等待线程结束
    pthread_join(service->thread, NULL);

    // 清理连接
    tidal_connect_cleanup_connections(service);

    // 设置状态为未连接
    tidal_connect_set_state(service, TIDAL_CONNECT_STATE_DISCONNECTED);
}

// 播放媒体
int tidal_connect_play(TidalConnectService* service) {
    if (!service || service->state < TIDAL_CONNECT_STATE_AUTHENTICATED) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 如果没有当前曲目，尝试加载第一个曲目
    if (!service->session.current_track.track_id[0]) {
        // 在实际实现中，应从当前播放列表加载第一个曲目
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 设置状态为播放中
    tidal_connect_set_state(service, TIDAL_CONNECT_STATE_PLAYING);

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 暂停播放
int tidal_connect_pause(TidalConnectService* service) {
    if (!service || service->state != TIDAL_CONNECT_STATE_PLAYING) {
        return -1;
    }

    tidal_connect_set_state(service, TIDAL_CONNECT_STATE_PAUSED);
    return 0;
}

// 停止播放
int tidal_connect_stop_playback(TidalConnectService* service) {
    if (!service || service->state < TIDAL_CONNECT_STATE_AUTHENTICATED) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 清空当前曲目
    memset(&service->session.current_track, 0, sizeof(TidalTrack));
    service->session.position_ms = 0;

    tidal_connect_set_state(service, TIDAL_CONNECT_STATE_AUTHENTICATED);

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 下一曲目
int tidal_connect_next_track(TidalConnectService* service) {
    if (!service || service->state < TIDAL_CONNECT_STATE_AUTHENTICATED) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 在实际实现中，应从播放列表加载下一个曲目
    tidal_connect_set_state(service, TIDAL_CONNECT_STATE_BUFFERING);

    // 模拟加载新曲目
    service->session.position_ms = 0;
    tidal_connect_set_state(service, TIDAL_CONNECT_STATE_PLAYING);

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 上一曲目
int tidal_connect_previous_track(TidalConnectService* service) {
    if (!service || service->state < TIDAL_CONNECT_STATE_AUTHENTICATED) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 在实际实现中，应从播放列表加载上一个曲目
    tidal_connect_set_state(service, TIDAL_CONNECT_STATE_BUFFERING);

    // 模拟加载新曲目
    service->session.position_ms = 0;
    tidal_connect_set_state(service, TIDAL_CONNECT_STATE_PLAYING);

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 调整播放位置
int tidal_connect_seek(TidalConnectService* service, uint64_t position_ms) {
    if (!service || service->state < TIDAL_CONNECT_STATE_PLAYING) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);
    service->session.position_ms = position_ms;
    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 设置音量
int tidal_connect_set_volume(TidalConnectService* service, float volume) {
    if (!service) {
        return -1;
    }

    volume = fmaxf(0.0f, fminf(1.0f, volume));

    pthread_mutex_lock(&service->mutex);
    service->session.volume = volume;
    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 设置静音
int tidal_connect_set_mute(TidalConnectService* service, bool muted) {
    if (!service) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);
    service->session.muted = muted;
    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 设置音频质量
int tidal_connect_set_quality(TidalConnectService* service, TidalAudioQuality quality) {
    if (!service) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);
    service->config.audio_quality = quality;
    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 设置随机播放
int tidal_connect_set_shuffle(TidalConnectService* service, bool shuffle) {
    if (!service || service->state < TIDAL_CONNECT_STATE_AUTHENTICATED) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);
    service->session.shuffle = shuffle;
    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 设置重复模式
int tidal_connect_set_repeat(TidalConnectService* service, int repeat_mode) {
    if (!service || service->state < TIDAL_CONNECT_STATE_AUTHENTICATED || repeat_mode < 0 || repeat_mode > 2) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);
    service->session.repeat_mode = repeat_mode;
    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 获取当前状态
TidalConnectState tidal_connect_get_state(TidalConnectService* service) {
    if (!service) {
        return TIDAL_CONNECT_STATE_ERROR;
    }

    TidalConnectState state;
    pthread_mutex_lock(&service->mutex);
    state = service->state;
    pthread_mutex_unlock(&service->mutex);
    return state;
}

// 获取会话信息
const TidalConnectSession* tidal_connect_get_session(TidalConnectService* service) {
    if (!service || service->state < TIDAL_CONNECT_STATE_CONNECTED) {
        return NULL;
    }

    return &service->session;
}

// 获取错误信息
const char* tidal_connect_get_error(TidalConnectService* service) {
    if (!service) {
        return "Invalid service pointer";
    }

    return service->error_msg;
}

// 工作线程函数
static void* tidal_connect_worker_thread(void* data) {
    TidalConnectService* service = (TidalConnectService*)data;
    fd_set read_fds;
    struct timeval timeout;
    int max_fd;

    // 初始化CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);

    while (1) {
        pthread_mutex_lock(&service->mutex);
        bool running = service->running;
        pthread_mutex_unlock(&service->mutex);

        if (!running) {
            break;
        }

        // 检查令牌是否过期
        if (service->state >= TIDAL_CONNECT_STATE_AUTHENTICATED) {
            uint64_t now = time(NULL);
            if (service->session.token_expiry > 0 && now > service->session.token_expiry - 60) {
                tidal_connect_refresh_token(service);
            }
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
            fprintf(stderr, "Tidal Connect select error: %s\n", strerror(errno));
            break;
        } else if (activity > 0) {
            // 服务器套接字有活动
            if (service->server_fd >= 0 && FD_ISSET(service->server_fd, &read_fds)) {
                tidal_connect_handle_client(service);
            }

            // 客户端套接字有活动
            if (service->client_fd >= 0 && FD_ISSET(service->client_fd, &read_fds)) {
                uint8_t buffer[8192];
                ssize_t bytes_read = recv(service->client_fd, buffer, sizeof(buffer), 0);

                if (bytes_read <= 0) {
                    // 连接关闭或出错
                    fprintf(stderr, "Tidal Connect client disconnected\n");
                    tidal_connect_cleanup_connections(service);
                    tidal_connect_set_state(service, TIDAL_CONNECT_STATE_DISCONNECTED);
                    if (service->config.enable_discovery) {
                        tidal_connect_set_state(service, TIDAL_CONNECT_STATE_DISCOVERING);
                    }
                } else {
                    // 处理接收到的数据
                    buffer[bytes_read] = '\0';
                    // 在实际实现中，应解析Tidal Connect协议消息
                }
            }
        }

        // 更新播放位置
        if (service->state == TIDAL_CONNECT_STATE_PLAYING && service->session.current_track.duration_ms > 0) {
            pthread_mutex_lock(&service->mutex);
            service->session.position_ms += 1000;
            if (service->session.position_ms >= service->session.current_track.duration_ms) {
                // 播放结束，根据重复模式处理
                if (service->session.repeat_mode == 1) {
                    // 单曲重复
                    service->session.position_ms = 0;
                } else if (service->session.repeat_mode == 2) {
                    // 全部重复
                    tidal_connect_next_track(service);
                } else {
                    // 无重复，停止播放
                    tidal_connect_stop_playback(service);
                }
            }
            pthread_mutex_unlock(&service->mutex);
        }
    }

    // 清理CURL
    curl_global_cleanup();

    return NULL;
}

// 设置服务器套接字
static int tidal_connect_setup_server(TidalConnectService* service) {
    struct sockaddr_in server_addr;

    // 创建TCP套接字
    service->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (service->server_fd < 0) {
        fprintf(stderr, "Failed to create Tidal Connect socket: %s\n", strerror(errno));
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
        fprintf(stderr, "Failed to bind Tidal Connect socket: %s\n", strerror(errno));
        close(service->server_fd);
        service->server_fd = -1;
        return -1;
    }

    // 监听连接
    if (listen(service->server_fd, 5) < 0) {
        fprintf(stderr, "Failed to listen on Tidal Connect socket: %s\n", strerror(errno));
        close(service->server_fd);
        service->server_fd = -1;
        return -1;
    }

    return 0;
}

// 清理连接
static void tidal_connect_cleanup_connections(TidalConnectService* service) {
    if (service->client_fd >= 0) {
        close(service->client_fd);
        service->client_fd = -1;
    }

    // 重置会话信息
    memset(&service->session, 0, sizeof(TidalConnectSession));
    service->session.volume = service->config.initial_volume;
}

// Avahi回调函数
static void tidal_connect_avahi_callback(AvahiClient* c, AvahiClientState state, void* userdata) {
    TidalConnectService* service = (TidalConnectService*)userdata;

    if (!service->running) {
        return;
    }

    switch (state) {
        case AVAHI_CLIENT_S_RUNNING:
            // 客户端运行中，创建服务
            if (!service->avahi_group) {
                tidal_connect_create_avahi_service(service);
            }
            break;

        case AVAHI_CLIENT_S_COLLISION:
            // 发生冲突，需要重新创建服务
            if (service->avahi_group) {
                avahi_entry_group_reset(service->avahi_group);
                tidal_connect_create_avahi_service(service);
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
static void tidal_connect_create_avahi_service(TidalConnectService* service) {
    char service_name[256];
    AvahiStringList* txt = NULL;
    int ret;

    // 格式化服务名称
    snprintf(service_name, sizeof(service_name), "%s", service->config.friendly_name);

    // 创建TXT记录
    txt = avahi_string_list_add_printf(txt, "name=%s", service->config.friendly_name);
    txt = avahi_string_list_add_printf(txt, "id=%s", service->config.device_id);
    txt = avahi_string_list_add_printf(txt, "model=%s", "RealTimeAudio");
    txt = avahi_string_list_add_printf(txt, "version=1.0.0");

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
        "_tidalconnect._tcp",
        NULL,
        NULL,
        service->config.port,
        txt
    );

    // 释放TXT记录
    avahi_string_list_free(txt);

    if (ret < 0) {
        fprintf(stderr, "Failed to add Tidal Connect service: %s\n", avahi_strerror(ret));
        avahi_entry_group_reset(service->avahi_group);
        return;
    }

    // 提交服务组
    ret = avahi_entry_group_commit(service->avahi_group);
    if (ret < 0) {
        fprintf(stderr, "Failed to commit Tidal Connect service group: %s\n", avahi_strerror(ret));
        avahi_entry_group_reset(service->avahi_group);
        return;
    }

    printf("Tidal Connect service published as '%s' on port %d\n", service_name, service->config.port);
}

// 处理客户端连接
static int tidal_connect_handle_client(TidalConnectService* service) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int new_socket = accept(service->server_fd, (struct sockaddr*)&client_addr, &client_len);

    if (new_socket < 0) {
        fprintf(stderr, "Failed to accept Tidal Connect connection: %s\n", strerror(errno));
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
    service->session.client_port = ntohs(client_addr.sin_port);

    // 设置状态
    tidal_connect_set_state(service, TIDAL_CONNECT_STATE_CONNECTED);

    printf("Tidal Connect client connected from %s:%d\n", service->session.client_ip, service->session.client_port);

    // 启动认证流程
    tidal_connect_authenticate(service);

    pthread_mutex_unlock(&service->mutex);

    return 0;
}

// 认证函数
static int tidal_connect_authenticate(TidalConnectService* service) {
    if (!service || service->state != TIDAL_CONNECT_STATE_CONNECTED) {
        return -1;
    }

    tidal_connect_set_state(service, TIDAL_CONNECT_STATE_AUTHENTICATING);

    // 在实际实现中，应实现Tidal Connect认证流程
    // 这包括设备注册、用户授权和令牌获取

    // 模拟认证成功
    snprintf(service->session.session_id, sizeof(service->session.session_id), "TIDAL-%08X%08X", rand(), rand());
    snprintf(service->session.access_token, sizeof(service->session.access_token), "ACCESS-%016X%016X", rand(), rand());
    service->session.token_expiry = time(NULL) + 3600; // 1小时后过期
    snprintf(service->session.user_id, sizeof(service->session.user_id), "USER-%08X", rand());

    tidal_connect_set_state(service, TIDAL_CONNECT_STATE_AUTHENTICATED);

    return 0;
}

// 刷新令牌
static int tidal_connect_refresh_token(TidalConnectService* service) {
    if (!service || service->state < TIDAL_CONNECT_STATE_AUTHENTICATED) {
        return -1;
    }

    // 在实际实现中，应调用Tidal API刷新访问令牌

    // 模拟刷新成功
    snprintf(service->session.access_token, sizeof(service->session.access_token), "ACCESS-%016X%016X", rand(), rand());
    service->session.token_expiry = time(NULL) + 3600; // 再延长1小时

    return 0;
}

// CURL写入回调
static size_t tidal_connect_curl_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    char* buffer = (char*)userp;

    // 复制内容到缓冲区
    strncat(buffer, (char*)contents, realsize);

    return realsize;
}

// 设置状态
static void tidal_connect_set_state(TidalConnectService* service, TidalConnectState state) {
    if (!service) return;

    pthread_mutex_lock(&service->mutex);
    TidalConnectState old_state = service->state;
    service->state = state;
    pthread_mutex_unlock(&service->mutex);

    // Only emit signal if state actually changed
    if (old_state != state) {
        char client_ip[INET_ADDRSTRLEN] = "unknown";
        if (service->client_fd >= 0) {
            struct sockaddr_in addr;
            socklen_t addr_len = sizeof(addr);
            if (getpeername(service->client_fd, (struct sockaddr*)&addr, &addr_len) == 0) {
                inet_ntop(AF_INET, &addr.sin_addr, client_ip, sizeof(client_ip));
            }
        }

        // Create JSON payload
        json_t* details = json_object();
        json_object_set_new(details, "old_state", json_integer(old_state));
        json_object_set_new(details, "new_state", json_integer(state));
        json_object_set_new(details, "client_ip", json_string(client_ip));
        json_object_set_new(details, "timestamp", json_integer(time(NULL)));

        const char* json_str = json_dumps(details, JSON_COMPACT);
        if (json_str) {
            // Emit D-Bus signal
            dbus_emit_signal("com.realtimeaudio.TidalConnect", DBUS_SIGNAL_TYPE_STATE_CHANGED, json_str);
            free((void*)json_str);
        }
        json_decref(details);

        // Original logging
        printf("Tidal Connect state changed from %d to %d\n", old_state, state);
    }
    if (!service || service->state == state) return;

    pthread_mutex_lock(&service->mutex);
    service->state = state;
    pthread_mutex_unlock(&service->mutex);

    printf("Tidal Connect state changed to: %d\n", state);
}

// 设置音频流
static int tidal_connect_setup_audio_stream(TidalConnectService* service) {
    if (!service || !service->context) {
        return -1;
    }

    // 创建PipeWire流属性
    struct pw_properties* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_STREAM_NAME, "Tidal Connect Stream",
        NULL
    );

    if (!props) {
        fprintf(stderr, "Failed to create properties for Tidal Connect stream\n");
        return -1;
    }

    // 创建流
    service->stream = pw_stream_new_simple(
        service->context,
        "tidalconnect-stream",
        props,
        NULL
    );

    if (!service->stream) {
        fprintf(stderr, "Failed to create Tidal Connect stream\n");
        pw_properties_free(props);
        return -1;
    }

    return 0;
}

// 获取曲目信息
static int tidal_connect_fetch_track_info(TidalConnectService* service, const char* track_id) {
    if (!service || !track_id || service->state < TIDAL_CONNECT_STATE_AUTHENTICATED) {
        return -1;
    }

    // 在实际实现中，应调用Tidal API获取曲目详细信息
    // 这里仅做模拟
    memset(&service->session.current_track, 0, sizeof(TidalTrack));
    strncpy(service->session.current_track.track_id, track_id, sizeof(service->session.current_track.track_id) - 1);
    snprintf(service->session.current_track.title, sizeof(service->session.current_track.title), "Sample Track");
    snprintf(service->session.current_track.artist, sizeof(service->session.current_track.artist), "Sample Artist");
    snprintf(service->session.current_track.album, sizeof(service->session.current_track.album), "Sample Album");
    service->session.current_track.duration_ms = 240000; // 4分钟
    service->session.current_track.sample_rate = 44100;
    service->session.current_track.channels = 2;
    service->session.current_track.bit_depth = 16;
    strncpy(service->session.current_track.audio_format, "FLAC", sizeof(service->session.current_track.audio_format) - 1);

    return 0;
}

// 解析曲目响应
static int tidal_connect_parse_track_response(TidalConnectService* service, const char* response) {
    // 在实际实现中，应解析Tidal API返回的JSON响应
    return 0;
}