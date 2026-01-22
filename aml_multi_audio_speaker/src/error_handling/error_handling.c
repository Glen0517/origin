/**
 * @file error_handling.c
 * @brief 统一错误处理模块实现
 * @details 提供统一的错误处理接口，包括错误定义、错误处理和错误恢复机制
 * @author AML Audio Team
 * @date 2026-01-22
 */

#include "error_handling.h"
#include "event.h"  // 事件系统
#include <pthread.h>
#include <string.h>
#include <time.h>

/******************************************************************************************
 * 错误处理系统内部数据结构
 ******************************************************************************************/

/**
 * @brief 错误处理回调结构
 */
typedef struct {
    ErrorCallback_t callback;  // 错误处理回调函数
    ErrorRecovery_t recovery;  // 错误恢复函数
    bool is_valid;             // 是否有效
} ErrorHandler_t;

/**
 * @brief 错误记录结构
 */
typedef struct {
    ErrorInfo_t error_info;  // 错误信息
    int count;              // 错误计数
    int last_timestamp;     // 上次发生时间
} ErrorRecord_t;

/******************************************************************************************
 * 错误处理系统全局变量
 ******************************************************************************************/

// 错误处理回调表
static ErrorHandler_t g_error_handlers[ERROR_OTHER_MAX + 1] = {0};

// 错误记录
static ErrorRecord_t g_error_records[ERROR_OTHER_MAX + 1] = {0};

// 线程安全机制
static pthread_mutex_t g_error_mutex = PTHREAD_MUTEX_INITIALIZER;

// 错误处理模块初始化标志
static bool g_error_handling_init = false;

/******************************************************************************************
 * 错误处理系统内部函数
 ******************************************************************************************/

/**
 * @brief 获取错误类型对应的字符串
 * @param type 错误类型
 * @return 错误类型字符串
 */
static const char *error_type_to_string(ErrorType_e type) {
    switch (type) {
        // 系统级错误
        case ERROR_SYSTEM_INIT: return "SYSTEM_INIT";
        case ERROR_SYSTEM_RESOURCE: return "SYSTEM_RESOURCE";
        case ERROR_SYSTEM_MEMORY: return "SYSTEM_MEMORY";
        case ERROR_SYSTEM_TEMPERATURE: return "SYSTEM_TEMPERATURE";
        case ERROR_SYSTEM_VOLTAGE: return "SYSTEM_VOLTAGE";
        
        // 音频级错误
        case ERROR_AUDIO_HAL_INIT: return "AUDIO_HAL_INIT";
        case ERROR_AUDIO_HAL_CONFIG: return "AUDIO_HAL_CONFIG";
        case ERROR_AUDIO_DECODE_INIT: return "AUDIO_DECODE_INIT";
        case ERROR_AUDIO_MIXER_INIT: return "AUDIO_MIXER_INIT";
        case ERROR_AUDIO_BUFFER_INIT: return "AUDIO_BUFFER_INIT";
        case ERROR_AUDIO_PCM_PLAY: return "AUDIO_PCM_PLAY";
        case ERROR_AUDIO_VOLUME_SET: return "AUDIO_VOLUME_SET";
        
        // 蓝牙级错误
        case ERROR_BT_INIT: return "BT_INIT";
        case ERROR_BT_CONNECT: return "BT_CONNECT";
        case ERROR_BT_A2DP_STREAM: return "BT_A2DP_STREAM";
        case ERROR_BT_PAIRING: return "BT_PAIRING";
        case ERROR_BT_MESH: return "BT_MESH";
        
        // WiFi媒体级错误
        case ERROR_WIFI_INIT: return "WIFI_INIT";
        case ERROR_WIFI_CONNECT: return "WIFI_CONNECT";
        case ERROR_WIFI_DLNA: return "WIFI_DLNA";
        case ERROR_WIFI_AIRPLAY: return "WIFI_AIRPLAY";
        case ERROR_WIFI_SPOTIFY: return "WIFI_SPOTIFY";
        case ERROR_WIFI_GOOGLE_CAST: return "WIFI_GOOGLE_CAST";
        
        // 外设级错误
        case ERROR_PERIPHERAL_LED: return "PERIPHERAL_LED";
        case ERROR_PERIPHERAL_KEY_IR: return "PERIPHERAL_KEY_IR";
        case ERROR_PERIPHERAL_LCD: return "PERIPHERAL_LCD";
        
        // 存储级错误
        case ERROR_STORAGE_MOUNT: return "STORAGE_MOUNT";
        case ERROR_STORAGE_READ: return "STORAGE_READ";
        case ERROR_STORAGE_WRITE: return "STORAGE_WRITE";
        case ERROR_STORAGE_SCAN: return "STORAGE_SCAN";
        
        // 网络级错误
        case ERROR_NETWORK_CONNECT: return "NETWORK_CONNECT";
        case ERROR_NETWORK_TIMEOUT: return "NETWORK_TIMEOUT";
        case ERROR_NETWORK_QUALITY: return "NETWORK_QUALITY";
        
        // 其他错误
        case ERROR_OTHER_UNKNOWN: return "OTHER_UNKNOWN";
        
        default: return "UNKNOWN_ERROR";
    }
}

