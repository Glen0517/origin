#include "system_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <pthread.h>

static void system_log_write_to_file(SystemLogService *service, const char *message)
{
    if (!service->file_output || !service->log_file_path)
        return;

    FILE *file = fopen(service->log_file_path, "a");
    if (!file)
        return;

    fprintf(file, "%s\n", message);
    fclose(file);
}

static void system_log_write_to_console(SystemLogService *service, const char *message)
{
    if (!service->console_output)
        return;

    const char *color_prefix = "\033[0m";
    const char *color_suffix = "\033[0m";

    // Simple color coding based on log level
    if (strstr(message, "[DEBUG]") != NULL) {
        color_prefix = "\033[36m";
    } else if (strstr(message, "[INFO]") != NULL) {
        color_prefix = "\033[32m";
    } else if (strstr(message, "[WARN]") != NULL) {
        color_prefix = "\033[33m";
    } else if (strstr(message, "[ERROR]") != NULL || strstr(message, "[FATAL]") != NULL) {
        color_prefix = "\033[31m";
    }

    printf("%s%s%s\n", color_prefix, message, color_suffix);
}

static const char* log_level_to_string(LogLevel level)
{
    switch (level) {
        case LOG_LEVEL_DEBUG: return "[DEBUG]";
        case LOG_LEVEL_INFO:  return "[INFO]";
        case LOG_LEVEL_WARN:  return "[WARN]";
        case LOG_LEVEL_ERROR: return "[ERROR]";
        case LOG_LEVEL_FATAL: return "[FATAL]";
        default: return "[UNKNOWN]";
    }
}

SystemLogService* system_log_create(struct pw_loop *loop, const char *log_file_path, LogLevel min_level, bool console_output, bool file_output)
{
    SystemLogService *service = calloc(1, sizeof(SystemLogService));
    if (!service)
        return NULL;

    service->loop = loop;
    service->min_level = min_level;
    service->console_output = console_output;
    service->file_output = file_output;

    if (log_file_path) {
        service->log_file_path = strdup(log_file_path);
    }

    pthread_mutex_init(&service->log_mutex, NULL);

    return service;
}

void system_log_destroy(SystemLogService *service)
{
    if (!service)
        return;

    pthread_mutex_destroy(&service->log_mutex);
    free(service->log_file_path);
    free(service);
}

int system_log_start(SystemLogService *service)
{
    if (!service)
        return -1;

    system_log_log_message(service, LOG_LEVEL_INFO, "System log service started");
    return 0;
}

int system_log_stop(SystemLogService *service)
{
    if (!service)
        return -1;

    system_log_log_message(service, LOG_LEVEL_INFO, "System log service stopped");
    return 0;
}

void system_log_log_message(SystemLogService *service, LogLevel level, const char *format, ...)
{
    if (!service || level < service->min_level)
        return;

    pthread_mutex_lock(&service->log_mutex);

    // Get current time
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // Format log message
    va_list args;
    va_start(args, format);

    char message[1024];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // Create full log line with timestamp and level
    char log_line[1024 + 32]; // Extra space for timestamp and level
    snprintf(log_line, sizeof(log_line), "%s %s %s", time_str, log_level_to_string(level), message);

    // Write to outputs
    system_log_write_to_file(service, log_line);
    system_log_write_to_console(service, log_line);

    pthread_mutex_unlock(&service->log_mutex);
}

void system_log_set_min_level(SystemLogService *service, LogLevel level)
{
    if (service)
        service->min_level = level;
}