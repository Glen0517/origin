/**
 * @file error_handling.h
 * @brief 统一错误处理模块
 * @details 提供统一的错误处理接口，包括错误定义、错误处理和错误恢复机制
 * @author AML Audio Team
 * @date 2026-01-22
 */

#ifndef __ERROR_HANDLING_H__
#define __ERROR_HANDLING_H__

#include "common_def.h"
#include "logger.h"
#include "event.h"

// 错误聚合配置
#define ERROR_AGGREGATION_WINDOW_SECONDS 60  // 错误聚合窗口（秒）
#define ERROR_AGGREGATION_THRESHOLD 5       // 错误聚合阈值

// 错误监控配置
#define ERROR_MONITOR_INTERVAL_SECONDS 300   // 错误监控间隔（秒）
#define ERROR_RATE_THRESHOLD 10              // 错误率阈值（每秒）

// 错误聚合结构体
typedef struct {
    ErrorType_e type;
    ErrorLevel_e level;
    const char *module;
    int count;
    int first_timestamp;
    int last_timestamp;
} ErrorAggregation_t;

// 错误监控结构体
typedef struct {
    uint32_t total_errors;
    uint32_t errors_in_window;
    uint32_t error_rate;
    ErrorType_e most_frequent_error;
    uint32_t most_frequent_error_count;
    const char *most_error_prone_module;
    uint32_t module_error_count;
} ErrorMonitor_t;

/**
 * @brief 错误类型定义
 */
typedef enum {
    // 系统级错误
    ERROR_SYSTEM_BASE = 1000,
    ERROR_SYSTEM_INIT = ERROR_SYSTEM_BASE,         // 系统初始化错误
    ERROR_SYSTEM_RESOURCE,                        // 系统资源错误
    ERROR_SYSTEM_MEMORY,                          // 系统内存错误
    ERROR_SYSTEM_TEMPERATURE,                    // 系统温度错误
    ERROR_SYSTEM_VOLTAGE,                        // 系统电压错误
    ERROR_SYSTEM_MAX = ERROR_SYSTEM_BASE + 99,
    
    // 音频级错误
    ERROR_AUDIO_BASE = 2000,
    ERROR_AUDIO_HAL_INIT = ERROR_AUDIO_BASE,       // HAL初始化错误
    ERROR_AUDIO_HAL_CONFIG,                      // HAL配置错误
    ERROR_AUDIO_DECODE_INIT,                     // 解码初始化错误
    ERROR_AUDIO_MIXER_INIT,                      // 混音器初始化错误
    ERROR_AUDIO_BUFFER_INIT,                     // 缓冲区初始化错误
    ERROR_AUDIO_PCM_PLAY,                        // PCM播放错误
    ERROR_AUDIO_VOLUME_SET,                      // 音量设置错误
    ERROR_AUDIO_MAX = ERROR_AUDIO_BASE + 99,
    
    // 蓝牙级错误
    ERROR_BT_BASE = 3000,
    ERROR_BT_INIT = ERROR_BT_BASE,                // 蓝牙初始化错误
    ERROR_BT_CONNECT,                            // 蓝牙连接错误
    ERROR_BT_A2DP_STREAM,                        // A2DP音频流错误
    ERROR_BT_PAIRING,                            // 蓝牙配对错误
    ERROR_BT_MESH,                               // 蓝牙MESH错误
    ERROR_BT_MAX = ERROR_BT_BASE + 99,
    
    // WiFi媒体级错误
    ERROR_WIFI_BASE = 4000,
    ERROR_WIFI_INIT = ERROR_WIFI_BASE,            // WiFi初始化错误
    ERROR_WIFI_CONNECT,                           // WiFi连接错误
    ERROR_WIFI_DLNA,                             // DLNA错误
    ERROR_WIFI_AIRPLAY,                           // AirPlay错误
    ERROR_WIFI_SPOTIFY,                           // Spotify错误
    ERROR_WIFI_GOOGLE_CAST,                       // Google Cast错误
    ERROR_WIFI_MAX = ERROR_WIFI_BASE + 99,
    
    // 外设级错误
    ERROR_PERIPHERAL_BASE = 5000,
    ERROR_PERIPHERAL_LED,                         // LED控制错误
    ERROR_PERIPHERAL_KEY_IR,                      // 按键/IR错误
    ERROR_PERIPHERAL_LCD,                         // LCD显示错误
    ERROR_PERIPHERAL_MAX = ERROR_PERIPHERAL_BASE + 99,
    
    // 存储级错误
    ERROR_STORAGE_BASE = 6000,
    ERROR_STORAGE_MOUNT,                          // 存储挂载错误
    ERROR_STORAGE_READ,                           // 存储读取错误
    ERROR_STORAGE_WRITE,                          // 存储写入错误
    ERROR_STORAGE_SCAN,                           // 媒体扫描错误
    ERROR_STORAGE_MAX = ERROR_STORAGE_BASE + 99,
    
    // 网络级错误
    ERROR_NETWORK_BASE = 7000,
    ERROR_NETWORK_CONNECT,                        // 网络连接错误
    ERROR_NETWORK_TIMEOUT,                        // 网络超时错误
    ERROR_NETWORK_QUALITY,                        // 网络质量错误
    ERROR_NETWORK_MAX = ERROR_NETWORK_BASE + 99,
    
    // 其他错误
    ERROR_OTHER_BASE = 9000,
    ERROR_OTHER_UNKNOWN,                          // 未知错误
    ERROR_OTHER_MAX = ERROR_OTHER_BASE + 99,
} ErrorType_e;

