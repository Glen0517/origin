#include "logger.h"
#include "common_def.h"

/******************************************************************************************
 * 【高级日志系统实现】- 作为log.c的补充，提供模块级日志分类功能
 ******************************************************************************************/

/******************************************************************************************
 * 【高级日志系统初始化】- 初始化所有模块日志分类
 ******************************************************************************************/
int advanced_log_init(void) {
    // 初始化各个模块的日志分类
    aml_log_set_from_string("audio_log:3");     // 音频模块日志
    aml_log_set_from_string("bt_log:3");        // 蓝牙模块日志
    aml_log_set_from_string("network_log:3");   // 网络模块日志
    aml_log_set_from_string("peripheral_log:3"); // 外设模块日志
    aml_log_set_from_string("storage_log:3");    // 存储模块日志
    
    // 输出初始化信息
    LOG_INFO("Advanced log system initialized with module categories");
    
    return SUCCESS;
}

/******************************************************************************************
 * 【高级日志系统设置】- 设置指定模块的日志级别
 ******************************************************************************************/
int advanced_log_set_level(const char *category, int level) {
    if (!category) {
        LOG_ERROR("Invalid category parameter");
        return INVALID_PARAM;
    }
    
    // 将日志级别转换为字符串
    char level_str[2] = {0};
    snprintf(level_str, sizeof(level_str), "%d", level);
    
    // 构建日志配置字符串
    char config[64] = {0};
    snprintf(config, sizeof(config), "%s:%s", category, level_str);
    
    // 设置日志级别
    aml_log_set_from_string(config);
    
    // 输出设置信息
    LOG_INFO("Set log level for %s to %d", category, level);
    
    return SUCCESS;
}

/******************************************************************************************
 * 【高级日志系统扩展】- 提供模块级日志分类管理
 ******************************************************************************************/

/**
 * @brief  获取模块日志分类
 * @details 获取指定模块的日志分类，用于模块级日志输出
 * @param  module_name 模块名称
 * @return 日志分类字符串
 */
const char* advanced_log_get_category(const char *module_name) {
    if (!module_name) {
        return "default";
    }
    
    // 根据模块名称返回对应的日志分类
    if (strcmp(module_name, "audio") == 0) {
        return "audio_log";
    } else if (strcmp(module_name, "bluetooth") == 0) {
        return "bt_log";
    } else if (strcmp(module_name, "network") == 0) {
        return "network_log";
    } else if (strcmp(module_name, "peripheral") == 0) {
        return "peripheral_log";
    } else if (strcmp(module_name, "storage") == 0) {
        return "storage_log";
    }
    
    return "default";
}

