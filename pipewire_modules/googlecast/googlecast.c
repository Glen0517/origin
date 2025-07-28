#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <math.h>
#include <jansson.h>
#include <curl/curl.h>
#include <zlib.h>
#include <libsrtp/srtp.h>

#include "googlecast.h"
#include "../../audio_processing/audio_processing.h"
#include "../alsa/alsa_plugin.h"
#include "../flowdsp/flowdsp.h"

// 前向声明
static void* google_cast_worker_thread(void* data);
static int google_cast_setup_server(GoogleCastService* service);
static void google_cast_cleanup_connections(GoogleCastService* service);
static void google_cast_avahi_callback(AvahiClient* c, AvahiClientState state, void* userdata);
static void google_cast_create_avahi_service(GoogleCastService* service);
static int google_cast_handle_client(GoogleCastService* service);
static int google_cast_process_http_request(GoogleCastService* service, int client_fd, const char* request);
static int google_cast_setup_ssl(GoogleCastService* service);
static int google_cast_setup_srtp(GoogleCastService* service, const uint8_t* key, size_t key_len);
static void google_cast_set_state(GoogleCastService* service, GoogleCastState state);
static json_t* google_cast_create_media_status(GoogleCastService* service);
static int google_cast_send_message(GoogleCastService* service, const char* namespace, const char* message);
static void google_cast_parse_media_message(GoogleCastService* service, const char* message);
static size_t google_cast_curl_write_callback(void* contents, size_t size, size_t nmemb, void* userp);

// 创建Google Cast服务
GoogleCastService* google_cast_create(struct pw_context* context, const GoogleCastConfig* config) {
    if (!context || !config) {
        fprintf(stderr, "Invalid parameters for google_cast_create\n");
        return NULL;
    }

    GoogleCastService* service = calloc(1, sizeof(GoogleCastService));
    if (!service) {
        fprintf(stderr, "Failed to allocate memory for GoogleCastService\n");
        return NULL;
    }

    // 初始化互斥锁
    pthread_mutex_init(&service->mutex, NULL);

    // 设置上下文
    service->context = context;
    service->state = GOOGLE_CAST_STATE_DISCONNECTED;
    service->running = false;
    service->server_fd = -1;
    service->client_fd = -1;

    // 复制配置
    service->config = *config;
    if (service->config.port == 0) {
        service->config.port = 8009; // 默认端口
    }
    if (!service->config.device_name[0]) {
        snprintf(service->config.device_name, sizeof(service->config.device_name), "RealTimeAudioCast");
    }
    if (!service->config.friendly_name[0]) {
        snprintf(service->config.friendly_name, sizeof(service->config.friendly_name), "RealTime Audio Cast");
    }
    if (!service->config.uuid[0]) {
        // 生成UUID
        snprintf(service->config.uuid, sizeof(service->config.uuid), "%08x-%04x-%04x-%04x-%012x",
                 rand(), rand() % 0x10000, rand() % 0x10000,
                 rand() % 0x10000, (uint64_t)rand() << 40 | (uint64_t)rand() << 20 | rand());
    }
    if (!service->config.manufacturer[0]) {
        snprintf(service->config.manufacturer, sizeof(service->config.manufacturer), "RealTime Audio Framework");
    }
    if (!service->config.model_name[0]) {
        snprintf(service->config.model_name, sizeof(service->config.model_name), "RT-Cast Audio");
    }
    if (!service->config.firmware_version[0]) {
        snprintf(service->config.firmware_version, sizeof(service->config.firmware_version), "1.0.0");
    }
    service->config.initial_volume = fmaxf(0.0f, fminf(1.0f, service->config.initial_volume));
    service->session.volume = service->config.initial_volume;

    // 创建应用配置
    service->app_config = json_object();
    json_object_set_new(service->app_config, "name", json_string("Default Media Receiver"));
    json_object_set_new(service->app_config, "id", json_string("CC1AD845"));
    json_object_set_new(service->app_config, "iconUrl", json_string("http://localhost:8009/icon.png"));

    return service;
}

