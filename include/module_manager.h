#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

#include "module_interface.h"

// 初始化模块管理器
int module_manager_init(size_t initial_capacity);

// 加载模块
ModuleError module_manager_load(const char* path, void* config);

// 卸载模块
ModuleError module_manager_unload(const char* module_id);

// 获取模块接口
ModuleInterface* module_manager_get_module(const char* module_id);

// 获取指定类型的所有模块
size_t module_manager_get_modules_by_type(ModuleType type, ModuleInterface** out_modules, size_t max_count);

// 清理模块管理器
void module_manager_cleanup();

// 预加载常用模块
int module_manager_preload_common_modules();

// 性能优化：启用模块缓存
void module_manager_enable_caching(bool enable);

// 性能优化：设置模块加载优先级
int module_manager_set_priority(const char* module_id, int priority);

#endif // MODULE_MANAGER_H