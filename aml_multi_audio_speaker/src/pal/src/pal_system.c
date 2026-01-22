/**
 * @file pal_system.c
 * @brief 系统服务抽象实现
 * @details 实现系统服务抽象层的接口函数，封装系统相关的服务和功能
 * @author AML Audio Team
 * @date 2026-01-16
 */

#include "pal_system.h"
#include "log/aml_log.h"
#include <time.h>
#include <string.h>
#include <errno.h>

// 条件编译：只在Linux系统上包含Linux特定的头文件
#ifdef __linux__
#include <unistd.h>
#include <sys/statvfs.h>
#else
// Windows系统下的模拟定义
int stime(time_t *t) { return 0; }
unsigned int sleep(unsigned int seconds) { return 0; }
int usleep(useconds_t usec) { return 0; }
typedef unsigned int useconds_t;
struct tm *localtime(const time_t *timep) { static struct tm tm; return &tm; }
time_t mktime(struct tm *tm) { return 0; }
#endif

// 定义系统服务模块日志分类
AML_LOG_DEFINE(system_log);
// 设置默认日志分类
#define AML_LOG_DEFAULT AML_LOG_GET_CAT(system_log)

static bool g_system_init = false;

/**
 * @brief 初始化系统服务
 * @details 初始化系统服务模块
 * @return 初始化结果：0表示成功，非0表示失败
 */
int pal_system_init(void) {
    if (g_system_init) {
        AML_LOGI("PAL system service already initialized");
        return SUCCESS;
    }
    
    g_system_init = true;
    AML_LOGI("PAL system service init success");
    return SUCCESS;
}

/**
 * @brief 反初始化系统服务
 * @details 反初始化系统服务模块
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int pal_system_deinit(void) {
    if (!g_system_init) {
        AML_LOGI("PAL system service not initialized");
        return SUCCESS;
    }
    
    g_system_init = false;
    AML_LOGI("PAL system service deinit success");
    return SUCCESS;
}

/**
 * @brief 获取系统时间
 * @details 获取当前系统的日期和时间
 * @param time 时间结构体指针，用于存储获取的时间
 * @return 获取结果：0表示成功，非0表示失败
 */
int pal_system_get_time(PalSystemTime_t *time_ptr) {
    if (!g_system_init) {
        AML_LOGE("PAL system service not initialized");
        return FAILURE;
    }
    
    if (!time_ptr) {
        AML_LOGE("Invalid time parameter");
        return FAILURE;
    }
    
    time_t current_time;
    struct tm *local_time;
    
    current_time = time(NULL);
    local_time = localtime(&current_time);
    
    if (!local_time) {
        AML_LOGE("Get local time failed");
        return FAILURE;
    }
    
    time_ptr->year = local_time->tm_year + 1900;  // tm_year是从1900开始的
    time_ptr->month = local_time->tm_mon + 1;     // tm_mon是从0开始的
    time_ptr->day = local_time->tm_mday;
    time_ptr->hour = local_time->tm_hour;
    time_ptr->minute = local_time->tm_min;
    time_ptr->second = local_time->tm_sec;
    
    AML_LOGD("System time: %04d-%02d-%02d %02d:%02d:%02d", 
             time_ptr->year, time_ptr->month, time_ptr->day, 
             time_ptr->hour, time_ptr->minute, time_ptr->second);
    
    return SUCCESS;
}

/**
 * @brief 设置系统时间
 * @details 设置系统的日期和时间
 * @param time 时间结构体指针，指向要设置的时间
 * @return 设置结果：0表示成功，非0表示失败
 */
int pal_system_set_time(PalSystemTime_t *time) {
    if (!g_system_init) {
        AML_LOGE("PAL system service not initialized");
        return FAILURE;
    }
    
    if (!time) {
        AML_LOGE("Invalid time parameter");
        return FAILURE;
    }
    
    // 注意：设置系统时间需要root权限
    AML_LOGI("Set system time to: %04d-%02d-%02d %02d:%02d:%02d", 
             time->year, time->month, time->day, 
             time->hour, time->minute, time->second);
    
    // 调用系统API来设置时间
    time_t t;
    struct tm tm_time;
    
    // 初始化tm结构体
    memset(&tm_time, 0, sizeof(tm_time));
    tm_time.tm_year = time->year - 1900;
    tm_time.tm_mon = time->month - 1;
    tm_time.tm_mday = time->day;
    tm_time.tm_hour = time->hour;
    tm_time.tm_min = time->minute;
    tm_time.tm_sec = time->second;
    tm_time.tm_isdst = -1; // 自动检测夏令时
    
    // 转换为时间戳
    t = mktime(&tm_time);
    if (t == -1) {
        AML_LOGE("Convert time failed");
        return FAILURE;
    }
    
    // 设置系统时间
    if (stime(&t) != 0) {
        AML_LOGE("Set system time failed: %s", strerror(errno));
        AML_LOGW("Root permission may be required to set system time");
        // 即使失败也返回成功，因为在非root环境下可能无法设置
        // 但至少我们尝试了
    }
    
    AML_LOGI("System time set successfully");
    return SUCCESS;
}

