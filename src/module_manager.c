#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "module_interface.h"
#include "audio_sdk.h"

// 模块结构体扩展
typedef struct LoadedModule {
    ModuleInterface* interface;
    void* handle; // 动态库句柄
    char* path; // 模块路径
    bool active;
    pthread_mutex_t mutex;
} LoadedModule;

// 模块管理器
typedef struct {
    LoadedModule** modules;
    size_t count;
    size_t capacity;
    pthread_mutex_t mutex;
} ModuleManager;

// 静态模块管理器实例
static ModuleManager g_module_manager = {0};
static bool g_initialized = false;

// 初始化模块管理器
int module_manager_init(size_t initial_capacity) {
    if (g_initialized) return MODULE_ERROR_ALREADY_LOADED;

    pthread_mutex_init(&g_module_manager.mutex, NULL);
    g_module_manager.capacity = initial_capacity > 0 ? initial_capacity : 10;
    g_module_manager.modules = calloc(g_module_manager.capacity, sizeof(LoadedModule*));
    if (!g_module_manager.modules) return MODULE_ERROR_LOAD_FAILED;

    g_initialized = true;
    return MODULE_SUCCESS;
}

// 加载模块
ModuleError module_manager_load(const char* path, void* config) {
    if (!g_initialized) return MODULE_ERROR_INIT_FAILED;
    if (!path) return MODULE_ERROR_INVALID_FORMAT;

    // 打开动态库
    void* handle = dlopen(path, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "Failed to load module: %s\n", dlerror());
        return MODULE_ERROR_LOAD_FAILED;
    }

    // 获取模块入口点
    ModuleEntryPoint entry = (ModuleEntryPoint)dlsym(handle, "module_get_interface");
    if (!entry) {
        fprintf(stderr, "Failed to find module entry point: %s\n", dlerror());
        dlclose(handle);
        return MODULE_ERROR_INVALID_FORMAT;
    }

    // 获取模块接口
    ModuleInterface* module = entry();
    if (!module) {
        fprintf(stderr, "Module returned NULL interface\n");
        dlclose(handle);
        return MODULE_ERROR_INVALID_FORMAT;
    }

    // 初始化模块
    if (module->init && module->init(config) != 0) {
        fprintf(stderr, "Module initialization failed\n");
        dlclose(handle);
        return MODULE_ERROR_INIT_FAILED;
    }

    // 创建加载的模块结构
    LoadedModule* loaded_module = calloc(1, sizeof(LoadedModule));
    if (!loaded_module) {
        module->deinit();
        dlclose(handle);
        return MODULE_ERROR_LOAD_FAILED;
    }

    loaded_module->interface = module;
    loaded_module->handle = handle;
    loaded_module->path = strdup(path);
    loaded_module->active = true;
    pthread_mutex_init(&loaded_module->mutex, NULL);

    // 添加到模块管理器
    pthread_mutex_lock(&g_module_manager.mutex);

    // 检查容量
    if (g_module_manager.count >= g_module_manager.capacity) {
        // 扩展容量
        size_t new_capacity = g_module_manager.capacity * 2;
        LoadedModule** new_modules = realloc(g_module_manager.modules, new_capacity * sizeof(LoadedModule*));
        if (!new_modules) {
            pthread_mutex_unlock(&g_module_manager.mutex);
            free(loaded_module->path);
            pthread_mutex_destroy(&loaded_module->mutex);
            free(loaded_module);
            module->deinit();
            dlclose(handle);
            return MODULE_ERROR_LOAD_FAILED;
        }
        g_module_manager.modules = new_modules;
        g_module_manager.capacity = new_capacity;
    }

    g_module_manager.modules[g_module_manager.count++] = loaded_module;
    pthread_mutex_unlock(&g_module_manager.mutex);

    printf("Successfully loaded module: %s (%s)\n", module->metadata.name, module->metadata.id);
    return MODULE_SUCCESS;
}

