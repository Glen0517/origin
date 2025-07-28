#include "system_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

// 添加日志配置结构体
typedef struct {
    const char *log_file_path;
    LogLevel min_level;
    bool console_output;
    bool file_output;
    size_t max_file_size; // 最大文件大小(字节)
    int max_backup_files; // 最大备份文件数
} LogConfig;

// 修改服务结构体
struct SystemLogService {
    struct pw_loop *loop;
    LogConfig config;
    pthread_mutex_t log_mutex;
    FILE *log_file; // 保持文件句柄打开
    unsigned long current_file_size;
};

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

// 修改创建函数
SystemLogService* system_log_create(struct pw_loop *loop, const char *log_file_path, LogLevel min_level, bool console_output, bool file_output)
{
    SystemLogService *service = calloc(1, sizeof(SystemLogService));
    if (!service) return NULL;

    service->loop = loop;
    service->config.min_level = min_level;
    service->config.console_output = console_output;
    service->config.file_output = file_output;
    service->config.max_file_size = 1024 * 1024; // 1MB
    service->config.max_backup_files = 5;
    service->log_file = NULL;
    service->current_file_size = 0;

    if (log_file_path) {
        service->config.log_file_path = strdup(log_file_path);
        // 尝试打开现有日志文件
        service->log_file = fopen(log_file_path, "a+");
        if (service->log_file) {
            // 获取当前文件大小
            fseek(service->log_file, 0, SEEK_END);
            service->current_file_size = ftell(service->log_file);
        }
    }

    pthread_mutex_init(&service->log_mutex, NULL);
    return service;
}

// 添加日志轮转函数
static int rotate_logs(SystemLogService *service)
{
    if (!service || !service->config.log_file_path) return -1;

    // 关闭当前日志文件
    if (service->log_file) {
        fclose(service->log_file);
        service->log_file = NULL;
    }

    // 轮转备份文件
    char backup_path[256];
    for (int i = service->config.max_backup_files - 1; i >= 0; i--) {
        // 构建当前备份文件名
        if (i == 0) {
            snprintf(backup_path, sizeof(backup_path), "%s.1", service->config.log_file_path);
        } else {
            snprintf(backup_path, sizeof(backup_path), "%s.%d", service->config.log_file_path, i + 1);
        }

        // 构建源文件名
        char src_path[256];
        if (i == 0) {
            snprintf(src_path, sizeof(src_path), "%s", service->config.log_file_path);
        } else {
            snprintf(src_path, sizeof(src_path), "%s.%d", service->config.log_file_path, i);
        }

        // 如果源文件存在，则重命名
        if (access(src_path, F_OK) == 0) {
            rename(src_path, backup_path);
        }
    }

    // 创建新日志文件
    service->log_file = fopen(service->config.log_file_path, "w");
    service->current_file_size = 0;
    return service->log_file ? 0 : -1;
}

// 修改日志写入函数
static void system_log_write_to_file(SystemLogService *service, const char *message)
{
    if (!service->config.file_output || !service->config.log_file_path) return;

    pthread_mutex_lock(&service->log_mutex);

    // 检查是否需要轮转日志
    size_t message_len = strlen(message) + 1; // +1 for newline
    if (service->log_file && service->current_file_size + message_len > service->config.max_file_size) {
        rotate_logs(service);
    }

    // 如果文件未打开，则尝试打开
    if (!service->log_file) {
        service->log_file = fopen(service->config.log_file_path, "a");
        if (!service->log_file) {
            pthread_mutex_unlock(&service->log_mutex);
            return;
        }
        // 获取当前文件大小
        fseek(service->log_file, 0, SEEK_END);
        service->current_file_size = ftell(service->log_file);
    }

    // 写入日志
    fprintf(service->log_file, "%s\n", message);
    fflush(service->log_file); // 确保立即写入
    service->current_file_size += message_len;

    pthread_mutex_unlock(&service->log_mutex);
}

// 修改日志消息函数
void system_log_log_message(SystemLogService *service, LogLevel level, const char *format, ...)
{
    if (!service || level < service->config.min_level) return;

    pthread_mutex_lock(&service->log_mutex);

    // 获取当前时间
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // 动态分配缓冲区格式化日志消息
    va_list args;
    va_start(args, format);
    char *message = NULL;
    int message_len = vasprintf(&message, format, args);
    va_end(args);

    if (message_len < 0 || !message) {
        pthread_mutex_unlock(&service->log_mutex);
        return;
    }

    // 创建完整日志行
    char *log_line = NULL;
    int log_line_len = asprintf(&log_line, "%s %s %s", time_str, log_level_to_string(level), message);
    free(message);

    if (log_line_len < 0 || !log_line) {
        pthread_mutex_unlock(&service->log_mutex);
        return;
    }

    // 写入输出
    system_log_write_to_file(service, log_line);
    system_log_write_to_console(service, log_line);

    free(log_line);
    pthread_mutex_unlock(&service->log_mutex);
}

// 修改销毁函数
void system_log_destroy(SystemLogService *service)
{
    if (!service) return;

    pthread_mutex_lock(&service->log_mutex);
    if (service->log_file) {
        fclose(service->log_file);
        service->log_file = NULL;
    }
    pthread_mutex_unlock(&service->log_mutex);

    pthread_mutex_destroy(&service->log_mutex);
    free((char*)service->config.log_file_path);
    free(service);
}

// 添加日志配置函数
void system_log_set_max_file_size(SystemLogService *service, size_t size)
{
    if (service) service->config.max_file_size = size;
}

void system_log_set_max_backup_files(SystemLogService *service, int count)
{
    if (service && count > 0) service->config.max_backup_files = count;
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

void system_log_set_min_level(SystemLogService *service, LogLevel level)
{
    if (service)
        service->min_level = level;
}