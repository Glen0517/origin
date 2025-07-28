#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <math.h>
#include <curl/curl.h>
#include <jansson.h>

#include "spotify_connect.h"
#include "../../include/dbus_utils.h"
#include "../../audio_processing/audio_processing.h"
#include "../alsa/alsa_plugin.h"

// 前向声明
static void spotify_session_callbacks_init(sp_session_callbacks* callbacks);
static void spotify_audio_callbacks_init(sp_audio_callbacks* callbacks);
static void* spotify_worker_thread(void* data);
static void spotify_process_events(SpotifyConnectService* service);
static int spotify_authenticate(SpotifyConnectService* service);
static int spotify_refresh_token(SpotifyConnectService* service);
static size_t spotify_write_callback(void* contents, size_t size, size_t nmemb, void* userp);
static void spotify_update_track_info(SpotifyConnectService* service, sp_track* track);
static void spotify_set_state(SpotifyConnectService* service, SpotifyState state);
static void spotify_logout(SpotifyConnectService* service);

// 创建Spotify Connect服务
SpotifyConnectService* spotify_connect_create(struct pw_context* context, const SpotifyConfig* config) {
    if (!context || !config || !config->client_id[0]) {
        fprintf(stderr, "Invalid parameters for spotify_connect_create\n");
        return NULL;
    }

    SpotifyConnectService* service = calloc(1, sizeof(SpotifyConnectService));
    if (!service) {
        fprintf(stderr, "Failed to allocate memory for SpotifyConnectService\n");
        return NULL;
    }

    // 初始化互斥锁
    pthread_mutex_init(&service->mutex, NULL);

    // 设置上下文
    service->context = context;
    service->state = SPOTIFY_STATE_DISCONNECTED;
    service->running = false;

    // 复制配置
    service->config = *config;
    if (service->config.initial_volume < 0 || service->config.initial_volume > 1.0f) {
        service->config.initial_volume = 0.7f; // 默认音量
    }
    service->session.volume = service->config.initial_volume;

    // 设置默认路径
    if (!service->config.cache_path[0]) {
        snprintf(service->config.cache_path, sizeof(service->config.cache_path), "/tmp/spotify_cache");
    }
    if (!service->config.settings_path[0]) {
        snprintf(service->config.settings_path, sizeof(service->config.settings_path), "/tmp/spotify_settings");
    }

    // 创建缓存和设置目录
    char mkdir_cmd[512];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s %s", service->config.cache_path, service->config.settings_path);
    system(mkdir_cmd);

    return service;
}

// 销毁Spotify Connect服务
void spotify_connect_destroy(SpotifyConnectService* service) {
    if (!service) return;

    spotify_connect_stop(service);
    spotify_logout(service);

    // 清理PipeWire资源
    if (service->stream) {
        pw_stream_destroy(service->stream);
        service->stream = NULL;
    }

    // 清理互斥锁
    pthread_mutex_destroy(&service->mutex);

    free(service);
}

