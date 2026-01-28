/**
 * @file error_handler.c
 * @brief 错误处理模块实现
 * @details 提供错误统计、错误恢复等功能
 * @author AML Audio Team
 * @date 2026-01-28
 */

#include "error_handler.h"

// 外部函数声明
void peripheral_reset(void);
void bluetooth_reset(void);
void audio_core_deinit(void);
void audio_core_init(void *cfg);
void wifi_media_reconnect(void);
void resource_release_unused(void);
void config_manager_reload(void *manager);

/**
 * @brief 初始化错误处理器
 * @details 创建并初始化错误处理器，用于错误统计和恢复
 * @return 错误处理器指针，失败返回NULL
 */
ErrorHandler_t *error_handler_init(void) {
    ErrorHandler_t *handler = (ErrorHandler_t *)malloc(sizeof(ErrorHandler_t));
    if (!handler) {
        LOG_ERROR("Failed to allocate error handler");
        return NULL;
    }
    
    memset(handler, 0, sizeof(ErrorHandler_t));
    
    // 初始化错误统计
    handler->error_stats = (ErrorStats_t *)malloc(sizeof(ErrorStats_t));
    if (!handler->error_stats) {
        LOG_ERROR("Failed to allocate error stats");
        free(handler);
        return NULL;
    }
    
    memset(handler->error_stats, 0, sizeof(ErrorStats_t));
    handler->error_stats->max_recent_errors = 100;
    handler->error_recovery_enabled = true;
    
    LOG_INFO("Error handler initialized");
    return handler;
}

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
                        const char *file_name, int line_num) {
    if (!handler || !handler->error_stats) {
        return;
    }
    
    // 创建错误记录
    ErrorRecord_t *record = (ErrorRecord_t *)malloc(sizeof(ErrorRecord_t));
    if (!record) {
        LOG_ERROR("Failed to allocate error record");
        return;
    }
    
    memset(record, 0, sizeof(ErrorRecord_t));
    record->error_id = ++handler->error_id_counter;
    record->error_type = error_type;
    record->error_level = error_level;
    record->error_code = error_code;
    record->error_msg = error_msg;
    record->module_name = module_name;
    record->file_name = file_name;
    record->line_num = line_num;
    record->timestamp = time(NULL);
    
    // 更新错误统计
    handler->error_stats->total_errors++;
    if (error_type < ERROR_TYPE_MAX) {
        handler->error_stats->type_counts[error_type]++;
    }
    if (error_level < ERROR_LEVEL_MAX) {
        handler->error_stats->level_counts[error_level]++;
    }
    handler->error_stats->last_error_time = record->timestamp;
    
    // 添加到最近错误记录列表
    if (handler->error_stats->recent_error_count >= handler->error_stats->max_recent_errors) {
        // 删除最旧的错误记录
        ErrorRecord_t *oldest = handler->error_stats->recent_errors;
        if (oldest) {
            handler->error_stats->recent_errors = oldest->next;
            free(oldest);
            handler->error_stats->recent_error_count--;
        }
    }
    
    record->next = handler->error_stats->recent_errors;
    handler->error_stats->recent_errors = record;
    handler->error_stats->recent_error_count++;
    
    // 根据错误级别输出日志
    switch (error_level) {
        case ERROR_LEVEL_INFO:
            LOG_INFO("Error [%d]: %s (Module: %s, Code: %d, File: %s:%d)", 
                     record->error_id, error_msg, module_name, error_code, file_name, line_num);
            break;
        case ERROR_LEVEL_WARNING:
            LOG_WARN("Error [%d]: %s (Module: %s, Code: %d, File: %s:%d)", 
                     record->error_id, error_msg, module_name, error_code, file_name, line_num);
            break;
        case ERROR_LEVEL_ERROR:
            LOG_ERROR("Error [%d]: %s (Module: %s, Code: %d, File: %s:%d)", 
                     record->error_id, error_msg, module_name, error_code, file_name, line_num);
            break;
        case ERROR_LEVEL_FATAL:
            LOG_FATAL("Error [%d]: %s (Module: %s, Code: %d, File: %s:%d)", 
                     record->error_id, error_msg, module_name, error_code, file_name, line_num);
            break;
        default:
            break;
    }
}

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
                         const char *module_name) {
    if (!handler || !handler->error_recovery_enabled) {
        return false;
    }
    
    // 根据错误类型和级别执行不同的恢复策略
    switch (error_type) {
        case ERROR_TYPE_HARDWARE:
            // 硬件错误恢复策略
            if (error_level == ERROR_LEVEL_ERROR) {
                LOG_INFO("Attempting hardware error recovery for module: %s", module_name);
                // 尝试重置硬件
                if (strcmp(module_name, "peripheral") == 0) {
                    // 重置外设
                    peripheral_reset();
                } else if (strcmp(module_name, "bluetooth") == 0) {
                    // 重置蓝牙模块
                    bluetooth_reset();
                }
                return true;
            }
            break;
        
        case ERROR_TYPE_SOFTWARE:
            // 软件错误恢复策略
            if (error_level == ERROR_LEVEL_ERROR) {
                LOG_INFO("Attempting software error recovery for module: %s", module_name);
                // 尝试重启模块
                if (strcmp(module_name, "audio_core") == 0) {
                    // 重启音频核心模块
                    audio_core_deinit();
                    // 这里简化处理，实际应从配置管理器获取配置
                    void *cfg = NULL;
                    audio_core_init(cfg);
                }
                return true;
            }
            break;
        
        case ERROR_TYPE_NETWORK:
            // 网络错误恢复策略
            if (error_level <= ERROR_LEVEL_ERROR) {
                LOG_INFO("Attempting network error recovery for module: %s", module_name);
                // 尝试重新连接网络
                if (strcmp(module_name, "wifi_media") == 0) {
                    // 重新连接WiFi
                    wifi_media_reconnect();
                }
                return true;
            }
            break;
        
        case ERROR_TYPE_RESOURCE:
            // 资源错误恢复策略
            if (error_level <= ERROR_LEVEL_ERROR) {
                LOG_INFO("Attempting resource error recovery for module: %s", module_name);
                // 尝试释放资源
                resource_release_unused();
                return true;
            }
            break;
        
        case ERROR_TYPE_CONFIG:
            // 配置错误恢复策略
            if (error_level <= ERROR_LEVEL_ERROR) {
                LOG_INFO("Attempting config error recovery for module: %s", module_name);
                // 尝试加载默认配置
                config_manager_reload(NULL);
                return true;
            }
            break;
        
        default:
            break;
    }
    
    return false;
}

