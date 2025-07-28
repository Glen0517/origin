#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dbus/dbus.h>
#include <pthread.h>
#include <pipewire/pipewire.h>

#include "playerctl.h"

// 播放器状态枚举
typedef enum {
    PLAYER_STATE_STOPPED,
    PLAYER_STATE_PLAYING,
    PLAYER_STATE_PAUSED,
    PLAYER_STATE_BUFFERING
} PlayerState;

// 播放器信息结构体
typedef struct {
    char name[128];           // 播放器名称
    char identity[256];       // 播放器标识
    PlayerState state;        // 播放状态
    char title[256];          // 当前标题
    char artist[256];         // 当前艺术家
    char album[256];          // 当前专辑
    uint64_t position;        // 当前位置 (微秒)
    uint64_t duration;        // 总时长 (微秒)
    bool can_play;            // 是否可以播放
    bool can_pause;           // 是否可以暂停
    bool can_go_next;         // 是否可以下一曲
    bool can_go_previous;     // 是否可以上一曲
} PlayerInfo;

// Playerctl模块结构体
typedef struct {
    DBusConnection* dbus_conn;  // D-Bus连接
    pthread_mutex_t mutex;      // 互斥锁
    PlayerInfo player;          // 当前播放器信息
    bool initialized;           // 是否已初始化
    // 回调函数
    void (*state_changed_cb)(const PlayerInfo* player);
    void (*metadata_changed_cb)(const PlayerInfo* player);
} PlayerctlModule;

// 全局模块实例
static PlayerctlModule module = {
    .dbus_conn = NULL,
    .initialized = false,
    .state_changed_cb = NULL,
    .metadata_changed_cb = NULL
};

// 前向声明
static DBusHandlerResult dbus_message_handler(DBusConnection* conn, DBusMessage* msg, void* user_data);
static void update_player_state(const char* state_str);
static void update_player_metadata(DBusMessageIter* iter);
static void emit_state_changed();
static void emit_metadata_changed();

// 初始化Playerctl模块
int playerctl_init() {
    int ret = 0;
    DBusError err;

    // 初始化互斥锁
    pthread_mutex_init(&module.mutex, NULL);

    // 初始化D-Bus
    dbus_error_init(&err);
    module.dbus_conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "D-Bus connection failed: %s\n", err.message);
        dbus_error_free(&err);
        return -1;
    }

    // 添加消息过滤器
    dbus_connection_add_filter(module.dbus_conn, dbus_message_handler, NULL, NULL);

    // 请求名称
    const char* dbus_name = "org.mpris.MediaPlayer2.playerctld";
    ret = dbus_bus_request_name(module.dbus_conn, dbus_name, DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "D-Bus name request failed: %s\n", err.message);
        dbus_error_free(&err);
        dbus_connection_unref(module.dbus_conn);
        module.dbus_conn = NULL;
        return -1;
    }

    // 订阅playerctld信号
    const char* playerctld_interface = "org.mpris.MediaPlayer2.playerctld";
    const char* mpris_interface = "org.mpris.MediaPlayer2.Player";

    // 订阅PropertiesChanged信号
    dbus_bus_add_match(module.dbus_conn, "type='signal',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged',path='/org/mpris/MediaPlayer2',arg0namespace='org.mpris.MediaPlayer2.Player'", &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Failed to add match rule: %s\n", err.message);
        dbus_error_free(&err);
    }

    // 刷新连接
    dbus_connection_flush(module.dbus_conn);

    // 初始化播放器信息
    memset(&module.player, 0, sizeof(PlayerInfo));
    module.player.state = PLAYER_STATE_STOPPED;

    // 启动D-Bus监听线程
    pthread_t dbus_thread;
    ret = pthread_create(&dbus_thread, NULL, dbus_listener_thread, NULL);
    if (ret != 0) {
        perror("Failed to create D-Bus listener thread");
        dbus_connection_unref(module.dbus_conn);
        module.dbus_conn = NULL;
        return -1;
    }

    pthread_detach(dbus_thread);
    module.initialized = true;

    printf("Playerctl module initialized successfully\n");
    return 0;
}

// D-Bus监听线程
static void* dbus_listener_thread(void* arg) {
    while (module.initialized && module.dbus_conn) {
        // 阻塞等待消息
        dbus_connection_read_write(module.dbus_conn, -1);
        dbus_message* msg;

        while ((msg = dbus_connection_pop_message(module.dbus_conn)) != NULL) {
            // 处理消息
            dbus_message_handler(module.dbus_conn, msg, NULL);
            dbus_message_unref(msg);
        }

        // 短暂休眠
        usleep(10000);
    }

    return NULL;
}