// 启动Spotify Connect服务
int spotify_connect_start(SpotifyConnectService* service) {
    if (!service || service->running) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 设置运行标志
    service->running = true;

    // 初始化D-Bus连接
    if (!dbus_initialize("SpotifyConnect")) {
        fprintf(stderr, "Failed to initialize D-Bus for Spotify Connect\n");
        service->running = false;
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 创建工作线程
    if (pthread_create(&service->thread, NULL, spotify_worker_thread, service) != 0) {
        fprintf(stderr, "Failed to create Spotify worker thread\n");
        service->running = false;
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 停止Spotify Connect服务
void spotify_connect_stop(SpotifyConnectService* service) {
    if (!service || !service->running) {
        return;
    }

    pthread_mutex_lock(&service->mutex);
    service->running = false;
    pthread_mutex_unlock(&service->mutex);

    // 等待线程结束
    pthread_join(service->thread, NULL);
}

// 播放Spotify曲目
int spotify_connect_play(SpotifyConnectService* service, const char* uri) {
    if (!service || !uri || service->state == SPOTIFY_STATE_ERROR) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    if (!service->spotify_session) {
        // 会话未初始化，先认证
        if (spotify_authenticate(service) < 0) {
            pthread_mutex_unlock(&service->mutex);
            return -1;
        }
    }

    // 如果提供了URI，先加载曲目
    if (uri && uri[0]) {
        sp_link* link = sp_link_create_from_string(uri);
        if (!link) {
            fprintf(stderr, "Invalid Spotify URI: %s\n", uri);
            pthread_mutex_unlock(&service->mutex);
            return -1;
        }

        if (sp_link_type(link) == SP_LINKTYPE_TRACK) {
            sp_track* track = sp_link_as_track(link);
            if (track && !sp_track_is_loaded(track)) {
                sp_session_preload(service->spotify_session, track);
                // 等待曲目加载
                int timeout = 0;
                while (!sp_track_is_loaded(track) && timeout < 100) {
                    usleep(10000);
                    timeout++;
                }
            }

            if (sp_track_is_loaded(track) && sp_track_is_playable(track)) {
                sp_error error = sp_session_player_load(service->spotify_session, track);
                if (error != SP_ERROR_OK) {
                    fprintf(stderr, "Failed to load track: %s\n", sp_error_message(error));
                    sp_link_release(link);
                    pthread_mutex_unlock(&service->mutex);
                    return -1;
                }
                spotify_update_track_info(service, track);
            } else {
                fprintf(stderr, "Track not playable or failed to load\n");
                sp_link_release(link);
                pthread_mutex_unlock(&service->mutex);
                return -1;
            }
            sp_link_release(link);
        } else {
            fprintf(stderr, "URI is not a track\n");
            pthread_mutex_unlock(&service->mutex);
            return -1;
        }
    }

    // 开始播放
    sp_error error = sp_session_player_play(service->spotify_session, true);
    if (error != SP_ERROR_OK) {
        fprintf(stderr, "Failed to start playback: %s\n", sp_error_message(error));
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    spotify_set_state(service, SPOTIFY_STATE_PLAYING);
    pthread_mutex_unlock(&service->mutex);

    return 0;
}

// 暂停播放
int spotify_connect_pause(SpotifyConnectService* service) {
    if (!service || service->state != SPOTIFY_STATE_PLAYING || !service->spotify_session) {
        return -1;
    }

    sp_error error = sp_session_player_play(service->spotify_session, false);
    if (error != SP_ERROR_OK) {
        fprintf(stderr, "Failed to pause playback: %s\n", sp_error_message(error));
        return -1;
    }

    spotify_set_state(service, SPOTIFY_STATE_PAUSED);
    return 0;
}

// 恢复播放
int spotify_connect_resume(SpotifyConnectService* service) {
    if (!service || service->state != SPOTIFY_STATE_PAUSED || !service->spotify_session) {
        return -1;
    }

    sp_error error = sp_session_player_play(service->spotify_session, true);
    if (error != SP_ERROR_OK) {
        fprintf(stderr, "Failed to resume playback: %s\n", sp_error_message(error));
        return -1;
    }

    spotify_set_state(service, SPOTIFY_STATE_PLAYING);
    return 0;
}

// 跳过到下一曲目
int spotify_connect_next(SpotifyConnectService* service) {
    if (!service || service->state < SPOTIFY_STATE_CONNECTED || !service->spotify_session) {
        return -1;
    }

    sp_error error = sp_session_player_next(service->spotify_session);
    if (error != SP_ERROR_OK) {
        fprintf(stderr, "Failed to skip to next track: %s\n", sp_error_message(error));
        return -1;
    }

    spotify_set_state(service, SPOTIFY_STATE_LOADING);
    return 0;
}

// 返回到上一曲目
int spotify_connect_prev(SpotifyConnectService* service) {
    if (!service || service->state < SPOTIFY_STATE_CONNECTED || !service->spotify_session) {
        return -1;
    }

    sp_error error = sp_session_player_prev(service->spotify_session);
    if (error != SP_ERROR_OK) {
        fprintf(stderr, "Failed to return to previous track: %s\n", sp_error_message(error));
        return -1;
    }

    spotify_set_state(service, SPOTIFY_STATE_LOADING);
    return 0;
}

// 设置音量
int spotify_connect_set_volume(SpotifyConnectService* service, float volume) {
    if (!service) {
        return -1;
    }

    volume = fmaxf(0.0f, fminf(1.0f, volume));

    pthread_mutex_lock(&service->mutex);
    service->session.volume = volume;
    pthread_mutex_unlock(&service->mutex);

    if (service->spotify_session) {
        sp_session_set_volume_normalization(service->spotify_session, service->config.enable_audio_normalization ? volume : 0.0f);
    }

    return 0;
}

// 设置播放位置
int spotify_connect_seek(SpotifyConnectService* service, uint64_t position_ms) {
    if (!service || service->state < SPOTIFY_STATE_PLAYING || !service->spotify_session) {
        return -1;
    }

    sp_error error = sp_session_player_seek(service->spotify_session, position_ms);
    if (error != SP_ERROR_OK) {
        fprintf(stderr, "Failed to seek: %s\n", sp_error_message(error));
        return -1;
    }

    service->session.position_ms = position_ms;
    return 0;
}

// 设置随机播放
int spotify_connect_set_shuffle(SpotifyConnectService* service, bool shuffle) {
    if (!service || service->state < SPOTIFY_STATE_CONNECTED || !service->spotify_session) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);
    service->session.shuffle = shuffle;
    sp_session_player_enable_shuffle(service->spotify_session, shuffle);
    pthread_mutex_unlock(&service->mutex);

    return 0;
}

// 设置重复播放
int spotify_connect_set_repeat(SpotifyConnectService* service, bool repeat) {
    if (!service || service->state < SPOTIFY_STATE_CONNECTED || !service->spotify_session) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);
    service->session.repeat = repeat;
    sp_session_player_enable_repeat(service->spotify_session, repeat);
    pthread_mutex_unlock(&service->mutex);

    return 0;
}

// 获取当前状态
SpotifyState spotify_connect_get_state(SpotifyConnectService* service) {
    if (!service) {
        return SPOTIFY_STATE_ERROR;
    }

    SpotifyState state;
    pthread_mutex_lock(&service->mutex);
    state = service->state;
    pthread_mutex_unlock(&service->mutex);
    return state;
}

// 获取当前会话信息
const SpotifySession* spotify_connect_get_session(SpotifyConnectService* service) {
    if (!service || service->state < SPOTIFY_STATE_CONNECTED) {
        return NULL;
    }

    return &service->session;
}

// 工作线程函数
static void* spotify_worker_thread(void* data) {
    SpotifyConnectService* service = (SpotifyConnectService*)data;

    // 初始化libspotify
    sp_session_config config;
    memset(&config, 0, sizeof(config));

    config.api_version = SPOTIFY_API_VERSION;
    config.cache_location = service->config.cache_path;
    config.settings_location = service->config.settings_path;
    config.application_key = g_appkey;
    config.application_key_size = sizeof(g_appkey);
    config.user_agent = "RealTimeAudioFramework/1.0";

    // 设置回调
    sp_session_callbacks session_callbacks;
    spotify_session_callbacks_init(&session_callbacks);
    config.callbacks = &session_callbacks;
    config.userdata = service;

    // 创建音频驱动
    sp_audio_driver_callbacks audio_callbacks;
    spotify_audio_callbacks_init(&audio_callbacks);
    service->audio_driver = sp_audio_driver_create(&audio_callbacks, service);

    // 创建会话
    sp_error error = sp_session_create(&config, &service->spotify_session);
    if (error != SP_ERROR_OK) {
        fprintf(stderr, "Failed to create spotify session: %s\n", sp_error_message(error));
        service->state = SPOTIFY_STATE_ERROR;
        return NULL;
    }

    // 进行认证
    if (spotify_authenticate(service) < 0) {
        fprintf(stderr, "Spotify authentication failed\n");
        service->state = SPOTIFY_STATE_ERROR;
        return NULL;
    }

    // 主循环
    while (1) {
        pthread_mutex_lock(&service->mutex);
        bool running = service->running;
        pthread_mutex_unlock(&service->mutex);

        if (!running) {
            break;
        }

        // 处理事件
        spotify_process_events(service);

        // 更新播放位置
        if (service->state == SPOTIFY_STATE_PLAYING && service->spotify_session) {
            pthread_mutex_lock(&service->mutex);
            service->session.position_ms = sp_session_player_get_position(service->spotify_session);
            pthread_mutex_unlock(&service->mutex);
        }

        // 检查令牌过期
        time_t now = time(NULL);
        if (service->token_expires > 0 && now > service->token_expires - 60) {
            spotify_refresh_token(service);
        }

        // 等待下一个事件或超时
        usleep(service->next_timeout > 0 ? service->next_timeout * 1000 : 100000);
    }

    return NULL;
}

// 处理Spotify事件
static void spotify_process_events(SpotifyConnectService* service) {
    if (!service || !service->spotify_session) return;

    sp_session_process_events(service->spotify_session, &service->next_timeout);
}

// 会话回调初始化
static void spotify_session_callbacks_init(sp_session_callbacks* callbacks) {
    memset(callbacks, 0, sizeof(sp_session_callbacks));
    callbacks->logged_in = spotify_logged_in_callback;
    callbacks->logged_out = spotify_logged_out_callback;
    callbacks->metadata_updated = spotify_metadata_updated_callback;
    callbacks->connection_error = spotify_connection_error_callback;
    callbacks->message_to_user = spotify_message_to_user_callback;
    callbacks->notify_main_thread = spotify_notify_main_thread_callback;
    callbacks->music_delivery = spotify_music_delivery_callback;
    callbacks->play_token_lost = spotify_play_token_lost_callback;
    callbacks->track_end = spotify_track_end_callback;
    callbacks->track_started = spotify_track_started_callback;
    callbacks->streaming_error = spotify_streaming_error_callback;
    callbacks->userinfo_updated = spotify_userinfo_updated_callback;
}

// 音频回调初始化
static void spotify_audio_callbacks_init(sp_audio_callbacks* callbacks) {
    memset(callbacks, 0, sizeof(sp_audio_callbacks));
    callbacks->write = spotify_audio_write_callback;
    callbacks->flush = spotify_audio_flush_callback;
    callbacks->close = spotify_audio_close_callback;
}

// 认证函数
static int spotify_authenticate(SpotifyConnectService* service) {
    if (!service) return -1;

    spotify_set_state(service, SPOTIFY_STATE_CONNECTING);

    // 如果令牌未过期，直接使用
    time_t now = time(NULL);
    if (service->access_token[0] && service->token_expires > now + 60) {
        sp_error error = sp_session_renew_session(service->spotify_session);
        if (error == SP_ERROR_OK) {
            spotify_set_state(service, SPOTIFY_STATE_CONNECTED);
            return 0;
        }
    }

    // 否则进行OAuth认证
    CURL* curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize curl\n");
        return -1;
    }

    // 设置POST数据
    char post_data[512];
    snprintf(post_data, sizeof(post_data),
             "grant_type=client_credentials&client_id=%s&client_secret=%s",
             service->config.client_id, service->config.client_secret);

    // 设置curl选项
    curl_easy_setopt(curl, CURLOPT_URL, "https://accounts.spotify.com/api/token");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, spotify_write_callback);

    // 存储响应
    char response[1024] = {0};
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

    // 执行请求
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return -1;
    }

    // 解析JSON响应
    json_t* root = json_loads(response, 0, NULL);
    if (!root) {
        fprintf(stderr, "Failed to parse Spotify auth response\n");
        curl_easy_cleanup(curl);
        return -1;
    }

    // 提取访问令牌和过期时间
    json_t* access_token = json_object_get(root, "access_token");
    json_t* expires_in = json_object_get(root, "expires_in");

    if (json_is_string(access_token) && json_is_integer(expires_in)) {
        strncpy(service->access_token, json_string_value(access_token), sizeof(service->access_token) - 1);
        service->token_expires = now + json_integer_value(expires_in);

        // 使用令牌登录
        sp_error error = sp_session_login_with_token(service->spotify_session, service->access_token, 0);
        if (error != SP_ERROR_OK) {
            fprintf(stderr, "Failed to login with token: %s\n", sp_error_message(error));
            json_decref(root);
            curl_easy_cleanup(curl);
            return -1;
        }
    } else {
        fprintf(stderr, "Spotify auth response missing required fields\n");
        json_decref(root);
        curl_easy_cleanup(curl);
        return -1;
    }

    // 清理
    json_decref(root);
    curl_easy_cleanup(curl);

    return 0;
}