/**
 * @brief 获取错误级别对应的字符串
 * @param level 错误级别
 * @return 错误级别字符串
 */
static const char *error_level_to_string(ErrorLevel_e level) {
    switch (level) {
        case ERROR_LEVEL_INFO: return "INFO";
        case ERROR_LEVEL_WARNING: return "WARNING";
        case ERROR_LEVEL_ERROR: return "ERROR";
        case ERROR_LEVEL_CRITICAL: return "CRITICAL";
        case ERROR_LEVEL_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 记录错误信息
 * @param error_info 错误信息
 */
static void record_error(ErrorInfo_t *error_info) {
    if (!error_info) return;
    
    ErrorType_e type = error_info->type;
    
    // 更新错误记录
    g_error_records[type].error_info = *error_info;
    g_error_records[type].count++;
    g_error_records[type].last_timestamp = error_info->timestamp;
}

/**
 * @brief 记录错误统计
 * @param type 错误类型
 */
static void update_error_statistics(ErrorType_e type) {
    // 这里可以添加更复杂的错误统计逻辑
    // 例如：按模块统计、按时间段统计等
}

/******************************************************************************************
 * 错误处理系统对外接口实现
 ******************************************************************************************/

/**
 * @brief 错误处理模块初始化
 * @return SUCCESS/FAILURE
 */
int error_handling_init(void) {
    pthread_mutex_lock(&g_error_mutex);
    
    if (g_error_handling_init) {
        pthread_mutex_unlock(&g_error_mutex);
        LOG_INFO("Error handling module already initialized");
        return SUCCESS;
    }
    
    // 初始化错误处理回调表
    memset(g_error_handlers, 0, sizeof(g_error_handlers));
    
    // 初始化错误记录
    memset(g_error_records, 0, sizeof(g_error_records));
    
    g_error_handling_init = true;
    LOG_INFO("Error handling module initialized successfully");
    
    pthread_mutex_unlock(&g_error_mutex);
    return SUCCESS;
}

/**
 * @brief 错误处理模块反初始化
 * @return SUCCESS/FAILURE
 */
int error_handling_deinit(void) {
    pthread_mutex_lock(&g_error_mutex);
    
    if (!g_error_handling_init) {
        pthread_mutex_unlock(&g_error_mutex);
        LOG_INFO("Error handling module not initialized");
        return SUCCESS;
    }
    
    // 清理错误处理回调表
    memset(g_error_handlers, 0, sizeof(g_error_handlers));
    
    // 清理错误记录
    memset(g_error_records, 0, sizeof(g_error_records));
    
    g_error_handling_init = false;
    LOG_INFO("Error handling module deinitialized successfully");
    
    pthread_mutex_unlock(&g_error_mutex);
    return SUCCESS;
}

/**
 * @brief 报告错误
 * @param type 错误类型
 * @param level 错误级别
 * @param module 错误模块
 * @param message 错误消息
 * @param code 错误代码
 * @param data 错误数据
 * @return SUCCESS/FAILURE
 */
int error_report(ErrorType_e type, ErrorLevel_e level, const char *module, const char *message, int code, void *data) {
    if (!g_error_handling_init) {
        LOG_ERROR("Error handling module not initialized");
        return FAILURE;
    }
    
    if (type < 0 || type > ERROR_OTHER_MAX) {
        LOG_ERROR("Invalid error type: %d", type);
        return FAILURE;
    }
    
    pthread_mutex_lock(&g_error_mutex);
    
    // 构建错误信息
    ErrorInfo_t error_info = {
        .type = type,
        .level = level,
        .module = module,
        .message = message,
        .code = code,
        .data = data,
        .timestamp = (int)time(NULL)
    };
    
    // 记录错误
    record_error(&error_info);
    
    // 更新错误统计
    update_error_statistics(type);
    
    // 根据错误级别记录日志
    switch (level) {
        case ERROR_LEVEL_INFO:
            LOG_INFO("Error [%s] - Module: %s, Message: %s, Code: %d", 
                     error_type_to_string(type), module, message, code);
            break;
        case ERROR_LEVEL_WARNING:
            LOG_WARN("Error [%s] - Module: %s, Message: %s, Code: %d", 
                     error_type_to_string(type), module, message, code);
            break;
        case ERROR_LEVEL_ERROR:
            LOG_ERROR("Error [%s] - Module: %s, Message: %s, Code: %d", 
                      error_type_to_string(type), module, message, code);
            break;
        case ERROR_LEVEL_CRITICAL:
            LOG_ERROR("CRITICAL Error [%s] - Module: %s, Message: %s, Code: %d", 
                      error_type_to_string(type), module, message, code);
            break;
        case ERROR_LEVEL_FATAL:
            LOG_ERROR("FATAL Error [%s] - Module: %s, Message: %s, Code: %d", 
                      error_type_to_string(type), module, message, code);
            break;
    }
    
    // 调用错误处理回调函数
    if (g_error_handlers[type].is_valid && g_error_handlers[type].callback) {
        g_error_handlers[type].callback(&error_info);
    }
    
    // 尝试自动恢复错误
    if (level >= ERROR_LEVEL_ERROR && g_error_handlers[type].is_valid && g_error_handlers[type].recovery) {
        int ret = g_error_handlers[type].recovery(&error_info);
        if (ret == SUCCESS) {
            LOG_INFO("Error [%s] recovered successfully", error_type_to_string(type));
        } else {
            LOG_ERROR("Failed to recover error [%s]", error_type_to_string(type));
        }
    }
    
    // 发送错误事件
    event_notify(EVENT_ERROR_OCCURRED, (void *)&error_info);
    
    pthread_mutex_unlock(&g_error_mutex);
    return SUCCESS;
}

/**
 * @brief 注册错误处理回调函数
 * @param type 错误类型
 * @param callback 错误处理回调函数
 * @return SUCCESS/FAILURE
 */
int error_register_callback(ErrorType_e type, ErrorCallback_t callback) {
    if (!g_error_handling_init) {
        LOG_ERROR("Error handling module not initialized");
        return FAILURE;
    }
    
    if (type < 0 || type > ERROR_OTHER_MAX) {
        LOG_ERROR("Invalid error type: %d", type);
        return FAILURE;
    }
    
    pthread_mutex_lock(&g_error_mutex);
    
    g_error_handlers[type].callback = callback;
    g_error_handlers[type].is_valid = true;
    
    LOG_INFO("Registered callback for error type: %s", error_type_to_string(type));
    
    pthread_mutex_unlock(&g_error_mutex);
    return SUCCESS;
}

/**
 * @brief 注册错误恢复函数
 * @param type 错误类型
 * @param recovery 错误恢复函数
 * @return SUCCESS/FAILURE
 */
int error_register_recovery(ErrorType_e type, ErrorRecovery_t recovery) {
    if (!g_error_handling_init) {
        LOG_ERROR("Error handling module not initialized");
        return FAILURE;
    }
    
    if (type < 0 || type > ERROR_OTHER_MAX) {
        LOG_ERROR("Invalid error type: %d", type);
        return FAILURE;
    }
    
    pthread_mutex_lock(&g_error_mutex);
    
    g_error_handlers[type].recovery = recovery;
    g_error_handlers[type].is_valid = true;
    
    LOG_INFO("Registered recovery function for error type: %s", error_type_to_string(type));
    
    pthread_mutex_unlock(&g_error_mutex);
    return SUCCESS;
}

/**
 * @brief 尝试恢复错误
 * @param type 错误类型
 * @param error_info 错误信息
 * @return SUCCESS/FAILURE
 */
int error_recover(ErrorType_e type, ErrorInfo_t *error_info) {
    if (!g_error_handling_init) {
        LOG_ERROR("Error handling module not initialized");
        return FAILURE;
    }
    
    if (type < 0 || type > ERROR_OTHER_MAX) {
        LOG_ERROR("Invalid error type: %d", type);
        return FAILURE;
    }
    
    pthread_mutex_lock(&g_error_mutex);
    
    if (!g_error_handlers[type].is_valid || !g_error_handlers[type].recovery) {
        pthread_mutex_unlock(&g_error_mutex);
        LOG_WARN("No recovery function registered for error type: %s", error_type_to_string(type));
        return FAILURE;
    }
    
    int ret = g_error_handlers[type].recovery(error_info);
    
    pthread_mutex_unlock(&g_error_mutex);
    return ret;
}

/**
 * @brief 获取错误统计信息
 * @param type 错误类型
 * @return 错误发生次数
 */
int error_get_count(ErrorType_e type) {
    if (!g_error_handling_init) {
        LOG_ERROR("Error handling module not initialized");
        return 0;
    }
    
    if (type < 0 || type > ERROR_OTHER_MAX) {
        LOG_ERROR("Invalid error type: %d", type);
        return 0;
    }
    
    pthread_mutex_lock(&g_error_mutex);
    int count = g_error_records[type].count;
    pthread_mutex_unlock(&g_error_mutex);
    
    return count;
}

/**
 * @brief 获取最近的错误信息
 * @param type 错误类型
 * @param error_info 用于存储错误信息
 * @return SUCCESS/FAILURE
 */
int error_get_last(ErrorType_e type, ErrorInfo_t *error_info) {
    if (!g_error_handling_init) {
        LOG_ERROR("Error handling module not initialized");
        return FAILURE;
    }
    
    if (type < 0 || type > ERROR_OTHER_MAX) {
        LOG_ERROR("Invalid error type: %d", type);
        return FAILURE;
    }
    
    if (!error_info) {
        LOG_ERROR("Invalid error_info parameter");
        return FAILURE;
    }
    
    pthread_mutex_lock(&g_error_mutex);
    
    if (g_error_records[type].count == 0) {
        pthread_mutex_unlock(&g_error_mutex);
        LOG_WARN("No error record found for error type: %s", error_type_to_string(type));
        return FAILURE;
    }
    
    *error_info = g_error_records[type].error_info;
    
    pthread_mutex_unlock(&g_error_mutex);
    return SUCCESS;
}

/**
 * @brief 清除错误统计信息
 * @param type 错误类型，如果为0则清除所有错误统计
 * @return SUCCESS/FAILURE
 */
int error_clear_count(ErrorType_e type) {
    if (!g_error_handling_init) {
        LOG_ERROR("Error handling module not initialized");
        return FAILURE;
    }
    
    pthread_mutex_lock(&g_error_mutex);
    
    if (type == 0) {
        // 清除所有错误统计
        memset(g_error_records, 0, sizeof(g_error_records));
        LOG_INFO("Cleared all error statistics");
    } else if (type > 0 && type <= ERROR_OTHER_MAX) {
        // 清除指定类型的错误统计
        memset(&g_error_records[type], 0, sizeof(ErrorRecord_t));
        LOG_INFO("Cleared error statistics for type: %s", error_type_to_string(type));
    } else {
        pthread_mutex_unlock(&g_error_mutex);
        LOG_ERROR("Invalid error type: %d", type);
        return FAILURE;
    }
    
    pthread_mutex_unlock(&g_error_mutex);
    return SUCCESS;
}

/**
 * @brief 打印错误统计信息
 * @return SUCCESS/FAILURE
 */
int error_print_statistics(void) {
    if (!g_error_handling_init) {
        LOG_ERROR("Error handling module not initialized");
        return FAILURE;
    }
    
    pthread_mutex_lock(&g_error_mutex);
    
    LOG_INFO("Error Statistics:");
    
    int total_errors = 0;
    
    // 遍历所有错误类型，打印统计信息
    for (int i = 0; i <= ERROR_OTHER_MAX; i++) {
        if (g_error_records[i].count > 0) {
            LOG_INFO("  %-20s: %d times, Last: %s", 
                     error_type_to_string((ErrorType_e)i),
                     g_error_records[i].count,
                     ctime((time_t *)&g_error_records[i].last_timestamp));
            total_errors += g_error_records[i].count;
        }
    }
    
    LOG_INFO("Total errors: %d", total_errors);
    
    pthread_mutex_unlock(&g_error_mutex);
    return SUCCESS;
}
