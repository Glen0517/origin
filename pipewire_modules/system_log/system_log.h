#ifndef SYSTEM_LOG_H
#define SYSTEM_LOG_H

#include <stdint.h>
#include <stdbool.h>
#include <pipewire/pipewire.h>
#include <spa/utils/string.h>
#include <spa/utils/log.h>

typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} LogLevel;

typedef struct {
    struct pw_core *core;
    struct pw_loop *loop;
    struct spa_log *spa_log;
    LogLevel min_level;
    char *log_file_path;
    bool console_output;
    bool file_output;
    pthread_mutex_t log_mutex;
    void *user_data;
} SystemLogService;

SystemLogService* system_log_create(struct pw_loop *loop, const char *log_file_path, LogLevel min_level, bool console_output, bool file_output);
void system_log_destroy(SystemLogService *service);
int system_log_start(SystemLogService *service);
int system_log_stop(SystemLogService *service);
void system_log_log_message(SystemLogService *service, LogLevel level, const char *format, ...);
void system_log_set_min_level(SystemLogService *service, LogLevel level);

#endif // SYSTEM_LOG_H