// 刷新令牌
static int spotify_refresh_token(SpotifyConnectService* service) {
    if (!service || !service->refresh_token[0]) {
        return spotify_authenticate(service);
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize curl\n");
        return -1;
    }

    // 设置POST数据
    char post_data[512];
    snprintf(post_data, sizeof(post_data),
             "grant_type=refresh_token&refresh_token=%s&client_id=%s&client_secret=%s",
             service->refresh_token, service->config.client_id, service->config.client_secret);

    // 设置curl选项
    curl_easy_setopt(curl, CURLOPT_URL, "https://accounts.spotify.com/api/token");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, spotify_write_callback);

    // 存储响应
    char response[1024] = {0};
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

    // 执行请求
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return -1;
    }

    // 解析JSON响应
    json_t* root = json_loads(response, 0, NULL);
    if (!root) {
        fprintf(stderr, "Failed to parse Spotify refresh response\n");
        curl_easy_cleanup(curl);
        return -1;
    }

    // 提取访问令牌和过期时间
    json_t* access_token = json_object_get(root, "access_token");
    json_t* expires_in = json_object_get(root, "expires_in");

    if (json_is_string(access_token) && json_is_integer(expires_in)) {
        strncpy(service->access_token, json_string_value(access_token), sizeof(service->access_token) - 1);
        service->token_expires = time(NULL) + json_integer_value(expires_in);

        // 更新会话令牌
        sp_error error = sp_session_renew_session(service->spotify_session);
        if (error != SP_ERROR_OK) {
            fprintf(stderr, "Failed to renew session: %s\n", sp_error_message(error));
        }
    } else {
        fprintf(stderr, "Spotify refresh response missing required fields\n");
        json_decref(root);
        curl_easy_cleanup(curl);
        return -1;
    }

    // 清理
    json_decref(root);
    curl_easy_cleanup(curl);

    return 0;
}

