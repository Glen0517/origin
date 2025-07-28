#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dbus/dbus.h>
#include "dbus_utils.h"

// 静态变量
static DBusConnection* dbus_conn = NULL;
static pthread_mutex_t dbus_mutex = PTHREAD_MUTEX_INITIALIZER;
static const char* DBUS_INTERFACE = "org.AudioFramework.MediaService";
static const char* DBUS_OBJECT_PATH = "/org/AudioFramework/MediaService";
static const char* DBUS_SIGNAL_NAME = "EventOccurred";

// D-Bus错误处理
static void handle_dbus_error(DBusError* error, const char* context) {
    if (dbus_error_is_set(error)) {
        fprintf(stderr, "D-Bus error in %s: %s - %s\n", context, error->name, error->message);
        dbus_error_free(error);
    }
}

// 初始化D-Bus连接
bool dbus_initialize(const char* service_name) {
    if (!service_name) {
        fprintf(stderr, "Invalid service name for D-Bus initialization\n");
        return false;
    }

    pthread_mutex_lock(&dbus_mutex);

    if (dbus_conn) {
        pthread_mutex_unlock(&dbus_mutex);
        return true; // 已初始化
    }

    DBusError error;
    dbus_error_init(&error);

    // 连接到会话总线
    dbus_conn = dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (!dbus_conn) {
        handle_dbus_error(&error, "connecting to session bus");
        pthread_mutex_unlock(&dbus_mutex);
        return false;
    }

    // 请求唯一的服务名称
    char unique_name[256];
    snprintf(unique_name, sizeof(unique_name), "org.AudioFramework.%s", service_name);
    int ret = dbus_bus_request_name(dbus_conn, unique_name, DBUS_NAME_FLAG_REPLACE_EXISTING, &error);
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        handle_dbus_error(&error, "requesting service name");
        dbus_connection_unref(dbus_conn);
        dbus_conn = NULL;
        pthread_mutex_unlock(&dbus_mutex);
        return false;
    }

    pthread_mutex_unlock(&dbus_mutex);
    return true;
}

// 发送D-Bus信号
bool dbus_emit_signal(const char* service, DBusSignalType type, const char* details) {
    if (!service || !details || type >= DBUS_SIGNAL_MAX) {
        fprintf(stderr, "Invalid parameters for D-Bus signal emission\n");
        return false;
    }

    pthread_mutex_lock(&dbus_mutex);

    if (!dbus_conn) {
        fprintf(stderr, "D-Bus connection not initialized\n");
        pthread_mutex_unlock(&dbus_mutex);
        return false;
    }

    // 创建信号消息
    DBusMessage* msg = dbus_message_new_signal(
        DBUS_OBJECT_PATH,
        DBUS_INTERFACE,
        DBUS_SIGNAL_NAME
    );

    if (!msg) {
        fprintf(stderr, "Failed to create D-Bus message\n");
        pthread_mutex_unlock(&dbus_mutex);
        return false;
    }

    // 追加参数: service (string), type (int32), details (string)
    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);

    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &service) ||
        !dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &type) ||
        !dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &details)) {
        fprintf(stderr, "Failed to append D-Bus message arguments\n");
        dbus_message_unref(msg);
        pthread_mutex_unlock(&dbus_mutex);
        return false;
    }

    // 发送信号
    DBusError error;
    dbus_error_init(&error);
    bool success = dbus_connection_send(dbus_conn, msg, NULL);
    dbus_connection_flush(dbus_conn);

    handle_dbus_error(&error, "sending signal");
    dbus_message_unref(msg);
    pthread_mutex_unlock(&dbus_mutex);

    return success;
}

// 清理D-Bus连接
void dbus_cleanup(void) {
    pthread_mutex_lock(&dbus_mutex);

    if (dbus_conn) {
        dbus_connection_unref(dbus_conn);
        dbus_conn = NULL;
    }

    pthread_mutex_unlock(&dbus_mutex);
    pthread_mutex_destroy(&dbus_mutex);
}