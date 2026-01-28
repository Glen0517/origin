/**
 * @file error_handler.h
 * @brief 错误处理模块头文件
 * @details 提供错误统计、错误恢复等功能
 * @author AML Audio Team
 * @date 2026-01-28
 */

#ifndef __ERROR_HANDLER_H__
#define __ERROR_HANDLER_H__

#include "common_def.h"
#include "logger.h"

// 错误类型定义
typedef enum {
    ERROR_TYPE_NONE = 0,
    ERROR_TYPE_HARDWARE = 1,
    ERROR_TYPE_SOFTWARE = 2,
    ERROR_TYPE_NETWORK = 3,
    ERROR_TYPE_RESOURCE = 4,
    ERROR_TYPE_CONFIG = 5,
    ERROR_TYPE_MAX
} ErrorType_e;

// 错误级别定义
typedef enum {
    ERROR_LEVEL_INFO = 0,
    ERROR_LEVEL_WARNING = 1,
    ERROR_LEVEL_ERROR = 2,
    ERROR_LEVEL_FATAL = 3,
    ERROR_LEVEL_MAX
} ErrorLevel_e;

// 错误记录结构体
typedef struct ErrorRecord {
    uint32_t error_id;
    ErrorType_e error_type;
    ErrorLevel_e error_level;
    int error_code;
    const char *error_msg;
    const char *module_name;
    const char *file_name;
    int line_num;
    uint64_t timestamp;
    struct ErrorRecord *next;
} ErrorRecord_t;

// 错误统计结构体
typedef struct ErrorStats {
    uint32_t total_errors;
    uint32_t type_counts[ERROR_TYPE_MAX];
    uint32_t level_counts[ERROR_LEVEL_MAX];
    uint32_t module_error_counts[64]; // 模块错误计数
    uint64_t last_error_time;
    ErrorRecord_t *recent_errors; // 最近的错误记录
    uint32_t recent_error_count;
    uint32_t max_recent_errors;
} ErrorStats_t;

// 错误处理结构体
typedef struct {
    ErrorStats_t *error_stats;
    uint32_t error_id_counter;
    bool error_recovery_enabled;
} ErrorHandler_t;

/**
 * @brief 初始化错误处理器
 * @details 创建并初始化错误处理器，用于错误统计和恢复
 * @return 错误处理器指针，失败返回NULL
 */
ErrorHandler_t *error_handler_init(void);

/**
 * @brief 记录错误
 * @details 记录错误信息并更新错误统计
 * @param handler 错误处理器指针
 * @param error_type 错误类型
 * @param error_level 错误级别
 * @param error_code 错误代码
 * @param error_msg 错误消息
 * @param module_name 模块名称
 * @param file_name 文件名称
 * @param line_num 行号
 */
void error_handler_record(ErrorHandler_t *handler, ErrorType_e error_type, 
                        ErrorLevel_e error_level, int error_code, 
                        const char *error_msg, const char *module_name, 
                        const char *file_name, int line_num);

/**
 * @brief 尝试错误恢复
 * @details 根据错误类型和级别尝试恢复策略
 * @param handler 错误处理器指针
 * @param error_type 错误类型
 * @param error_level 错误级别
 * @param error_code 错误代码
 * @param module_name 模块名称
 * @return 恢复是否成功
 */
bool error_handler_recover(ErrorHandler_t *handler, ErrorType_e error_type, 
                         ErrorLevel_e error_level, int error_code, 
                         const char *module_name);

/**
 * @brief 输出错误统计信息
 * @details 输出错误统计和最近的错误记录
 * @param handler 错误处理器指针
 */
void error_handler_print_stats(ErrorHandler_t *handler);

/**
 * @brief 反初始化错误处理器
 * @details 反初始化错误处理器，释放资源
 * @param handler 错误处理器指针
 */
void error_handler_deinit(ErrorHandler_t *handler);

#endif // __ERROR_HANDLER_H__