// CURL写入回调
static size_t spotify_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    char* buffer = (char*)userp;

    // 复制内容到缓冲区
    strncat(buffer, (char*)contents, realsize);

    return realsize;
}

// 更新曲目信息
static void spotify_update_track_info(SpotifyConnectService* service, sp_track* track) {
    if (!service || !track || !sp_track_is_loaded(track)) return;

    SpotifyTrack* current_track = &service->session.current_track;

    // 清空当前曲目信息
    memset(current_track, 0, sizeof(SpotifyTrack));

    // 设置URI
    sp_link* link = sp_link_create_from_track(track, 0);
    if (link) {
        sp_link_as_string(link, current_track->uri, sizeof(current_track->uri));
        sp_link_release(link);
    }

    // 设置标题
    const char* title = sp_track_name(track);
    if (title) {
        strncpy(current_track->title, title, sizeof(current_track->title) - 1);
    }

    // 设置艺术家
    int num_artists = sp_track_num_artists(track);
    for (int i = 0; i < num_artists; i++) {
        sp_artist* artist = sp_track_artist(track, i);
        if (artist) {
            if (i > 0) strncat(current_track->artist, ", ", sizeof(current_track->artist) - strlen(current_track->artist) - 1);
            strncat(current_track->artist, sp_artist_name(artist), sizeof(current_track->artist) - strlen(current_track->artist) - 1);
        }
    }

    // 设置专辑
    sp_album* album = sp_track_album(track);
    if (album && sp_album_is_loaded(album)) {
        strncpy(current_track->album, sp_album_name(album), sizeof(current_track->album) - 1);
    }

    // 设置时长
    current_track->duration_ms = sp_track_duration(track);

    // 设置曲目编号
    current_track->track_number = sp_track_index(track);

    // 设置可播放状态
    current_track->is_playable = sp_track_is_playable(track);

    // 更新播放队列长度
    service->session.play_queue_length = sp_session_player_num_tracks_in_queue(service->spotify_session);
}

