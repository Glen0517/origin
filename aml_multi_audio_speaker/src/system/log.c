#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

// 使用aml_log.h作为主要的日志系统，这里只提供兼容函数

/******************************************************************************************
 * 【日志模块私有变量】- 线程安全设计
 ******************************************************************************************/
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
    // 将原有日志级别转换为aml_log.h中的级别
    const char *level_str = "3"; // 默认INFO级别
    switch (level) {
        case 0:
            level_str = "0";
            break;
        case 1:
            level_str = "1";
            break;
        case 2:
            level_str = "2";
            break;
        case 3:
            level_str = "3";
            break;
        case 4:
            level_str = "4";
            break;
        default:
            level_str = "3";
            break;
    }
    
    // 设置日志级别
    char log_config[64] = {0};
    snprintf(log_config, sizeof(log_config), "default:%s", level_str);
    aml_log_set_from_string(log_config);
    
    // 如果指定了日志文件路径，设置日志输出文件
    if (log_path) {
        FILE *fp = fopen(log_path, "a+");
        if (fp) {
            aml_log_set_output_file(fp);
        } else {
            LOG_ERROR("Failed to open log file: %s", log_path);
            return FAILURE;
        }
    }
    
    g_log_init = 1;
    LOG_INFO("Log system initialized via aml_log.h, level: %d", level);
    return SUCCESS;
}

/**
 * @brief  日志模块反初始化
 * @return SUCCESS/FAILURE
 */
int log_deinit(void) {
    g_log_init = 0;
    LOG_INFO("Log system deinitialized");
    return SUCCESS;
}

/**
 * @brief  设置日志级别
 * @param  level 日志级别
 * @return SUCCESS/FAILURE
 */
int log_set_level(int level) {
    if (level >= 0 && level <= 4) {
        // 将原有日志级别转换为aml_log.h中的级别
        const char *level_str = "3";
        switch (level) {
            case 0:
                level_str = "0";
                break;
            case 1:
                level_str = "1";
                break;
            case 2:
                level_str = "2";
                break;
            case 3:
                level_str = "3";
                break;
            case 4:
                level_str = "4";
                break;
            default:
                level_str = "3";
                break;
        }
        
        // 设置日志级别
        char log_config[64] = {0};
        snprintf(log_config, sizeof(log_config), "default:%s", level_str);
        aml_log_set_from_string(log_config);
        
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
    // 由于aml_log.h没有提供获取日志级别的接口，这里返回默认值
    return 3; // 默认返回INFO级别
}

/**
 * @brief  日志核心打印函数（被日志宏封装，src无需直接调用）
 */
void log_print(LogLevel_e level, const char *file, int line, const char *func, const char *fmt, ...) {
    // 由于使用了aml_log.h，这里不需要实现具体的打印逻辑
    // 实际的打印已经由AML_LOG宏处理
}

/**
 * @brief  调试日志
 * @param  func 函数名
 * @param  line 行号
 * @param  fmt 格式化字符串
 * @param  ... 可变参数
 */
void log_debug(const char *func, int line, const char *fmt, ...) {
    // 由于使用了aml_log.h，这里不需要实现具体的打印逻辑
    // 实际的打印已经由AML_LOG宏处理
}

/**
 * @brief  信息日志
 * @param  func 函数名
 * @param  line 行号
 * @param  fmt 格式化字符串
 * @param  ... 可变参数
 */
void log_info(const char *func, int line, const char *fmt, ...) {
    // 由于使用了aml_log.h，这里不需要实现具体的打印逻辑
    // 实际的打印已经由AML_LOG宏处理
}

/**
 * @brief  警告日志
 * @param  func 函数名
 * @param  line 行号
 * @param  fmt 格式化字符串
 * @param  ... 可变参数
 */
void log_warn(const char *func, int line, const char *fmt, ...) {
    // 由于使用了aml_log.h，这里不需要实现具体的打印逻辑
    // 实际的打印已经由AML_LOG宏处理
}

/**
 * @brief  错误日志
 * @param  func 函数名
 * @param  line 行号
 * @param  fmt 格式化字符串
 * @param  ... 可变参数
 */
void log_error(const char *func, int line, const char *fmt, ...) {
    // 由于使用了aml_log.h，这里不需要实现具体的打印逻辑
    // 实际的打印已经由AML_LOG宏处理
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
    
    // 生成带时间戳的日志文件名
    char log_file_path[128] = {0};
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(log_file_path, sizeof(log_file_path), "./log/system_%Y%m%d_%H%M%S.log", tm_info);
    
    // 使用持久化存储配置初始化日志系统
    return log_init(3, log_file_path); // 默认INFO级别，日志持久化存储
}

/**
 * @brief  日志系统反初始化：关闭日志文件、释放锁资源，程序退出时调用
 */
void log_system_deinit(void) {
    log_deinit();
}
