/**
 * @file exception.c
 * @brief 系统异常处理模块
 * @details 实现系统异常的捕获、处理、日志记录和系统恢复
 * @author AML Audio Team
 * @date 2026-01-19
 */

#include "system.h"
#include "logger.h"
#include "event.h"
#include "hal.h"
#include "pal.h"
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <execinfo.h>

/******************************************************************************************
 * 异常处理内部数据结构
 ******************************************************************************************/

/**
 * @brief 异常信息结构体
 */
typedef struct {
    int exception_type;      // 异常类型
    char exception_msg[256]; // 异常消息
    void *exception_addr;    // 异常地址
    int exception_code;      // 异常代码
    time_t timestamp;        // 发生时间
} ExceptionInfo_t;

/**
 * @brief 异常历史记录
 */
#define MAX_EXCEPTION_HISTORY 10
static ExceptionInfo_t g_exception_history[MAX_EXCEPTION_HISTORY];
static int g_exception_count = 0;
static bool g_exception_handler_init = false;

/******************************************************************************************
 * 异常处理内部函数
 ******************************************************************************************/

/**
 * @brief 记录异常信息
 * @param type 异常类型
 * @param msg 异常消息
 * @param addr 异常地址
 * @param code 异常代码
 */
static void exception_record(int type, const char *msg, void *addr, int code) {
    // 记录异常信息到历史记录
    ExceptionInfo_t *info = &g_exception_history[g_exception_count % MAX_EXCEPTION_HISTORY];
    info->exception_type = type;
    strncpy(info->exception_msg, msg, sizeof(info->exception_msg) - 1);
    info->exception_addr = addr;
    info->exception_code = code;
    info->timestamp = time(NULL);
    
    g_exception_count++;
    
    // 记录到系统日志
    LOG_ERROR("Exception recorded: type=%d, msg=%s, addr=%p, code=%d, count=%d", 
              type, msg, addr, code, g_exception_count);
}

/**
 * @brief 生成异常报告
 * @param type 异常类型
 * @param msg 异常消息
 * @param addr 异常地址
 * @param code 异常代码
 */
static void exception_generate_report(int type, const char *msg, void *addr, int code) {
    char report[1024] = {0};
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[64] = {0};
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    snprintf(report, sizeof(report), 
             "=== Exception Report ===\n" 
             "Time: %s\n" 
             "Type: %d\n" 
             "Message: %s\n" 
             "Address: %p\n" 
             "Code: %d\n" 
             "Count: %d\n" 
             "System Status:\n", 
             time_str, type, msg, addr, code, g_exception_count);
    
    // 添加系统状态信息
    int cpu_usage = 0;
    int mem_usage = 0;
    pal_system_get_cpu_usage(&cpu_usage);
    pal_system_get_memory_usage(&mem_usage);
    
    char system_status[256] = {0};
    snprintf(system_status, sizeof(system_status), 
             "  CPU Usage: %d%%\n" 
             "  Memory Usage: %d%%\n" 
             "  System State: %d\n" 
             "=====================\n", 
             cpu_usage, mem_usage, system_api_get_state());
    
    strcat(report, system_status);
    
    // 保存异常报告到文件
    char filename[128] = {0};
    snprintf(filename, sizeof(filename), "/tmp/exception_report_%d.txt", (int)now);
    FILE *fp = fopen(filename, "w");
    if (fp) {
        fwrite(report, 1, strlen(report), fp);
        fclose(fp);
        LOG_INFO("Exception report saved to: %s", filename);
    } else {
        LOG_ERROR("Failed to save exception report");
    }
    
    // 打印异常报告
    LOG_ERROR("%s", report);
}

/**
 * @brief 系统恢复策略
 * @param type 异常类型
 */
static int exception_recovery(int type) {
    LOG_INFO("Executing system recovery for exception type: %d", type);
    
    switch (type) {
        case EXCEPTION_TYPE_HARDWARE:
            // 硬件异常：重启硬件驱动
            LOG_INFO("Recovering from hardware exception");
            hal_audio_deinit();
            hal_audio_init();
            hal_bt_deinit();
            hal_bt_init();
            break;
            
        case EXCEPTION_TYPE_MEMORY:
            // 内存异常：释放不必要的内存
            LOG_INFO("Recovering from memory exception");
            // 这里可以添加内存清理代码
            break;
            
        case EXCEPTION_TYPE_NETWORK:
            // 网络异常：重启网络服务
            LOG_INFO("Recovering from network exception");
            // 这里可以添加网络服务重启代码
            break;
            
        case EXCEPTION_TYPE_AUDIO:
            // 音频异常：重启音频系统
            LOG_INFO("Recovering from audio exception");
            hal_audio_deinit();
            hal_audio_init();
            break;
            
        default:
            // 通用异常：重启系统服务
            LOG_INFO("Recovering from general exception");
            break;
    }
    
    // 发送系统恢复事件
    event_notify(EVENT_SYSTEM_RECOVERY, (void *)&type);
    
    LOG_INFO("System recovery completed");
    return SUCCESS;
}

/**
 * @brief 信号处理函数
 * @param signum 信号编号
 */