// 设置状态
static void spotify_set_state(SpotifyConnectService* service, SpotifyState state) {
    if (!service || service->state == state) return;

    SpotifyState old_state = service->state;
    char details[512];
    snprintf(details, sizeof(details), "{\"event\":\"connection_state_changed\",\"old_state\":%d,\"new_state\":%d,\"client_ip\":\"%s\",\"timestamp\":%lld}",
             old_state, state, service->session.client_ip, (long long)time(NULL));

    pthread_mutex_lock(&service->mutex);
    service->state = state;
    pthread_mutex_unlock(&service->mutex);

    dbus_emit_signal("SpotifyConnect", DBUS_SIGNAL_CONNECTION_STATE_CHANGED, details);
    printf("Spotify state changed from %d to %d\n", old_state, state);
}

// 登出函数
static void spotify_logout(SpotifyConnectService* service) {
    if (!service || !service->spotify_session) return;

    sp_session_logout(service->spotify_session);
    service->access_token[0] = '\0';
    service->refresh_token[0] = '\0';
    service->token_expires = 0;
}

// 会话回调函数实现
// 这些回调函数需要根据libspotify文档实现，此处省略具体实现
void spotify_logged_in_callback(sp_session* session, sp_error error) {}
void spotify_logged_out_callback(sp_session* session) {}
void spotify_metadata_updated_callback(sp_session* session) {}
void spotify_connection_error_callback(sp_session* session, sp_error error) {}
void spotify_message_to_user_callback(sp_session* session, const char* message) {}
void spotify_notify_main_thread_callback(sp_session* session) {}
int spotify_music_delivery_callback(sp_session* session, const sp_audioformat* format, const void* frames, int num_frames) { return num_frames; }
void spotify_play_token_lost_callback(sp_session* session) {}
void spotify_track_end_callback(sp_session* session) {}
void spotify_track_started_callback(sp_session* session, sp_track* track) {}
void spotify_streaming_error_callback(sp_session* session, sp_error error) {}
void spotify_userinfo_updated_callback(sp_session* session) {}

// 音频回调函数实现
int spotify_audio_write_callback(sp_audio_driver* driver, const void* frames, int num_frames) { return num_frames; }
void spotify_audio_flush_callback(sp_audio_driver* driver) {}
void spotify_audio_close_callback(sp_audio_driver* driver) {}