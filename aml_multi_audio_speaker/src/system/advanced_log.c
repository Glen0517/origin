#include "log/aml_log.h"
#include "common_def.h"

/******************************************************************************************
 * 【高级日志系统实现】- 利用aml_log.h
 ******************************************************************************************/

// 定义各个模块的日志分类
AML_LOG_DEFINE(audio_log);      // 音频模块日志
AML_LOG_DEFINE(bt_log);         // 蓝牙模块日志
AML_LOG_DEFINE(network_log);    // 网络模块日志
AML_LOG_DEFINE(peripheral_log); // 外设模块日志
AML_LOG_DEFINE(storage_log);    // 存储模块日志

// 设置默认日志分类
#define AML_LOG_DEFAULT AML_LOG_GET_CAT(audio_log)

/******************************************************************************************
 * 【高级日志系统初始化】- 初始化所有日志分类
 ******************************************************************************************/
int advanced_log_init(void) {
    // 初始化默认日志分类
    aml_log_set_from_string("default:3"); // 默认INFO级别
    
    // 初始化各个模块的日志分类
    aml_log_set_from_string("audio_log:3");
    aml_log_set_from_string("bt_log:3");
    aml_log_set_from_string("network_log:3");
    aml_log_set_from_string("peripheral_log:3");
    aml_log_set_from_string("storage_log:3");
    
    // 设置日志输出文件
    FILE *log_file = fopen("./log/advanced.log", "a+");
    if (log_file) {
        aml_log_set_output_file(log_file);
    }
    
    // 输出初始化信息
    AML_LOGI("Advanced log system initialized");
    AML_LOGCATI(audio_log, "Audio module log initialized");
    AML_LOGCATI(bt_log, "Bluetooth module log initialized");
    AML_LOGCATI(network_log, "Network module log initialized");
    AML_LOGCATI(peripheral_log, "Peripheral module log initialized");
    AML_LOGCATI(storage_log, "Storage module log initialized");
    
    return SUCCESS;
}

/******************************************************************************************
 * 【高级日志系统设置】- 设置日志级别
 ******************************************************************************************/
int advanced_log_set_level(const char *category, int level) {
    // 将日志级别转换为字符串
    char level_str[2] = {0};
    snprintf(level_str, sizeof(level_str), "%d", level);
    
    // 构建日志配置字符串
    char config[64] = {0};
    snprintf(config, sizeof(config), "%s:%s", category, level_str);
    
    // 设置日志级别
    aml_log_set_from_string(config);
    
    // 输出设置信息
    AML_LOGI("Set log level for %s to %d", category, level);
    
    return SUCCESS;
}

/******************************************************************************************
 * 【高级日志系统示例】- 各个模块的日志使用示例
 ******************************************************************************************/

// 音频模块日志示例
void audio_log_example(void) {
    AML_LOGCATD(audio_log, "Audio module debug message");
    AML_LOGCATI(audio_log, "Audio module info message");
    AML_LOGCATW(audio_log, "Audio module warning message");
    AML_LOGCATE(audio_log, "Audio module error message");
}

// 蓝牙模块日志示例
void bt_log_example(void) {
    AML_LOGCATD(bt_log, "Bluetooth module debug message");
    AML_LOGCATI(bt_log, "Bluetooth module info message");
    AML_LOGCATW(bt_log, "Bluetooth module warning message");
    AML_LOGCATE(bt_log, "Bluetooth module error message");
}

// 网络模块日志示例
void network_log_example(void) {
    AML_LOGCATD(network_log, "Network module debug message");
    AML_LOGCATI(network_log, "Network module info message");
    AML_LOGCATW(network_log, "Network module warning message");
    AML_LOGCATE(network_log, "Network module error message");
}

// 外设模块日志示例
void peripheral_log_example(void) {
    AML_LOGCATD(peripheral_log, "Peripheral module debug message");
    AML_LOGCATI(peripheral_log, "Peripheral module info message");
    AML_LOGCATW(peripheral_log, "Peripheral module warning message");
    AML_LOGCATE(peripheral_log, "Peripheral module error message");
}

// 存储模块日志示例
void storage_log_example(void) {
    AML_LOGCATD(storage_log, "Storage module debug message");
    AML_LOGCATI(storage_log, "Storage module info message");
    AML_LOGCATW(storage_log, "Storage module warning message");
    AML_LOGCATE(storage_log, "Storage module error message");
}