// 卸载模块
ModuleError module_manager_unload(const char* module_id) {
    if (!g_initialized) return MODULE_ERROR_INIT_FAILED;
    if (!module_id) return MODULE_ERROR_INVALID_FORMAT;

    pthread_mutex_lock(&g_module_manager.mutex);

    // 查找模块
    for (size_t i = 0; i < g_module_manager.count; i++) {
        LoadedModule* module = g_module_manager.modules[i];
        if (strcmp(module->interface->metadata.id, module_id) == 0) {
            // 锁定模块
            pthread_mutex_lock(&module->mutex);

            // 调用模块清理函数
            if (module->interface->deinit) {
                module->interface->deinit();
            }

            // 关闭动态库
            dlclose(module->handle);

            // 释放资源
            free(module->path);
            pthread_mutex_unlock(&module->mutex);
            pthread_mutex_destroy(&module->mutex);
            free(module);

            // 从数组中移除
            g_module_manager.count--;
            if (i < g_module_manager.count) {
                g_module_manager.modules[i] = g_module_manager.modules[g_module_manager.count];
            }
            g_module_manager.modules[g_module_manager.count] = NULL;

            pthread_mutex_unlock(&g_module_manager.mutex);
            printf("Unloaded module: %s\n", module_id);
            return MODULE_SUCCESS;
        }
    }

    pthread_mutex_unlock(&g_module_manager.mutex);
    return MODULE_ERROR_LOAD_FAILED; // 未找到模块
}

// 获取模块接口
ModuleInterface* module_manager_get_module(const char* module_id) {
    if (!g_initialized || !module_id) return NULL;

    pthread_mutex_lock(&g_module_manager.mutex);

    for (size_t i = 0; i < g_module_manager.count; i++) {
        LoadedModule* module = g_module_manager.modules[i];
        if (strcmp(module->interface->metadata.id, module_id) == 0) {
            pthread_mutex_unlock(&g_module_manager.mutex);
            return module->interface;
        }
    }

    pthread_mutex_unlock(&g_module_manager.mutex);
    return NULL;
}

// 获取指定类型的所有模块
size_t module_manager_get_modules_by_type(ModuleType type, ModuleInterface** out_modules, size_t max_count) {
    if (!g_initialized || !out_modules || max_count == 0) return 0;

    size_t result_count = 0;
    pthread_mutex_lock(&g_module_manager.mutex);

    for (size_t i = 0; i < g_module_manager.count && result_count < max_count; i++) {
        LoadedModule* module = g_module_manager.modules[i];
        if (module->interface->metadata.type == type) {
            out_modules[result_count++] = module->interface;
        }
    }

    pthread_mutex_unlock(&g_module_manager.mutex);
    return result_count;
}

// 清理模块管理器
void module_manager_cleanup() {
    if (!g_initialized) return;

    pthread_mutex_lock(&g_module_manager.mutex);

    // 卸载所有模块
    for (size_t i = 0; i < g_module_manager.count; i++) {
        LoadedModule* module = g_module_manager.modules[i];
        if (module->interface->deinit) {
            module->interface->deinit();
        }
        dlclose(module->handle);
        free(module->path);
        pthread_mutex_destroy(&module->mutex);
        free(module);
    }

    free(g_module_manager.modules);
    g_module_manager.modules = NULL;
    g_module_manager.count = 0;
    g_module_manager.capacity = 0;

    pthread_mutex_unlock(&g_module_manager.mutex);
    pthread_mutex_destroy(&g_module_manager.mutex);

    g_initialized = false;
}

// 性能优化：预加载常用模块
int module_manager_preload_common_modules() {
    if (!g_initialized) return MODULE_ERROR_INIT_FAILED;

    ModuleError err;
    
    // 预加载ALSA输出模块
    err = module_manager_load("/usr/lib/audio_modules/alsa_output.so", NULL);
    if (err != MODULE_SUCCESS && err != MODULE_ERROR_ALREADY_LOADED) {
        fprintf(stderr, "Warning: Failed to preload ALSA output module: %d\n", err);
    }

    // 预加载系统日志模块
    err = module_manager_load("/usr/lib/audio_modules/system_log.so", NULL);
    if (err != MODULE_SUCCESS && err != MODULE_ERROR_ALREADY_LOADED) {
        fprintf(stderr, "Warning: Failed to preload system log module: %d\n", err);
    }

    return MODULE_SUCCESS;
}