#ifndef DBUS_UTILS_H
#define DBUS_UTILS_H

#include <stdbool.h>
#include <stdint.h>

// D-Bus信号类型枚举
typedef enum {
    DBUS_SIGNAL_CONNECTION_STATE_CHANGED,
    DBUS_SIGNAL_STREAM_STARTED,
    DBUS_SIGNAL_STREAM_STOPPED,
    DBUS_SIGNAL_ERROR_OCCURRED,
    DBUS_SIGNAL_MAX
} DBusSignalType;

// D-Bus连接初始化
// service_name: 服务名称标识（如"AirPlay2"、"SpotifyConnect"）
bool dbus_initialize(const char* service_name);

// D-Bus连接清理
void dbus_cleanup(void);

// 发送D-Bus信号
// service: 服务名称
// type: 信号类型
// details: JSON格式的事件详情字符串
bool dbus_emit_signal(const char* service, DBusSignalType type, const char* details);

#endif // DBUS_UTILS_H