static void signal_handler(int signum) {
    void *callstack[10];
    int stack_size = 0;
    char *stack_str = NULL;
    
    LOG_ERROR("Caught signal: %d", signum);
    
    // 获取调用栈
    stack_size = backtrace(callstack, 10);
    stack_str = backtrace_symbols_fd(callstack, stack_size, STDERR_FILENO);
    
    // 记录异常信息
    char msg[256] = {0};
    switch (signum) {
        case SIGSEGV:
            strcpy(msg, "Segmentation fault");
            break;
        case SIGBUS:
            strcpy(msg, "Bus error");
            break;
        case SIGILL:
            strcpy(msg, "Illegal instruction");
            break;
        case SIGFPE:
            strcpy(msg, "Floating point exception");
            break;
        case SIGABRT:
            strcpy(msg, "Aborted");
            break;
        default:
            snprintf(msg, sizeof(msg), "Signal %d", signum);
            break;
    }
    
    exception_record(EXCEPTION_TYPE_SIGNAL, msg, NULL, signum);
    exception_generate_report(EXCEPTION_TYPE_SIGNAL, msg, NULL, signum);
    
    // 执行系统恢复
    exception_recovery(EXCEPTION_TYPE_SIGNAL);
    
    // 对于致命信号，需要重启系统
    if (signum == SIGSEGV || signum == SIGBUS || signum == SIGILL) {
        LOG_ERROR("Fatal signal received, rebooting system...");
        // 这里可以添加重启系统的代码
        // system("reboot");
    }
}

/******************************************************************************************
 * 异常处理对外接口
 ******************************************************************************************/

/**
 * @brief 初始化异常处理模块
 * @return 初始化结果：0表示成功，非0表示失败
 */
int exception_init(void) {
    if (g_exception_handler_init) {
        LOG_INFO("Exception handler already initialized");
        return SUCCESS;
    }
    
    // 初始化异常历史记录
    memset(g_exception_history, 0, sizeof(g_exception_history));
    g_exception_count = 0;
    
    // 注册信号处理函数
    signal(SIGSEGV, signal_handler);
    signal(SIGBUS, signal_handler);
    signal(SIGILL, signal_handler);
    signal(SIGFPE, signal_handler);
    signal(SIGABRT, signal_handler);
    
    // 初始化完成
    g_exception_handler_init = true;
    LOG_INFO("Exception handler initialized successfully");
    return SUCCESS;
}

/**
 * @brief 反初始化异常处理模块
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int exception_deinit(void) {
    if (!g_exception_handler_init) {
        LOG_INFO("Exception handler not initialized");
        return SUCCESS;
    }
    
    // 恢复默认信号处理
    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    signal(SIGILL, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
    
    // 清空异常历史记录
    memset(g_exception_history, 0, sizeof(g_exception_history));
    g_exception_count = 0;
    
    g_exception_handler_init = false;
    LOG_INFO("Exception handler deinitialized successfully");
    return SUCCESS;
}

/**
 * @brief 处理系统异常
 * @param type 异常类型
 * @param msg 异常消息
 * @param addr 异常地址
 * @param code 异常代码
 * @return 处理结果：0表示成功，非0表示失败
 */
int exception_handle(int type, const char *msg, void *addr, int code) {
    if (!g_exception_handler_init) {
        LOG_ERROR("Exception handler not initialized");
        return FAILURE;
    }
    
    // 记录异常信息
    exception_record(type, msg, addr, code);
    
    // 生成异常报告
    exception_generate_report(type, msg, addr, code);
    
    // 执行系统恢复
    exception_recovery(type);
    
    // 发送异常事件
    event_notify(EVENT_SYSTEM_EXCEPTION, (void *)&type);
    
    LOG_INFO("Exception handled: type=%d, msg=%s", type, msg);
    return SUCCESS;
}

/**
 * @brief 获取异常历史记录
 * @param history 异常历史记录缓冲区
 * @param max_count 最大记录数
 * @return 实际记录数
 */
int exception_get_history(ExceptionInfo_t *history, int max_count) {
    if (!g_exception_handler_init) {
        LOG_ERROR("Exception handler not initialized");
        return 0;
    }
    
    if (!history || max_count <= 0) {
        LOG_ERROR("Invalid parameters");
        return 0;
    }
    
    int count = (g_exception_count < max_count) ? g_exception_count : max_count;
    int start_idx = (g_exception_count >= MAX_EXCEPTION_HISTORY) ? 
                    (g_exception_count % MAX_EXCEPTION_HISTORY) : 0;
    
    for (int i = 0; i < count; i++) {
        int idx = (start_idx + i) % MAX_EXCEPTION_HISTORY;
        history[i] = g_exception_history[idx];
    }
    
    return count;
}

/**
 * @brief 获取异常统计信息
 * @param total_count 总异常数
 * @param last_exception_time 最后一次异常时间
 * @return 获取结果：0表示成功，非0表示失败
 */
int exception_get_statistics(int *total_count, time_t *last_exception_time) {
    if (!g_exception_handler_init) {
        LOG_ERROR("Exception handler not initialized");
        return FAILURE;
    }
    
    if (total_count) {
        *total_count = g_exception_count;
    }
    
    if (last_exception_time && g_exception_count > 0) {
        int last_idx = (g_exception_count - 1) % MAX_EXCEPTION_HISTORY;
        *last_exception_time = g_exception_history[last_idx].timestamp;
    }
    
    return SUCCESS;
}

/**
 * @brief 清理异常历史记录
 * @return 清理结果：0表示成功，非0表示失败
 */
int exception_clear_history(void) {
    if (!g_exception_handler_init) {
        LOG_ERROR("Exception handler not initialized");
        return FAILURE;
    }
    
    memset(g_exception_history, 0, sizeof(g_exception_history));
    g_exception_count = 0;
    
    LOG_INFO("Exception history cleared");
    return SUCCESS;
}