// D-Bus消息处理函数
static DBusHandlerResult dbus_message_handler(DBusConnection* conn, DBusMessage* msg, void* user_data) {
    if (!dbus_message_is_signal(msg, "org.freedesktop.DBus.Properties", "PropertiesChanged")) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    const char* interface_name;
    DBusMessageIter iter;
    DBusMessageIter dict_iter;
    DBusMessageIter variant_iter;

    if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &interface_name, DBUS_TYPE_ARRAY, &dict_iter, DBUS_TYPE_DICT_ENTRY, &dict_iter, DBUS_TYPE_INVALID)) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    // 检查是否是播放器接口
    if (strcmp(interface_name, "org.mpris.MediaPlayer2.Player") != 0) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    pthread_mutex_lock(&module.mutex);

    // 解析属性变化
    while (dbus_message_iter_get_arg_type(&dict_iter) == DBUS_TYPE_DICT_ENTRY) {
        char* property_name;

        dbus_message_iter_recurse(&dict_iter, &variant_iter);
        dbus_message_iter_get_basic(&variant_iter, &property_name);
        dbus_message_iter_next(&variant_iter);
        dbus_message_iter_recurse(&variant_iter, &variant_iter);

        // 处理不同属性
        if (strcmp(property_name, "PlaybackStatus") == 0) {
            char* state_str;
            dbus_message_iter_get_basic(&variant_iter, &state_str);
            update_player_state(state_str);
            emit_state_changed();
        } else if (strcmp(property_name, "Metadata") == 0) {
            update_player_metadata(&variant_iter);
            emit_metadata_changed();
        } else if (strcmp(property_name, "Position") == 0) {
            dbus_message_iter_get_basic(&variant_iter, &module.player.position);
        } else if (strcmp(property_name, "CanPlay") == 0) {
            dbus_message_iter_get_basic(&variant_iter, &module.player.can_play);
        } else if (strcmp(property_name, "CanPause") == 0) {
            dbus_message_iter_get_basic(&variant_iter, &module.player.can_pause);
        } else if (strcmp(property_name, "CanGoNext") == 0) {
            dbus_message_iter_get_basic(&variant_iter, &module.player.can_go_next);
        } else if (strcmp(property_name, "CanGoPrevious") == 0) {
            dbus_message_iter_get_basic(&variant_iter, &module.player.can_go_previous);
        }

        dbus_message_iter_next(&dict_iter);
    }

    pthread_mutex_unlock(&module.mutex);
    return DBUS_HANDLER_RESULT_HANDLED;
}

// 更新播放器状态
static void update_player_state(const char* state_str) {
    if (strcmp(state_str, "Playing") == 0) {
        module.player.state = PLAYER_STATE_PLAYING;
    } else if (strcmp(state_str, "Paused") == 0) {
        module.player.state = PLAYER_STATE_PAUSED;
    } else if (strcmp(state_str, "Stopped") == 0) {
        module.player.state = PLAYER_STATE_STOPPED;
    } else if (strcmp(state_str, "Buffering") == 0) {
        module.player.state = PLAYER_STATE_BUFFERING;
    }
}

// 更新播放器元数据
static void update_player_metadata(DBusMessageIter* iter) {
    // 解析元数据字典
    while (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_DICT_ENTRY) {
        DBusMessageIter entry_iter;
        DBusMessageIter value_iter;
        char* key;

        dbus_message_iter_recurse(iter, &entry_iter);
        dbus_message_iter_get_basic(&entry_iter, &key);
        dbus_message_iter_next(&entry_iter);
        dbus_message_iter_recurse(&entry_iter, &value_iter);

        // 处理不同元数据
        if (strcmp(key, "xesam:title") == 0) {
            char* title;
            dbus_message_iter_get_basic(&value_iter, &title);
            strncpy(module.player.title, title, sizeof(module.player.title)-1);
        } else if (strcmp(key, "xesam:artist") == 0) {
            // 艺术家是字符串数组
            if (dbus_message_iter_get_arg_type(&value_iter) == DBUS_TYPE_ARRAY) {
                DBusMessageIter array_iter;
                dbus_message_iter_recurse(&value_iter, &array_iter);
                if (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_STRING) {
                    char* artist;
                    dbus_message_iter_get_basic(&array_iter, &artist);
                    strncpy(module.player.artist, artist, sizeof(module.player.artist)-1);
                }
            }
        } else if (strcmp(key, "xesam:album") == 0) {
            char* album;
            dbus_message_iter_get_basic(&value_iter, &album);
            strncpy(module.player.album, album, sizeof(module.player.album)-1);
        } else if (strcmp(key, "mpris:length") == 0) {
            dbus_message_iter_get_basic(&value_iter, &module.player.duration);
        }

        dbus_message_iter_next(iter);
    }
}