// 销毁Google Cast服务
void google_cast_destroy(GoogleCastService* service) {
    if (!service) return;

    google_cast_stop(service);

    // 清理SSL
    if (service->ssl) {
        SSL_shutdown(service->ssl);
        SSL_free(service->ssl);
    }
    if (service->ssl_ctx) {
        SSL_CTX_free(service->ssl_ctx);
    }

    // 清理SRTP
    if (service->srtp_initialized) {
        srtp_dealloc(&service->srtp_send_policy.ssrc.type);
        srtp_dealloc(&service->srtp_recv_policy.ssrc.type);
        srtp_shutdown();
    }

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

    // 清理JSON配置
    if (service->app_config) {
        json_decref(service->app_config);
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

// 启动Google Cast服务
int google_cast_start(GoogleCastService* service) {
    if (!service || service->running) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 设置运行标志
    service->running = true;

    // 创建服务器套接字
    if (google_cast_setup_server(service) < 0) {
        fprintf(stderr, "Failed to setup Google Cast server\n");
        service->running = false;
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 初始化SSL
    if (service->config.enable_encryption && google_cast_setup_ssl(service) < 0) {
        fprintf(stderr, "Failed to setup SSL\n");
        google_cast_cleanup_connections(service);
        service->running = false;
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 初始化Avahi
    service->avahi_poll = avahi_threaded_poll_new();
    if (!service->avahi_poll) {
        fprintf(stderr, "Failed to create Avahi threaded poll\n");
        google_cast_cleanup_connections(service);
        service->running = false;
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 创建Avahi客户端
    int error;
    service->avahi_client = avahi_client_new(avahi_threaded_poll_get(service->avahi_poll), AVAHI_CLIENT_NO_FAIL, google_cast_avahi_callback, service, &error);
    if (!service->avahi_client) {
        fprintf(stderr, "Failed to create Avahi client: %s\n", avahi_strerror(error));
        avahi_threaded_poll_free(service->avahi_poll);
        service->avahi_poll = NULL;
        google_cast_cleanup_connections(service);
        service->running = false;
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 启动Avahi线程池
    avahi_threaded_poll_start(service->avahi_poll);

    // 创建工作线程
    if (pthread_create(&service->thread, NULL, google_cast_worker_thread, service) != 0) {
        fprintf(stderr, "Failed to create Google Cast worker thread\n");
        avahi_threaded_poll_stop(service->avahi_poll);
        avahi_client_free(service->avahi_client);
        avahi_threaded_poll_free(service->avahi_poll);
        service->avahi_poll = NULL;
        service->avahi_client = NULL;
        google_cast_cleanup_connections(service);
        service->running = false;
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 设置状态为发现中
    google_cast_set_state(service, GOOGLE_CAST_STATE_DISCOVERING);

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 停止Google Cast服务
void google_cast_stop(GoogleCastService* service) {
    if (!service || !service->running) {
        return;
    }

    pthread_mutex_lock(&service->mutex);
    service->running = false;
    pthread_mutex_unlock(&service->mutex);

    // 等待线程结束
    pthread_join(service->thread, NULL);

    // 清理连接
    google_cast_cleanup_connections(service);

    // 设置状态为未连接
    google_cast_set_state(service, GOOGLE_CAST_STATE_DISCONNECTED);
}

// 加载媒体
int google_cast_load_media(GoogleCastService* service, const char* url, const char* mime_type) {
    if (!service || !url || !mime_type || service->state < GOOGLE_CAST_STATE_CONNECTED) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 设置状态为缓冲中
    google_cast_set_state(service, GOOGLE_CAST_STATE_BUFFERING);

    // 清空当前媒体信息
    memset(&service->session.media, 0, sizeof(GoogleCastMedia));

    // 设置媒体信息
    strncpy(service->session.media.content_id, url, sizeof(service->session.media.content_id) - 1);
    strncpy(service->session.media.mime_type, mime_type, sizeof(service->session.media.mime_type) - 1);
    strncpy(service->session.media.stream_type, "BUFFERED", sizeof(service->session.media.stream_type) - 1);

    // 创建会话ID
    snprintf(service->session.session_id, sizeof(service->session.session_id), "%08x", rand());
    snprintf(service->session.transport_id, sizeof(service->session.transport_id), "%08x", rand());

    // 解析MIME类型设置音频格式
    if (strstr(mime_type, "audio/flac")) {
        service->session.media.format.format = SPA_AUDIO_FORMAT_F32;
    } else if (strstr(mime_type, "audio/mpeg")) {
        service->session.media.format.format = SPA_AUDIO_FORMAT_S16;
    } else if (strstr(mime_type, "audio/aac")) {
        service->session.media.format.format = SPA_AUDIO_FORMAT_S16;
    } else {
        service->session.media.format.format = SPA_AUDIO_FORMAT_F32;
    }
    service->session.media.format.channels = 2;
    service->session.media.format.rate = 44100;

    // 创建PipeWire流
    if (!service->stream) {
        struct pw_properties* props = pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Playback",
            PW_KEY_MEDIA_ROLE, "Music",
            PW_KEY_STREAM_NAME, "Google Cast Stream",
            NULL
        );

        if (props) {
            service->stream = pw_stream_new_simple(
                service->context,
                "googlecast-stream",
                props,
                NULL
            );
        }
    }

    // 发送媒体加载消息
    json_t* media = json_object();
    json_object_set_new(media, "contentId", json_string(url));
    json_object_set_new(media, "streamType", json_string("BUFFERED"));
    json_object_set_new(media, " contentType", json_string(mime_type));

    json_t* request = json_object();
    json_object_set_new(request, "type", json_string("LOAD"));
    json_object_set_new(request, "media", media);
    json_object_set_new(request, "sessionId", json_string(service->session.session_id));
    json_object_set_new(request, "transportId", json_string(service->session.transport_id));

    char* request_str = json_dumps(request, JSON_COMPACT);
    int ret = google_cast_send_message(service, "urn:x-cast:com.google.cast.media", request_str);

    // 清理
    free(request_str);
    json_decref(request);

    // 设置状态为播放中
    google_cast_set_state(service, GOOGLE_CAST_STATE_PLAYING);

    pthread_mutex_unlock(&service->mutex);
    return ret;
}

// 播放媒体
int google_cast_play(GoogleCastService* service) {
    if (!service || service->state != GOOGLE_CAST_STATE_PAUSED) {
        return -1;
    }

    // 发送播放消息
    json_t* request = json_object();
    json_object_set_new(request, "type", json_string("PLAY"));
    json_object_set_new(request, "sessionId", json_string(service->session.session_id));
    json_object_set_new(request, "transportId", json_string(service->session.transport_id));

    char* request_str = json_dumps(request, JSON_COMPACT);
    int ret = google_cast_send_message(service, "urn:x-cast:com.google.cast.media", request_str);

    // 清理
    free(request_str);
    json_decref(request);

    // 更新状态
    google_cast_set_state(service, GOOGLE_CAST_STATE_PLAYING);

    return ret;
}

// 暂停播放
int google_cast_pause(GoogleCastService* service) {
    if (!service || service->state != GOOGLE_CAST_STATE_PLAYING) {
        return -1;
    }

    // 发送暂停消息
    json_t* request = json_object();
    json_object_set_new(request, "type", json_string("PAUSE"));
    json_object_set_new(request, "sessionId", json_string(service->session.session_id));
    json_object_set_new(request, "transportId", json_string(service->session.transport_id));

    char* request_str = json_dumps(request, JSON_COMPACT);
    int ret = google_cast_send_message(service, "urn:x-cast:com.google.cast.media", request_str);

    // 清理
    free(request_str);
    json_decref(request);

    // 更新状态
    google_cast_set_state(service, GOOGLE_CAST_STATE_PAUSED);

    return ret;
}

// 停止播放
int google_cast_stop_media(GoogleCastService* service) {
    if (!service || service->state < GOOGLE_CAST_STATE_PLAYING) {
        return -1;
    }

    // 发送停止消息
    json_t* request = json_object();
    json_object_set_new(request, "type", json_string("STOP"));
    json_object_set_new(request, "sessionId", json_string(service->session.session_id));
    json_object_set_new(request, "transportId", json_string(service->session.transport_id));

    char* request_str = json_dumps(request, JSON_COMPACT);
    int ret = google_cast_send_message(service, "urn:x-cast:com.google.cast.media", request_str);

    // 清理
    free(request_str);
    json_decref(request);

    // 清空媒体信息
    memset(&service->session.media, 0, sizeof(GoogleCastMedia));
    service->session.position_ms = 0;

    // 更新状态
    google_cast_set_state(service, GOOGLE_CAST_STATE_CONNECTED);

    return ret;
}

// 调整播放位置
int google_cast_seek(GoogleCastService* service, uint64_t position_ms) {
    if (!service || service->state < GOOGLE_CAST_STATE_PLAYING) {
        return -1;
    }

    // 发送 seek 消息
    json_t* request = json_object();
    json_object_set_new(request, "type", json_string("SEEK"));
    json_object_set_new(request, "sessionId", json_string(service->session.session_id));
    json_object_set_new(request, "transportId", json_string(service->session.transport_id));
    json_object_set_new(request, "currentTime", json_integer(position_ms / 1000));

    char* request_str = json_dumps(request, JSON_COMPACT);
    int ret = google_cast_send_message(service, "urn:x-cast:com.google.cast.media", request_str);

    // 清理
    free(request_str);
    json_decref(request);

    // 更新位置
    service->session.position_ms = position_ms;

    return ret;
}

// 设置音量
int google_cast_set_volume(GoogleCastService* service, float volume) {
    if (!service) {
        return -1;
    }

    volume = fmaxf(0.0f, fminf(1.0f, volume));

    pthread_mutex_lock(&service->mutex);
    service->session.volume = volume;
    pthread_mutex_unlock(&service->mutex);

    // 发送音量更新消息
    json_t* volume_obj = json_object();
    json_object_set_new(volume_obj, "level", json_real(volume));
    json_object_set_new(volume_obj, "muted", json_boolean(service->session.muted));

    json_t* request = json_object();
    json_object_set_new(request, "type", json_string("SET_VOLUME"));
    json_object_set_new(request, "volume", volume_obj);

    char* request_str = json_dumps(request, JSON_COMPACT);
    int ret = google_cast_send_message(service, "urn:x-cast:com.google.cast.receiver", request_str);

    // 清理
    free(request_str);
    json_decref(request);

    return ret;
}

// 设置静音
int google_cast_set_mute(GoogleCastService* service, bool muted) {
    if (!service) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);
    service->session.muted = muted;
    pthread_mutex_unlock(&service->mutex);

    // 发送静音更新消息
    json_t* volume_obj = json_object();
    json_object_set_new(volume_obj, "level", json_real(service->session.volume));
    json_object_set_new(volume_obj, "muted", json_boolean(muted));

    json_t* request = json_object();
    json_object_set_new(request, "type", json_string("SET_VOLUME"));
    json_object_set_new(request, "volume", volume_obj);

    char* request_str = json_dumps(request, JSON_COMPACT);
    int ret = google_cast_send_message(service, "urn:x-cast:com.google.cast.receiver", request_str);

    // 清理
    free(request_str);
    json_decref(request);

    return ret;
}

// 获取当前状态
GoogleCastState google_cast_get_state(GoogleCastService* service) {
    if (!service) {
        return GOOGLE_CAST_STATE_ERROR;
    }

    GoogleCastState state;
    pthread_mutex_lock(&service->mutex);
    state = service->state;
    pthread_mutex_unlock(&service->mutex);
    return state;
}

// 获取当前会话信息
const GoogleCastSession* google_cast_get_session(GoogleCastService* service) {
    if (!service || service->state < GOOGLE_CAST_STATE_CONNECTED) {
        return NULL;
    }

    return &service->session;
}

// 工作线程函数
static void* google_cast_worker_thread(void* data) {
    GoogleCastService* service = (GoogleCastService*)data;
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
            fprintf(stderr, "Google Cast select error: %s\n", strerror(errno));
            break;
        } else if (activity > 0) {
            // 服务器套接字有活动
            if (FD_ISSET(service->server_fd, &read_fds)) {
                google_cast_handle_client(service);
            }

            // 客户端套接字有活动
            if (service->client_fd >= 0 && FD_ISSET(service->client_fd, &read_fds)) {
                uint8_t buffer[8192];
                ssize_t bytes_read;

                // 读取数据
                if (service->config.enable_encryption && service->ssl) {
                    bytes_read = SSL_read(service->ssl, buffer, sizeof(buffer));
                } else {
                    bytes_read = recv(service->client_fd, buffer, sizeof(buffer), 0);
                }

                if (bytes_read <= 0) {
                    // 连接关闭或出错
                    fprintf(stderr, "Google Cast client disconnected\n");
                    google_cast_cleanup_connections(service);
                    google_cast_set_state(service, GOOGLE_CAST_STATE_DISCONNECTING);
                    google_cast_set_state(service, GOOGLE_CAST_STATE_DISCONNECTED);
                    google_cast_set_state(service, GOOGLE_CAST_STATE_DISCOVERING);
                } else {
                    // 处理SRTP解密
                    if (service->srtp_initialized) {
                        int decrypted_len = sizeof(buffer);
                        if (srtp_unprotect(&service->srtp_recv_policy, buffer, &bytes_read, buffer, &decrypted_len) != srtp_err_status_ok) {
                            fprintf(stderr, "SRTP unprotect failed\n");
                            continue;
                        }
                        bytes_read = decrypted_len;
                    }

                    // 检查是否是HTTP请求
                    if (strstr((char*)buffer, "HTTP/1.1") || strstr((char*)buffer, "GET ") || strstr((char*)buffer, "POST ")) {
                        google_cast_process_http_request(service, service->client_fd, (char*)buffer);
                    } else if (strstr((char*)buffer, "CAST-V2") || strstr((char*)buffer, "urn:x-cast:")) {
                        // 处理Cast协议消息
                        google_cast_parse_media_message(service, (char*)buffer);
                    } else if (service->state == GOOGLE_CAST_STATE_PLAYING) {
                        // 处理音频数据
                        if (service->stream) {
                            // 应用音量
                            float* audio_data = (float*)buffer;
                            int num_samples = bytes_read / sizeof(float);
                            for (int i = 0; i < num_samples; i++) {
                                audio_data[i] *= service->session.volume;
                                if (service->session.muted) {
                                    audio_data[i] = 0;
                                }
                            }

                            // 发送到PipeWire流
                            // 实际实现中应使用pw_stream_write
                        }
                    }
                }
            }
        }

        // 更新播放位置
        if (service->state == GOOGLE_CAST_STATE_PLAYING && service->session.media.duration_ms > 0) {
            pthread_mutex_lock(&service->mutex);
            service->session.position_ms += 1000; // 每秒钟增加1000毫秒
            if (service->session.position_ms >= service->session.media.duration_ms) {
                // 播放结束
                google_cast_stop_media(service);
            }
            pthread_mutex_unlock(&service->mutex);
        }
    }

    return NULL;
}

// 设置服务器套接字
static int google_cast_setup_server(GoogleCastService* service) {
    struct sockaddr_in server_addr;

    // 创建TCP套接字
    service->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (service->server_fd < 0) {
        fprintf(stderr, "Failed to create Google Cast socket: %s\n", strerror(errno));
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
        fprintf(stderr, "Failed to bind Google Cast socket: %s\n", strerror(errno));
        close(service->server_fd);
        service->server_fd = -1;
        return -1;
    }

    // 监听连接
    if (listen(service->server_fd, 5) < 0) {
        fprintf(stderr, "Failed to listen on Google Cast socket: %s\n", strerror(errno));
        close(service->server_fd);
        service->server_fd = -1;
        return -1;
    }

    return 0;
}

// 清理连接
static void google_cast_cleanup_connections(GoogleCastService* service) {
    if (service->client_fd >= 0) {
        if (service->ssl) {
            SSL_shutdown(service->ssl);
            SSL_free(service->ssl);
            service->ssl = NULL;
        }
        close(service->client_fd);
        service->client_fd = -1;
    }

    // 重置会话信息
    memset(&service->session, 0, sizeof(GoogleCastSession));
    service->session.volume = service->config.initial_volume;
}

// Avahi回调函数
static void google_cast_avahi_callback(AvahiClient* c, AvahiClientState state, void* userdata) {
    GoogleCastService* service = (GoogleCastService*)userdata;

    if (!service->running) {
        return;
    }

    switch (state) {
        case AVAHI_CLIENT_S_RUNNING:
            // 客户端运行中，创建服务
            if (!service->avahi_group) {
                google_cast_create_avahi_service(service);
            }
            break;

        case AVAHI_CLIENT_S_COLLISION:
            // 发生冲突，需要重新创建服务
            if (service->avahi_group) {
                avahi_entry_group_reset(service->avahi_group);
                google_cast_create_avahi_service(service);
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
static void google_cast_create_avahi_service(GoogleCastService* service) {
    char service_name[256];
    char port_str[8];
    AvahiStringList* txt = NULL;
    int ret;

    // 格式化服务名称
    snprintf(service_name, sizeof(service_name), "%s", service->config.friendly_name);

    // 格式化端口
    snprintf(port_str, sizeof(port_str), "%d", service->config.port);

    // 创建TXT记录
    txt = avahi_string_list_add_printf(txt, "id=%s", service->config.uuid);
    txt = avahi_string_list_add_printf(txt, "cd=1.0");
    txt = avahi_string_list_add_printf(txt, "fn=%s", service->config.friendly_name);
    txt = avahi_string_list_add_printf(txt, "md=%s", service->config.model_name);
    txt = avahi_string_list_add_printf(txt, "mf=%s", service->config.manufacturer);
    txt = avahi_string_list_add_printf(txt, "ve=%s", service->config.firmware_version);
    txt = avahi_string_list_add_printf(txt, "rs=Youtube,Netflix,Spotify");
    txt = avahi_string_list_add_printf(txt, "bs=FA8FCA26E5B9");
    txt = avahi_string_list_add_printf(txt, "st=0");
    txt = avahi_string_list_add_printf(txt, "ca=2004");

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
        "_googlecast._tcp",
        NULL,
        NULL,
        service->config.port,
        txt
    );

    // 释放TXT记录
    avahi_string_list_free(txt);

    if (ret < 0) {
        fprintf(stderr, "Failed to add Google Cast service: %s\n", avahi_strerror(ret));
        avahi_entry_group_reset(service->avahi_group);
        return;
    }

    // 提交服务组
    ret = avahi_entry_group_commit(service->avahi_group);
    if (ret < 0) {
        fprintf(stderr, "Failed to commit Google Cast service group: %s\n", avahi_strerror(ret));
        avahi_entry_group_reset(service->avahi_group);
        return;
    }

    printf("Google Cast service published as '%s' on port %d\n", service_name, service->config.port);
}

// 处理客户端连接
static int google_cast_handle_client(GoogleCastService* service) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int new_socket = accept(service->server_fd, (struct sockaddr*)&client_addr, &client_len);

    if (new_socket < 0) {
        fprintf(stderr, "Failed to accept Google Cast connection: %s\n", strerror(errno));
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 关闭现有连接
    if (service->client_fd >= 0) {
        if (service->ssl) {
            SSL_shutdown(service->ssl);
            SSL_free(service->ssl);
            service->ssl = NULL;
        }
        close(service->client_fd);
    }

    // 设置新客户端
    service->client_fd = new_socket;
    strncpy(service->session.client_ip, inet_ntoa(client_addr.sin_addr), sizeof(service->session.client_ip) - 1);
    service->session.client_port = ntohs(client_addr.sin_port);

    // 如果启用加密，建立SSL连接
    if (service->config.enable_encryption && service->ssl_ctx) {
        service->ssl = SSL_new(service->ssl_ctx);
        SSL_set_fd(service->ssl, service->client_fd);

        if (SSL_accept(service->ssl) <= 0) {
            fprintf(stderr, "SSL accept failed\n");
            ERR_print_errors_fp(stderr);
            close(service->client_fd);
            service->client_fd = -1;
            SSL_free(service->ssl);
            service->ssl = NULL;
            pthread_mutex_unlock(&service->mutex);
            return -1;
        }
    }

    // 设置状态
    google_cast_set_state(service, GOOGLE_CAST_STATE_CONNECTED);

    printf("Google Cast client connected from %s:%d\n", service->session.client_ip, service->session.client_port);

    pthread_mutex_unlock(&service->mutex);

    return 0;
}

// 处理HTTP请求
static int google_cast_process_http_request(GoogleCastService* service, int client_fd, const char* request) {
    if (!service || client_fd < 0 || !request) {
        return -1;
    }

    const char* response = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain\r\n"
                          "Connection: close\r\n"
                          "Access-Control-Allow-Origin: *\r\n\r\n"
                          "Google Cast Receiver Ready\r\n";

    const char* not_found = "HTTP/1.1 404 Not Found\r\n"
                           "Content-Type: text/plain\r\n"
                           "Connection: close\r\n\r\n"
                           "Resource not found\r\n";

    const char* xml_response = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: application/xml\r\n"
                              "Connection: close\r\n\r\n"
                              "<?xml version=\"1.0\"?><root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
                              "<specVersion><major>1</major><minor>0</minor></specVersion>"
                              "<URLBase>http://%s:%d</URLBase>"
                              "<device><deviceType>urn:schemas-upnp-org:device:MediaRenderer:1</deviceType>"
                              "<friendlyName>%s</friendlyName><manufacturer>%s</manufacturer>"
                              "<manufacturerURL>http://localhost</manufacturerURL>"
                              "<modelDescription>%s</modelDescription><modelName>%s</modelName>"
                              "<modelNumber>%s</modelNumber><modelURL>http://localhost</modelURL>"
                              "<serialNumber>%s</serialNumber><UDN>uuid:%s</UDN>"
                              "<UPC>123456789012</UPC><iconList><icon><mimetype>image/png</mimetype>"
                              "<width>48</width><height>48</height><depth>24</depth>"
                              "<url>/icon.png</url></icon></iconList></device></root>\r\n";

    char xml_buffer[2048];

    // 检查请求路径
    if (strstr(request, "/ssdp/device-desc.xml")) {
        // 生成XML响应
        snprintf(xml_buffer, sizeof(xml_buffer), xml_response,
                 service->session.client_ip, service->config.port,
                 service->config.friendly_name, service->config.manufacturer,
                 service->config.model_name, service->config.model_name,
                 service->config.firmware_version, service->config.uuid,
                 service->config.uuid);
        send(client_fd, xml_buffer, strlen(xml_buffer), 0);
    } else if (strstr(request, "/connection")) {
        // 处理连接请求
        send(client_fd, response, strlen(response), 0);
    } else if (strstr(request, "/cast")) {
        // 处理Cast请求
        send(client_fd, response, strlen(response), 0);
    } else {
        // 资源未找到
        send(client_fd, not_found, strlen(not_found), 0);
    }

    return 0;
}

// 设置SSL
static int google_cast_setup_ssl(GoogleCastService* service) {
    // 初始化OpenSSL
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    // 创建SSL上下文
    service->ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!service->ssl_ctx) {
        fprintf(stderr, "Failed to create SSL context\n");
        ERR_print_errors_fp(stderr);
        return -1;
    }

    // 设置证书和私钥
    if (service->config.server_cert[0] && service->config.private_key[0]) {
        // 使用提供的证书
        if (SSL_CTX_use_certificate_file(service->ssl_ctx, service->config.server_cert, SSL_FILETYPE_PEM) <= 0) {
            fprintf(stderr, "Failed to load server certificate\n");
            ERR_print_errors_fp(stderr);
            SSL_CTX_free(service->ssl_ctx);
            service->ssl_ctx = NULL;
            return -1;
        }

        if (SSL_CTX_use_PrivateKey_file(service->ssl_ctx, service->config.private_key, SSL_FILETYPE_PEM) <= 0) {
            fprintf(stderr, "Failed to load private key\n");
            ERR_print_errors_fp(stderr);
            SSL_CTX_free(service->ssl_ctx);
            service->ssl_ctx = NULL;
            return -1;
        }

        // 验证私钥
        if (!SSL_CTX_check_private_key(service->ssl_ctx)) {
            fprintf(stderr, "Private key does not match the public certificate\n");
            SSL_CTX_free(service->ssl_ctx);
            service->ssl_ctx = NULL;
            return -1;
        }
    } else {
        // 生成自签名证书
        fprintf(stderr, "Generating self-signed certificate...\n");
        // 实际实现中应生成自签名证书
    }

    return 0;
}

// 设置SRTP
static int google_cast_setup_srtp(GoogleCastService* service, const uint8_t* key, size_t key_len) {
    if (!service || !key || key_len < 16) {
        return -1;
    }

    // 初始化SRTP
    if (srtp_init() != srtp_err_status_ok) {
        fprintf(stderr, "SRTP initialization failed\n");
        return -1;
    }

    // 配置发送策略
    memset(&service->srtp_send_policy, 0, sizeof(srtp_policy_t));
    service->srtp_send_policy.ssrc.type = ssrc_any_inbound;
    service->srtp_send_policy.key = key;
    service->srtp_send_policy.cipher = SRTP_AES_128_CM_HMAC_SHA1_80;
    service->srtp_send_policy.key_len = 16;
    service->srtp_send_policy.auth_tag_len = 10;

    // 配置接收策略
    memset(&service->srtp_recv_policy, 0, sizeof(srtp_policy_t));
    service->srtp_recv_policy.ssrc.type = ssrc_any_outbound;
    service->srtp_recv_policy.key = key;
    service->srtp_recv_policy.cipher = SRTP_AES_128_CM_HMAC_SHA1_80;
    service->srtp_recv_policy.key_len = 16;
    service->srtp_recv_policy.auth_tag_len = 10;

    // 应用策略
    if (srtp_create(&service->srtp_send_policy) != srtp_err_status_ok ||
        srtp_create(&service->srtp_recv_policy) != srtp_err_status_ok) {
        fprintf(stderr, "SRTP policy creation failed\n");
        return -1;
    }

    service->srtp_initialized = true;
    return 0;
}

// 设置状态
static void google_cast_set_state(GoogleCastService* service, GoogleCastState state) {
    if (!service || service->state == state) return;

    GoogleCastState old_state = service->state;
    char details[512];
    snprintf(details, sizeof(details), "{\"event\":\"connection_state_changed\",\"old_state\":%d,\"new_state\":%d,\"client_ip\":\"%s\",\"timestamp\":%lld}",
             old_state, state, service->session.client_ip, (long long)time(NULL));

    pthread_mutex_lock(&service->mutex);
    service->state = state;
    pthread_mutex_unlock(&service->mutex);

    dbus_emit_signal("GoogleCast", DBUS_SIGNAL_CONNECTION_STATE_CHANGED, details);
    printf("Google Cast state changed from %d to %d\n", old_state, state);
}

// 创建媒体状态消息
static json_t* google_cast_create_media_status(GoogleCastService* service) {
    json_t* status = json_object();
    json_object_set_new(status, "type", json_string("MEDIA_STATUS"));
    json_object_set_new(status, "status", json_array());

    json_t* media_status = json_object();
    json_object_set_new(media_status, "mediaSessionId", json_string(service->session.session_id));
    json_object_set_new(media_status, "playbackRate", json_real(1.0));
    json_object_set_new(media_status, "playerState", json_string(
        service->state == GOOGLE_CAST_STATE_PLAYING ? "PLAYING" :
        service->state == GOOGLE_CAST_STATE_PAUSED ? "PAUSED" :
        service->state == GOOGLE_CAST_STATE_BUFFERING ? "BUFFERING" : "IDLE"
    ));
    json_object_set_new(media_status, "currentTime", json_real(service->session.position_ms / 1000.0));
    json_object_set_new(media_status, "supportedMediaCommands", json_integer(0xFFFFFFFF));
    json_object_set_new(media_status, "volume", json_pack("{s:f,s:b}", "level", service->session.volume, "muted", service->session.muted));

    json_t* media = json_object();
    json_object_set_new(media, "contentId", json_string(service->session.media.content_id));
    json_object_set_new(media, "streamType", json_string(service->session.media.stream_type));
    json_object_set_new(media, " contentType", json_string(service->session.media.mime_type));
    json_object_set_new(media, "duration", json_real(service->session.media.duration_ms / 1000.0));
    json_object_set_new(media_status, "media", media);

    json_array_append_new(json_object_get(status, "status"), media_status);

    return status;
}

// 发送消息
static int google_cast_send_message(GoogleCastService* service, const char* namespace, const char* message) {
    if (!service || !namespace || !message || service->client_fd < 0) {
        return -1;
    }

    char cast_message[2048];
size_t msg_len = snprintf(cast_message, sizeof(cast_message),
    "%s\n""Content-Type: application/json\n""Content-Length: %zu\n\n""%s",
    "POST /v2/sessions/%s/send HTTP/1.1",
    strlen(message), message, service->session.session_id);

    if (msg_len >= sizeof(cast_message)) {
        fprintf(stderr, "Google Cast message too long\n");
        return -1;
    }

    // 应用SRTP加密
    if (service->srtp_initialized) {
        uint8_t encrypted_buffer[2048];
        int encrypted_len = sizeof(encrypted_buffer);
        if (srtp_protect(&service->srtp_send_policy, (uint8_t*)cast_message, msg_len, encrypted_buffer, &encrypted_len) != srtp_err_status_ok) {
            fprintf(stderr, "SRTP protect failed\n");
            return -1;
        }
        msg_len = encrypted_len;
        memcpy(cast_message, encrypted_buffer, msg_len);
    }

    // 发送消息
    if (service->ssl) {
        return SSL_write(service->ssl, cast_message, msg_len);
    } else {
        return send(service->client_fd, cast_message, msg_len, 0);
    }
}

// 解析媒体消息
static void google_cast_parse_media_message(GoogleCastService* service, const char* message) {
    if (!service || !message) return;

    json_error_t error;
    json_t* root = json_loads(message, 0, &error);
    if (!root) {
        fprintf(stderr, "JSON parse error: %s at line %d\n", error.text, error.line);
        return;
    }

    // 处理不同类型的消息
    const char* type = json_string_value(json_object_get(root, "type"));
    if (type) {
        if (strcmp(type, "PLAY") == 0) {
            google_cast_play(service);
        } else if (strcmp(type, "PAUSE") == 0) {
            google_cast_pause(service);
        } else if (strcmp(type, "STOP") == 0) {
            google_cast_stop_media(service);
        } else if (strcmp(type, "SEEK") == 0) {
            json_t* time = json_object_get(root, "currentTime");
            if (time) {
                google_cast_seek(service, (uint64_t)(json_number_value(time) * 1000));
            }
        } else if (strcmp(type, "SET_VOLUME") == 0) {
            json_t* volume = json_object_get(root, "volume");
            if (volume) {
                json_t* level = json_object_get(volume, "level");
                json_t* muted = json_object_get(volume, "muted");
                if (level) {
                    google_cast_set_volume(service, json_number_value(level));
                }
                if (muted) {
                    google_cast_set_mute(service, json_boolean_value(muted));
                }
            }
        } else if (strcmp(type, "LOAD") == 0) {
            json_t* media = json_object_get(root, "media");
            if (media) {
                const char* content_id = json_string_value(json_object_get(media, "contentId"));
                const char* mime_type = json_string_value(json_object_get(media, " contentType"));
                if (content_id && mime_type) {
                    google_cast_load_media(service, content_id, mime_type);
                }
            }
        }
    }

    json_decref(root);
}

// CURL写入回调
static size_t google_cast_curl_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    char* buffer = (char*)userp;

    // 复制内容到缓冲区
    strncat(buffer, (char*)contents, realsize);

    return realsize;
}