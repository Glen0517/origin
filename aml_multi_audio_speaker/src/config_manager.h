/**
 * @file config_manager.h
 * @brief 配置文件管理器
 * @details 负责读取和解析配置文件，提供配置项访问
 * @author AML Audio Team
 * @date 2026-01-27
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

// 配置管理器结构体
typedef struct {
    char *config_file_path;
    void *config_data;
} ConfigManager_t;

/**
 * @brief 初始化配置管理器
 * @details 创建并初始化配置管理器，读取配置文件
 * @param config_file_path 配置文件路径
 * @return 配置管理器指针，失败返回NULL
 */
ConfigManager_t *config_manager_init(const char *config_file_path);

/**
 * @brief 反初始化配置管理器
 * @details 反初始化配置管理器，释放资源
 * @param manager 配置管理器指针
 */
void config_manager_deinit(ConfigManager_t *manager);

/**
 * @brief 获取整型配置项
 * @details 从配置管理器中获取整型配置项
 * @param manager 配置管理器指针
 * @param key 配置项键名
 * @param default_val 默认值
 * @return 配置项值，失败返回默认值
 */
int config_manager_get_int(ConfigManager_t *manager, const char *key, int default_val);

/**
 * @brief 获取布尔型配置项
 * @details 从配置管理器中获取布尔型配置项
 * @param manager 配置管理器指针
 * @param key 配置项键名
 * @param default_val 默认值
 * @return 配置项值，失败返回默认值
 */
bool config_manager_get_bool(ConfigManager_t *manager, const char *key, bool default_val);

/**
 * @brief 获取字符串配置项
 * @details 从配置管理器中获取字符串配置项
 * @param manager 配置管理器指针
 * @param key 配置项键名
 * @param default_val 默认值
 * @return 配置项值，失败返回默认值
 */
const char *config_manager_get_string(ConfigManager_t *manager, const char *key, const char *default_val);

/**
 * @brief 设置整型配置项
 * @details 设置整型配置项
 * @param manager 配置管理器指针
 * @param key 配置项键名
 * @param value 配置项值
 * @return 是否成功
 */
bool config_manager_set_int(ConfigManager_t *manager, const char *key, int value);

/**
 * @brief 设置布尔型配置项
 * @details 设置布尔型配置项
 * @param manager 配置管理器指针
 * @param key 配置项键名
 * @param value 配置项值
 * @return 是否成功
 */
bool config_manager_set_bool(ConfigManager_t *manager, const char *key, bool value);

/**
 * @brief 设置字符串配置项
 * @details 设置字符串配置项
 * @param manager 配置管理器指针
 * @param key 配置项键名
 * @param value 配置项值
 * @return 是否成功
 */
bool config_manager_set_string(ConfigManager_t *manager, const char *key, const char *value);

/**
 * @brief 保存配置到文件
 * @details 将配置保存到文件
 * @param manager 配置管理器指针
 * @return 是否成功
 */
bool config_manager_save(ConfigManager_t *manager);

#endif /* CONFIG_MANAGER_H */