/**
 * @brief 输出错误统计信息
 * @details 输出错误统计和最近的错误记录
 * @param handler 错误处理器指针
 */
void error_handler_print_stats(ErrorHandler_t *handler) {
    if (!handler || !handler->error_stats) {
        return;
    }
    
    ErrorStats_t *stats = handler->error_stats;
    
    LOG_INFO("=== Error Statistics ===");
    LOG_INFO("Total errors: %d", stats->total_errors);
    LOG_INFO("Error type counts:");
    LOG_INFO("  Hardware: %d", stats->type_counts[ERROR_TYPE_HARDWARE]);
    LOG_INFO("  Software: %d", stats->type_counts[ERROR_TYPE_SOFTWARE]);
    LOG_INFO("  Network: %d", stats->type_counts[ERROR_TYPE_NETWORK]);
    LOG_INFO("  Resource: %d", stats->type_counts[ERROR_TYPE_RESOURCE]);
    LOG_INFO("  Config: %d", stats->type_counts[ERROR_TYPE_CONFIG]);
    
    LOG_INFO("Error level counts:");
    LOG_INFO("  Info: %d", stats->level_counts[ERROR_LEVEL_INFO]);
    LOG_INFO("  Warning: %d", stats->level_counts[ERROR_LEVEL_WARNING]);
    LOG_INFO("  Error: %d", stats->level_counts[ERROR_LEVEL_ERROR]);
    LOG_INFO("  Fatal: %d", stats->level_counts[ERROR_LEVEL_FATAL]);
    
    LOG_INFO("Last error time: %s", ctime((time_t *)&stats->last_error_time));
    
    if (stats->recent_error_count > 0) {
        LOG_INFO("Recent errors (%d/%d):", stats->recent_error_count, stats->max_recent_errors);
        ErrorRecord_t *curr = stats->recent_errors;
        int count = 0;
        while (curr && count < 10) { // 最多显示10个最近的错误
            LOG_INFO("  [%d] %s (Module: %s, Level: %d, Code: %d)", 
                     curr->error_id, curr->error_msg, curr->module_name, 
                     curr->error_level, curr->error_code);
            curr = curr->next;
            count++;
        }
    }
    
    LOG_INFO("=======================");
}

/**
 * @brief 反初始化错误处理器
 * @details 反初始化错误处理器，释放资源
 * @param handler 错误处理器指针
 */
void error_handler_deinit(ErrorHandler_t *handler) {
    if (!handler) {
        return;
    }
    
    // 打印错误统计
    error_handler_print_stats(handler);
    
    // 释放最近错误记录
    if (handler->error_stats) {
        ErrorRecord_t *curr = handler->error_stats->recent_errors;
        while (curr) {
            ErrorRecord_t *next = curr->next;
            free(curr);
            curr = next;
        }
        free(handler->error_stats);
    }
    
    // 释放错误处理器
    free(handler);
    LOG_INFO("Error handler deinitialized");
}
