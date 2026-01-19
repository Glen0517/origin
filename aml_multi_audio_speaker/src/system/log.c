#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

/******************************************************************************************
 * 【日志模块私有变量】- 线程安全设计
 ******************************************************************************************/
static int g_log_level = LOG_LEVEL_INFO;
static FILE *g_log_file = NULL;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_log_init = 0;

/******************************************************************************************
 * 【日志模块对外接口】- 标准化工业级实现
 ******************************************************************************************/

/**
 * @brief  日志模块初始化
 * @param  level 日志级别
 * @param  log_path 日志文件路径，NULL则只输出到控制台
 * @return SUCCESS/FAILURE
 */
int log_init(int level, const char *log_path) {
    // 设置日志级别
    if (level >= LOG_LEVEL_NONE && level <= LOG_LEVEL_DEBUG) {
        g_log_level = level;
    }
    
    // 打开日志文件（如果指定）
    if (log_path) {
        g_log_file = fopen(log_path, "a+");
        if (!g_log_file) {
            fprintf(stderr, "Failed to open log file: %s\n", log_path);
            return FAILURE;
        }
    }
    
    g_log_init = 1;
    LOG_INFO("Log system initialized, level: %d", g_log_level);
    return SUCCESS;
}

/**
 * @brief  日志模块反初始化
 * @return SUCCESS/FAILURE
 */
int log_deinit(void) {
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
    
    pthread_mutex_destroy(&g_log_mutex);
    g_log_init = 0;
    return SUCCESS;
}

/**
 * @brief  设置日志级别
 * @param  level 日志级别
 * @return SUCCESS/FAILURE
 */
int log_set_level(int level) {
    if (level >= LOG_LEVEL_NONE && level <= LOG_LEVEL_DEBUG) {
        g_log_level = level;
        LOG_INFO("Log level set to: %d", level);
        return SUCCESS;
    }
    return INVALID_PARAM;
}

/**
 * @brief  获取当前日志级别
 * @return 日志级别
 */
int log_get_level(void) {
    return g_log_level;
}

/**
 * @brief  日志核心打印函数（被日志宏封装，src无需直接调用）
 */
void log_print(LogLevel_e level, const char *file, int line, const char *func, const char *fmt, ...) {
    if (level < g_log_level) {
        return;
    }
    
    // 获取当前时间
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    char time_str[20];
    snprintf(time_str, sizeof(time_str), "%04d-%02d-%02d %02d:%02d:%02d",
             tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
             tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
    
    // 日志级别字符串
    const char *level_str;
    switch (level) {
        case LOG_LEVEL_DEBUG:
            level_str = "DEBUG";
            break;
        case LOG_LEVEL_INFO:
            level_str = "INFO";
            break;
        case LOG_LEVEL_WARN:
            level_str = "WARN";
            break;
        case LOG_LEVEL_ERROR:
            level_str = "ERROR";
            break;
        default:
            level_str = "UNKNOWN";
            break;
    }
    
    // 获取文件名（只保留最后一部分）
    const char *filename = strrchr(file, '\\');
    if (!filename) {
        filename = strrchr(file, '/');
    }
    if (filename) {
        filename++;
    } else {
        filename = file;
    }
    
    // 线程安全锁
    pthread_mutex_lock(&g_log_mutex);
    
    // 输出到控制台
    if (LOG_PRINT_CONSOLE) {
        printf("[%s][%s][%s:%d:%s] ", time_str, level_str, filename, line, func);
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        printf("\n");
    }
    
    // 输出到文件（如果打开）
    if (LOG_PRINT_FILE && g_log_file) {
        fprintf(g_log_file, "[%s][%s][%s:%d:%s] ", time_str, level_str, filename, line, func);
        va_list args;
        va_start(args, fmt);
        vfprintf(g_log_file, fmt, args);
        va_end(args);
        fprintf(g_log_file, "\n");
        fflush(g_log_file);
    }
    
    pthread_mutex_unlock(&g_log_mutex);
}

/**
 * @brief  调试日志
 * @param  func 函数名
 * @param  line 行号
 * @param  fmt 格式化字符串
 * @param  ... 可变参数
 */
void log_debug(const char *func, int line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_print(LOG_LEVEL_DEBUG, __FILE__, line, func, fmt, args);
    va_end(args);
}

/**
 * @brief  信息日志
 * @param  func 函数名
 * @param  line 行号
 * @param  fmt 格式化字符串
 * @param  ... 可变参数
 */
void log_info(const char *func, int line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_print(LOG_LEVEL_INFO, __FILE__, line, func, fmt, args);
    va_end(args);
}

/**
 * @brief  警告日志
 * @param  func 函数名
 * @param  line 行号
 * @param  fmt 格式化字符串
 * @param  ... 可变参数
 */
void log_warn(const char *func, int line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_print(LOG_LEVEL_WARN, __FILE__, line, func, fmt, args);
    va_end(args);
}

/**
 * @brief  错误日志
 * @param  func 函数名
 * @param  line 行号
 * @param  fmt 格式化字符串
 * @param  ... 可变参数
 */
void log_error(const char *func, int line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_print(LOG_LEVEL_ERROR, __FILE__, line, func, fmt, args);
    va_end(args);
}

/******************************************************************************************
 * 【日志模块兼容性接口】- 保持与现有代码的兼容性
 ******************************************************************************************/

/**
 * @brief  日志系统初始化：创建日志目录、初始化互斥锁、加载日志配置，在main.c中调用一次即可
 */
int log_system_init(void) {
    // 创建日志目录（如果不存在）
    system("mkdir -p ./log");
    
    // 使用默认配置初始化日志系统
    return log_init(SYS_LOG_LEVEL, LOG_FILE_PATH);
}

/**
 * @brief  日志系统反初始化：关闭日志文件、释放锁资源，程序退出时调用
 */
void log_system_deinit(void) {
    log_deinit();
}
