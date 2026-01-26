/**
 * @file config.h
 * @brief 配置管理模块接口
 * @details 提供配置文件管理、运行时配置更新和配置验证功能
 * @author AML Audio Team
 * @date 2026-01-26
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>
#include <stdbool.h>
#include "common_def.h"

/******************************************************************************************
 * 配置项类型定义
 ******************************************************************************************/
typedef enum {
    CONFIG_TYPE_INT,    // 整数类型
    CONFIG_TYPE_BOOL,   // 布尔类型
    CONFIG_TYPE_STRING, // 字符串类型
    CONFIG_TYPE_FLOAT,  // 浮点数类型
    CONFIG_TYPE_MAX
} ConfigType_e;

/******************************************************************************************
 * 配置项结构体
 ******************************************************************************************/
typedef struct {
    const char *key;       // 配置项键名
    ConfigType_e type;     // 配置项类型
    void *value;           // 配置项值指针
    const char *desc;      // 配置项描述
    bool read_only;        // 是否只读
    // 配置验证回调函数
    bool (*validator)(const void *value);
} ConfigItem_t;

/******************************************************************************************
 * 配置文件类型
 ******************************************************************************************/
typedef enum {
    CONFIG_FILE_SYSTEM,    // 系统配置文件
    CONFIG_FILE_AUDIO,     // 音频配置文件
    CONFIG_FILE_NETWORK,   // 网络配置文件
    CONFIG_FILE_PRODUCT,   // 产品配置文件
    CONFIG_FILE_MAX
} ConfigFileType_e;

/******************************************************************************************
 * 对外暴露接口 - 配置管理所有功能
 ******************************************************************************************/

/**
 * @brief 配置模块初始化
 * @return SUCCESS/FAILURE
 */
int config_init(void);

/**
 * @brief 配置模块反初始化
 * @return SUCCESS/FAILURE
 */
int config_deinit(void);

/**
 * @brief 加载配置文件
 * @param file_type 配置文件类型
 * @param file_path 配置文件路径
 * @return SUCCESS/FAILURE
 */
int config_load(ConfigFileType_e file_type, const char *file_path);

/**
 * @brief 保存配置到文件
 * @param file_type 配置文件类型
 * @param file_path 配置文件路径
 * @return SUCCESS/FAILURE
 */
int config_save(ConfigFileType_e file_type, const char *file_path);

/**
 * @brief 获取配置项值
 * @param key 配置项键名
 * @param value 输出值指针
 * @param type 配置项类型
 * @return SUCCESS/FAILURE
 */
int config_get(const char *key, void *value, ConfigType_e type);

/**
 * @brief 设置配置项值
 * @param key 配置项键名
 * @param value 配置项值
 * @param type 配置项类型
 * @return SUCCESS/FAILURE/INVALID_PARAM
 */
int config_set(const char *key, const void *value, ConfigType_e type);

/**
 * @brief 注册配置项
 * @param item 配置项结构体指针
 * @return SUCCESS/FAILURE
 */
int config_register_item(ConfigItem_t *item);

/**
 * @brief 取消注册配置项
 * @param key 配置项键名
 * @return SUCCESS/FAILURE
 */
int config_unregister_item(const char *key);

/**
 * @brief 验证配置项值
 * @param key 配置项键名
 * @param value 配置项值
 * @return true-验证通过，false-验证失败
 */
bool config_validate(const char *key, const void *value);

/**
 * @brief 应用配置变更
 * @return SUCCESS/FAILURE
 */
int config_apply_changes(void);

/**
 * @brief 重置配置到默认值
 * @return SUCCESS/FAILURE
 */
int config_reset_to_default(void);

/**
 * @brief 获取配置文件路径
 * @param file_type 配置文件类型
 * @return 配置文件路径
 */
const char *config_get_file_path(ConfigFileType_e file_type);

#endif // __CONFIG_H__