// 触发状态变化回调
static void emit_state_changed() {
    if (module.state_changed_cb) {
        module.state_changed_cb(&module.player);
    }
}

// 触发元数据变化回调
static void emit_metadata_changed() {
    if (module.metadata_changed_cb) {
        module.metadata_changed_cb(&module.player);
    }
}

// 播放控制函数
int playerctl_play() {
    if (!module.initialized || !module.dbus_conn) {
        return -1;
    }

    DBusMessage* msg;
    DBusMessage* reply;
    DBusError err;

    dbus_error_init(&err);

    // 创建播放命令消息
    msg = dbus_message_new_method_call(
        "org.mpris.MediaPlayer2.playerctld",
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player",
        "Play"
    );

    if (!msg) {
        fprintf(stderr, "Failed to create D-Bus message\n");
        return -1;
    }

    // 发送消息
    reply = dbus_connection_send_with_reply_and_block(module.dbus_conn, msg, -1, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Play command failed: %s\n", err.message);
        dbus_error_free(&err);
        return -1;
    }

    if (reply) {
        dbus_message_unref(reply);
    }

    return 0;
}

// 暂停控制函数
int playerctl_pause() {
    if (!module.initialized || !module.dbus_conn) {
        return -1;
    }

    DBusMessage* msg;
    DBusMessage* reply;
    DBusError err;

    dbus_error_init(&err);

    // 创建暂停命令消息
    msg = dbus_message_new_method_call(
        "org.mpris.MediaPlayer2.playerctld",
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player",
        "Pause"
    );

    if (!msg) {
        fprintf(stderr, "Failed to create D-Bus message\n");
        return -1;
    }

    // 发送消息
    reply = dbus_connection_send_with_reply_and_block(module.dbus_conn, msg, -1, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Pause command failed: %s\n", err.message);
        dbus_error_free(&err);
        return -1;
    }

    if (reply) {
        dbus_message_unref(reply);
    }

    return 0;
}

// 下一曲控制函数
int playerctl_next() {
    if (!module.initialized || !module.dbus_conn) {
        return -1;
    }

    DBusMessage* msg;
    DBusMessage* reply;
    DBusError err;

    dbus_error_init(&err);

    // 创建下一曲命令消息
    msg = dbus_message_new_method_call(
        "org.mpris.MediaPlayer2.playerctld",
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player",
        "Next"
    );

    if (!msg) {
        fprintf(stderr, "Failed to create D-Bus message\n");
        return -1;
    }

    // 发送消息
    reply = dbus_connection_send_with_reply_and_block(module.dbus_conn, msg, -1, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Next command failed: %s\n", err.message);
        dbus_error_free(&err);
        return -1;
    }

    if (reply) {
        dbus_message_unref(reply);
    }

    return 0;
}

// 上一曲控制函数
int playerctl_previous() {
    if (!module.initialized || !module.dbus_conn) {
        return -1;
    }

    DBusMessage* msg;
    DBusMessage* reply;
    DBusError err;

    dbus_error_init(&err);

    // 创建上一曲命令消息
    msg = dbus_message_new_method_call(
        "org.mpris.MediaPlayer2.playerctld",
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player",
        "Previous"
    );

    if (!msg) {
        fprintf(stderr, "Failed to create D-Bus message\n");
        return -1;
    }

    // 发送消息
    reply = dbus_connection_send_with_reply_and_block(module.dbus_conn, msg, -1, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Previous command failed: %s\n", err.message);
        dbus_error_free(&err);
        return -1;
    }

    if (reply) {
        dbus_message_unref(reply);
    }

    return 0;
}

// 获取当前播放器信息
void playerctl_get_player_info(PlayerInfo* info) {
    if (!info || !module.initialized) {
        return;
    }

    pthread_mutex_lock(&module.mutex);
    *info = module.player;
    pthread_mutex_unlock(&module.mutex);
}

// 设置状态变化回调
void playerctl_set_state_changed_callback(void (*callback)(const PlayerInfo*)) {
    pthread_mutex_lock(&module.mutex);
    module.state_changed_cb = callback;
    pthread_mutex_unlock(&module.mutex);
}

// 设置元数据变化回调
void playerctl_set_metadata_changed_callback(void (*callback)(const PlayerInfo*)) {
    pthread_mutex_lock(&module.mutex);
    module.metadata_changed_cb = callback;
    pthread_mutex_unlock(&module.mutex);
}

// 销毁Playerctl模块
void playerctl_destroy() {
    if (!module.initialized) {
        return;
    }

    module.initialized = false;

    if (module.dbus_conn) {
        dbus_connection_unref(module.dbus_conn);
        module.dbus_conn = NULL;
    }

    pthread_mutex_destroy(&module.mutex);
    printf("Playerctl module destroyed\n");
}