#include "logger.h"
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdint.h>

/******************************************************************************************
 * 静态全局变量：日志系统私有，外部不可见，保证封装性
 ******************************************************************************************/
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER; // 日志互斥锁，线程安全
static FILE *g_log_fp = NULL;                                   // 日志文件句柄
static uint64_t g_log_file_size = 0;                            // 当前日志文件大小

/******************************************************************************************
 * 私有函数声明：日志系统内部调用，无外部依赖
 ******************************************************************************************/
static const char *log_level_to_str(LogLevel_e level);
static int log_create_dir(const char *path);
static int log_rotate_file(void);
static uint64_t log_get_file_size(const char *path);

/******************************************************************************************
 * 日志系统初始化：项目启动时在main.c中调用，仅初始化一次
 ******************************************************************************************/
int log_system_init(void)
{
    int ret = 0;
    const char *log_dir = "./log";

    // 1. 创建日志目录，不存在则创建
    ret = log_create_dir(log_dir);
    if (ret != 0)
    {
        fprintf(stderr, "log dir create failed: %s\n", log_dir);
        return -1;
    }

    // 2. 打开日志文件，追加模式
    g_log_fp = fopen(LOG_FILE_PATH, "a+");
    if (g_log_fp == NULL)
    {
        fprintf(stderr, "log file open failed: %s, errno: %d\n", LOG_FILE_PATH, errno);
        return -1;
    }

    // 3. 获取当前日志文件大小，用于轮转判断
    g_log_file_size = log_get_file_size(LOG_FILE_PATH);

    // 4. 打印日志系统启动信息，仅高端产品可见
    LOG_INFO("=====================================");
    LOG_INFO("Audio Speaker Log System Init Success");
    LOG_INFO("Product Type: %d, Log Level: %d", CURRENT_PRODUCT_TYPE, SYS_LOG_LEVEL);
    LOG_INFO("Log File: %s, Max Size: %dMB", LOG_FILE_PATH, LOG_MAX_SIZE_MB);
    LOG_INFO("=====================================");

    return 0;
}

/******************************************************************************************
 * 日志系统反初始化：项目退出时调用，释放资源
 ******************************************************************************************/
void log_system_deinit(void)
{
    pthread_mutex_lock(&g_log_mutex);
    if (g_log_fp != NULL)
    {
        fflush(g_log_fp);
        fclose(g_log_fp);
        g_log_fp = NULL;
    }
    pthread_mutex_unlock(&g_log_mutex);
    pthread_mutex_destroy(&g_log_mutex);

    LOG_INFO("Log System Deinit Success");
}

/******************************************************************************************
 * 日志核心打印函数：实现日志格式化、双端输出、文件轮转、线程安全
 ******************************************************************************************/
void log_print(LogLevel_e level, const char *file, int line, const char *func, const char *fmt, ...)
{
    va_list args;
    char time_buf[32] = {0};
    char log_buf[1024] = {0};
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);

    // 线程安全：加锁防止多线程日志乱序
    pthread_mutex_lock(&g_log_mutex);

    // 1. 日志文件轮转判断：超过最大值则轮转
    if (g_log_file_size >= (uint64_t)LOG_MAX_SIZE_MB * 1024 * 1024)
    {
        log_rotate_file();
        g_log_file_size = log_get_file_size(LOG_FILE_PATH);
    }

    // 2. 格式化当前时间：YYYY-MM-DD HH:MM:SS
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_now);

    // 3. 格式化日志头：时间 + 等级 + 文件 + 行号 + 函数名
    snprintf(log_buf, sizeof(log_buf), "[%s] [%s] [%s:%d][%s] ", 
             time_buf, log_level_to_str(level), file, line, func);

    // 4. 格式化日志内容
    va_start(args, fmt);
    vsnprintf(log_buf + strlen(log_buf), sizeof(log_buf) - strlen(log_buf) - 1, fmt, args);
    va_end(args);
    strcat(log_buf, "\n");

    // 5. 输出到控制台（串口）
    if (LOG_PRINT_CONSOLE)
    {
        switch (level)
        {
            case LOG_LEVEL_ERROR: fprintf(stderr, "%s", log_buf); break;
            default: fprintf(stdout, "%s", log_buf); break;
        }
        fflush(stdout);
        fflush(stderr);
    }

    // 6. 输出到日志文件
    if (LOG_PRINT_FILE && g_log_fp != NULL)
    {
        fwrite(log_buf, strlen(log_buf), 1, g_log_fp);
        fflush(g_log_fp);
        g_log_file_size += strlen(log_buf);
    }

    // 解锁
    pthread_mutex_unlock(&g_log_mutex);
}

/******************************************************************************************
 * 私有函数：日志等级转字符串
 ******************************************************************************************/
static const char *log_level_to_str(LogLevel_e level)
{
    switch (level)
    {
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_WARN:  return "WARN ";
        case LOG_LEVEL_INFO:  return "INFO ";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        default:              return "NONE ";
    }
}

/******************************************************************************************
 * 私有函数：创建目录，支持多级目录
 ******************************************************************************************/
static int log_create_dir(const char *path)
{
    char dir_buf[256] = {0};
    strncpy(dir_buf, path, sizeof(dir_buf)-1);
    char *p = dir_buf + 1;

    while (*p != '\0')
    {
        if (*p == '/')
        {
            *p = '\0';
            if (access(dir_buf, F_OK) != 0)
            {
                if (mkdir(dir_buf, 0755) != 0)
                {
                    return -1;
                }
            }
            *p = '/';
        }
        p++;
    }

    if (access(dir_buf, F_OK) != 0)
    {
        if (mkdir(dir_buf, 0755) != 0)
        {
            return -1;
        }
    }

    return 0;
}

/******************************************************************************************
 * 私有函数：日志文件轮转，防止日志文件过大
 * 轮转规则：aml_audio.log → aml_audio.log.1 → aml_audio.log.2 ... 最多保留5个
 ******************************************************************************************/
static int log_rotate_file(void)
{
    char old_path[256] = {0};
    char new_path[256] = {0};
    int i = 0;

    if (g_log_fp != NULL)
    {
        fflush(g_log_fp);
        fclose(g_log_fp);
        g_log_fp = NULL;
    }

    // 日志文件后移：先删最旧的，再依次后移
    for (i = LOG_MAX_BACKUP_COUNT; i > 0; i--)
    {
        snprintf(old_path, sizeof(old_path), "%s.%d", LOG_FILE_PATH, i-1);
        snprintf(new_path, sizeof(new_path), "%s.%d", LOG_FILE_PATH, i);
        if (access(old_path, F_OK) == 0)
        {
            rename(old_path, new_path);
        }
    }

    // 重命名当前日志文件为.1
    rename(LOG_FILE_PATH, old_path);

    // 重新创建新的日志文件
    g_log_fp = fopen(LOG_FILE_PATH, "a+");
    if (g_log_fp == NULL)
    {
        fprintf(stderr, "log rotate file open failed: %d\n", errno);
        return -1;
    }

    return 0;
}

/******************************************************************************************
 * 私有函数：获取文件大小
 ******************************************************************************************/
static uint64_t log_get_file_size(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
    {
        return 0;
    }
    return (uint64_t)st.st_size;
}