/**
 * @brief 错误级别定义
 */
typedef enum {
    ERROR_LEVEL_INFO,         // 信息级错误
    ERROR_LEVEL_WARNING,      // 警告级错误
    ERROR_LEVEL_ERROR,        // 错误级错误
    ERROR_LEVEL_CRITICAL,     // 临界级错误
    ERROR_LEVEL_FATAL         // 致命级错误
} ErrorLevel_e;

/**
 * @brief 错误信息结构体
 */
typedef struct {
    ErrorType_e type;         // 错误类型
    ErrorLevel_e level;       // 错误级别
    const char *module;       // 错误模块
    const char *message;      // 错误消息
    int code;                 // 错误代码
    void *data;               // 错误数据
    int timestamp;            // 错误发生时间
} ErrorInfo_t;

/**
 * @brief 错误处理回调函数类型
 */
typedef void (*ErrorCallback_t)(ErrorInfo_t *error_info);

/**
 * @brief 错误恢复函数类型
 */
typedef int (*ErrorRecovery_t)(ErrorInfo_t *error_info);

/**
 * @brief 错误处理模块初始化
 * @return SUCCESS/FAILURE
 */
int error_handling_init(void);

/**
 * @brief 错误处理模块反初始化
 * @return SUCCESS/FAILURE
 */
int error_handling_deinit(void);

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
int error_report(ErrorType_e type, ErrorLevel_e level, const char *module, const char *message, int code, void *data);

/**
 * @brief 注册错误处理回调函数
 * @param type 错误类型
 * @param callback 错误处理回调函数
 * @return SUCCESS/FAILURE
 */
int error_register_callback(ErrorType_e type, ErrorCallback_t callback);

/**
 * @brief 注册错误恢复函数
 * @param type 错误类型
 * @param recovery 错误恢复函数
 * @return SUCCESS/FAILURE
 */
int error_register_recovery(ErrorType_e type, ErrorRecovery_t recovery);

/**
 * @brief 尝试恢复错误
 * @param type 错误类型
 * @param error_info 错误信息
 * @return SUCCESS/FAILURE
 */
int error_recover(ErrorType_e type, ErrorInfo_t *error_info);

/**
 * @brief 获取错误统计信息
 * @param type 错误类型
 * @return 错误发生次数
 */
int error_get_count(ErrorType_e type);

/**
 * @brief 获取最近的错误信息
 * @param type 错误类型
 * @param error_info 用于存储错误信息
 * @return SUCCESS/FAILURE
 */
int error_get_last(ErrorType_e type, ErrorInfo_t *error_info);

/**
 * @brief 清除错误统计信息
 * @param type 错误类型，如果为0则清除所有错误统计
 * @return SUCCESS/FAILURE
 */
int error_clear_count(ErrorType_e type);

/**
 * @brief 打印错误统计信息
 * @return SUCCESS/FAILURE
 */
int error_print_statistics(void);

/**
 * @brief 获取错误监控信息
 * @param monitor 错误监控信息指针
 * @return SUCCESS/FAILURE
 */
int error_get_monitor_info(ErrorMonitor_t *monitor);

/**
 * @brief 重置错误监控信息
 * @return SUCCESS/FAILURE
 */
int error_reset_monitor_info(void);

/**
 * @brief 检查错误率是否超过阈值
 * @param threshold 错误率阈值（每秒）
 * @return true表示超过阈值，false表示未超过
 */
bool error_check_rate_threshold(uint32_t threshold);

#endif /* __ERROR_HANDLING_H__ */