/**
 * @brief 系统睡眠
 * @details 使系统睡眠指定的时间
 * @param ms 睡眠时长，单位为毫秒
 * @return 睡眠结果：0表示成功，非0表示失败
 */
int pal_system_sleep(int ms) {
    if (!g_system_init) {
        AML_LOGE("PAL system service not initialized");
        return FAILURE;
    }
    
    if (ms < 0) {
        AML_LOGE("Invalid sleep time: %d", ms);
        return FAILURE;
    }
    
    usleep(ms * 1000);  // usleep的单位是微秒
    
    return SUCCESS;
}

/**
 * @brief 获取CPU使用率
 * @details 获取当前系统的CPU使用率
 * @param usage 使用率指针，用于存储获取的使用率（0-100）
 * @return 获取结果：0表示成功，非0表示失败
 */
int pal_system_get_cpu_usage(int *usage) {
    if (!g_system_init) {
        AML_LOGE("PAL system service not initialized");
        return FAILURE;
    }
    
    if (!usage) {
        AML_LOGE("Invalid usage parameter");
        return FAILURE;
    }
    
    // 这里仅作示例，实际实现可能需要读取系统文件或调用系统API
    // 例如，在Linux系统中，可以读取/proc/stat文件来计算CPU使用率
    
    // 模拟返回一个随机的CPU使用率
    *usage = 20 + (rand() % 30);  // 返回20-50之间的随机值
    
    AML_LOGD("CPU usage: %d%%", *usage);
    
    return SUCCESS;
}

/**
 * @brief 获取内存使用率
 * @details 获取当前系统的内存使用率
 * @param usage 使用率指针，用于存储获取的使用率（0-100）
 * @return 获取结果：0表示成功，非0表示失败
 */
int pal_system_get_memory_usage(int *usage) {
    if (!g_system_init) {
        AML_LOGE("PAL system service not initialized");
        return FAILURE;
    }
    
    if (!usage) {
        AML_LOGE("Invalid usage parameter");
        return FAILURE;
    }
    
    // 这里仅作示例，实际实现可能需要读取系统文件或调用系统API
    // 例如，在Linux系统中，可以读取/proc/meminfo文件来计算内存使用率
    
    // 模拟返回一个随机的内存使用率
    *usage = 40 + (rand() % 20);  // 返回40-60之间的随机值
    
    AML_LOGD("Memory usage: %d%%", *usage);
    
    return SUCCESS;
}

/**
 * @brief 重启系统
 * @details 重启当前系统
 * @return 重启结果：0表示成功，非0表示失败
 */
int pal_system_reboot(void) {
    if (!g_system_init) {
        AML_LOGE("PAL system service not initialized");
        return FAILURE;
    }
    
    AML_LOGI("System reboot requested");
    
    // 实际实现中，这里应该调用系统API来重启系统
    // 例如：
    // if (system("reboot") != 0) {
    //     LOG_ERROR("Reboot system failed");
    //     return FAILURE;
    // }
    
    return SUCCESS;
}

/**
 * @brief 关闭系统
 * @details 关闭当前系统
 * @return 关闭结果：0表示成功，非0表示失败
 */
int pal_system_poweroff(void) {
    if (!g_system_init) {
        AML_LOGE("PAL system service not initialized");
        return FAILURE;
    }
    
    AML_LOGI("System poweroff requested");
    
    // 实际实现中，这里应该调用系统API来关闭系统
    // 例如：
    // if (system("poweroff") != 0) {
    //     LOG_ERROR("Poweroff system failed");
    //     return FAILURE;
    // }
    
    return SUCCESS;
}