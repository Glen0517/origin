#ifndef MODULE_INTERFACE_H
#define MODULE_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

// 模块类型枚举
typedef enum {
    MODULE_TYPE_AUDIO_INPUT,
    MODULE_TYPE_AUDIO_OUTPUT,
    MODULE_TYPE_AUDIO_PROCESSING,
    MODULE_TYPE_NETWORK_SERVICE,
    MODULE_TYPE_CONTROL,
    MODULE_TYPE_OTHER
} ModuleType;

// 音频格式结构
typedef struct {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bit_depth;
    uint32_t buffer_size;
} AudioFormat;

// 模块元数据
typedef struct {
    const char* id;
    const char* name;
    const char* description;
    const char* version;
    ModuleType type;
} ModuleMetadata;

// 模块接口结构体
typedef struct ModuleInterface {
    // 元数据
    ModuleMetadata metadata;

    // 初始化模块
    int (*init)(void* config);

    // 销毁模块
    void (*deinit)(void);

    // 处理音频数据 (输入/输出/处理模块)
    int (*process_audio)(const void* input, void* output, size_t size, AudioFormat* format);

    // 获取模块状态
    bool (*is_active)(void);

    // 设置模块参数
    int (*set_parameter)(const char* key, const char* value);

    // 获取模块参数
    const char* (*get_parameter)(const char* key);

    // 模块私有数据
    void* private_data;
} ModuleInterface;

// 模块入口函数定义
typedef ModuleInterface* (*ModuleEntryPoint)(void);

// 动态加载状态码
typedef enum {
    MODULE_SUCCESS = 0,
    MODULE_ERROR_LOAD_FAILED,
    MODULE_ERROR_INVALID_FORMAT,
    MODULE_ERROR_VERSION_MISMATCH,
    MODULE_ERROR_INIT_FAILED,
    MODULE_ERROR_ALREADY_LOADED
} ModuleError;

#endif // MODULE_INTERFACE_H