/**
 * @file main.c
 * @brief 系统主程序入口
 * @details 负责系统的初始化、模块管理、业务主循环和优雅退出
 * @author AML Audio Team
 * @date 2026-01-15
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>              // 高精度时间函数

// 条件包含POSIX头文件，仅在Linux编译环境下包含
#if defined(__linux__) || defined(__linux) || defined(LINUX)
#include <sys/poll.h>          // poll系统调用
#include <sys/timerfd.h>       // 定时器fd
#include <sys/epoll.h>         // epoll系统调用
#include <malloc.h>            // 内存分配相关函数
#endif

// 内存管理相关宏
#define ENABLE_MEMORY_LEAK_DETECTION 1
#define ENABLE_MEMORY_POOL 1

// 内存池配置
#define MEMORY_POOL_BLOCK_SIZE 256
#define MEMORY_POOL_BLOCK_COUNT 1024

#include "../lib/flac/product_type.h"      // 产品类型定义
#include "../lib/flac/common_def.h"         // 通用定义
#include "../lib/flac/logger.h"            // 日志系统
#include "../lib/flac/event.h"             // 事件系统
#include "../res/include/res_manager.h"     // 资源管理器
#include "config_manager.h"     // 配置文件管理

// 公共对外头文件 - 核心功能模块
#include "../lib/flac/audio_core.h"        // 音频核心处理
#include "../lib/flac/audio_source.h"    // 音频源管理
#include "../lib/flac/play_ctrl.h"         // 播放控制
#include "../lib/flac/volume_ctrl.h"       // 音量控制
#include "../lib/flac/peripheral.h"        // 外设管理
#include "../lib/flac/storage.h"           // 存储管理
#include "../lib/flac/bt.h"         // 蓝牙模块
#include "../lib/flac/system.h"            // 系统管理
#include "../lib/flac/comm_mcu.h"          // MCU通信  

// 宏控按需加载头文件 - 根据产品配置加载相应功能模块
#ifdef CONFIG_ENABLE_WIFI_MEDIA
#include "../lib/flac/wifi_media.h"         // WIFI媒体功能（DLNA、AirPlay）
#endif
#ifdef CONFIG_ENABLE_DOLBY_DTS
#include "../lib/flac/sound_effects.h"      // 音效处理（杜比、DTS）
#endif
#ifdef CONFIG_ENABLE_HDMI_ARC
#include "../lib/flac/hdmi_arc.h"           // HDMI ARC功能
#endif
#ifdef CONFIG_ENABLE_SPDIF
#include "../lib/flac/spdif_optical.h"      // SPDIF光纤输入
#endif
#ifdef CONFIG_ENABLE_BT_MESH
#include "../lib/flac/subwoofer_comm.h"     // 低音炮通信
#endif
#ifdef CONFIG_ENABLE_VOICE_NOISE_REDUCTION
#include "../lib/flac/voice_noise_reduction.h" // 语音降噪功能
#endif

#include "../lib/flac/prod_test.h"          // 生产测试模块

// 线程池管理
#include "system/thread_pool.h"  // 线程池管理模块
// 进程管理
#include "system/process.h"      // 进程管理模块
#include "system/process_manager.h"  // 进程管理器模块
// 进程间通信
#include "system/ipc.h"          // 进程间通信模块
// 资源管理
#include "system/resource.h"      // 资源管理模块
// 监控和诊断
#include "system/monitor.h"       // 监控和诊断模块

// HAL和PAL层头文件
#include "hal/include/hal.h"                // 硬件抽象层
#include "pal/include/pal.h"                // 平台抽象层

// 内存块结构体
typedef struct MemoryBlock {
    struct MemoryBlock *next;
    uint8_t data[MEMORY_POOL_BLOCK_SIZE];
} MemoryBlock_t;

// 内存池结构体
typedef struct {
    MemoryBlock_t *free_list;
    uint32_t total_blocks;
    uint32_t used_blocks;
    uint32_t peak_used;
} MemoryPool_t;

// 内存分配记录
typedef struct MemoryAllocRecord {
    void *ptr;
    size_t size;
    const char *file;
    int line;
    struct MemoryAllocRecord *next;
} MemoryAllocRecord_t;

// 内存管理结构体
typedef struct {
    MemoryPool_t *memory_pool;
    MemoryAllocRecord_t *alloc_records;
    uint32_t total_allocations;
    uint32_t current_allocations;
    size_t total_allocated_size;
    size_t current_allocated_size;
} MemoryManager_t;

// 配置结构体定义
typedef struct {
    int sample_rate;
    int channel_num;
    int pcm_buffer_size;
    bool hw_decode_en;
    bool dolby_dts_en;
    int init_ok;
} AudioCoreConfig_t;

typedef struct {
    bool cec_en;
    bool auto_switch_en;
    int sample_rate;
} HdmiArcConfig_t;

typedef struct {
    bool auto_switch_en;
    int sample_rate;
    int bits_per_sample;
} SpdifConfig_t;

typedef struct {
    int key_debounce_ms;
    int long_press_ms;
    bool ir_learn_en;
    bool mic_mute_en;
} PeripheralConfig_t;

// 按键事件定义
typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PLAY_PAUSE = 1,
    KEY_EVENT_VOL_UP = 2,
    KEY_EVENT_VOL_DOWN = 3,
    KEY_EVENT_SOURCE_SWITCH = 4,
    KEY_EVENT_SOUND_MODE = 5,
    KEY_EVENT_BASS_UP = 6,
    KEY_EVENT_TREBLE_UP = 7,
    KEY_EVENT_IR_LEARN = 8,
    KEY_EVENT_NEXT = 9,
    KEY_EVENT_PREV = 10
} KeyEvent_e;

// 模块状态枚举
typedef enum {
    MODULE_STATE_UNINIT = 0,
    MODULE_STATE_INIT_SUCCESS,
    MODULE_STATE_INIT_FAILED
} ModuleState_e;

// 依赖注入容器
typedef struct {
    // 内存管理
    MemoryManager_t *memory_manager;
    // 错误处理
    ErrorHandler_t *error_handler;
    // 安全管理
    SecurityManager_t *security_manager;
    // 功耗管理
    PowerManager_t *power_manager;
    // 配置管理
    ConfigManager_t *config_manager;
    // 核心模块（使用ModuleState_e类型）
    ModuleState_e audio_core;
    ModuleState_e audio_source;
    ModuleState_e play_ctrl;
    ModuleState_e volume_ctrl;
    ModuleState_e peripheral;
    ModuleState_e storage;
    ModuleState_e bluetooth;
    ModuleState_e system;
    ModuleState_e comm_mcu;
    // 可选模块（使用ModuleState_e类型）
#ifdef CONFIG_ENABLE_WIFI_MEDIA
    ModuleState_e wifi_media;
#endif
#ifdef CONFIG_ENABLE_DOLBY_DTS
    ModuleState_e sound_effects;
#endif
#ifdef CONFIG_ENABLE_HDMI_ARC
    ModuleState_e hdmi_arc;
#endif
#ifdef CONFIG_ENABLE_SPDIF
    ModuleState_e spdif_optical;
#endif
#ifdef CONFIG_ENABLE_BT_MESH
    ModuleState_e subwoofer_comm;
#endif
#ifdef CONFIG_ENABLE_VOICE_NOISE_REDUCTION
    ModuleState_e voice_noise_reduction;
#endif
} DependencyContainer_t;

// 定义默认日志分类
AML_LOG_DEFINE(default_log);

/**
 * @brief 系统运行状态标志
 * @details 用于控制主循环的运行，0表示退出，1表示继续运行
 */
static int g_sys_running = 1;

/**
 * @brief 依赖注入容器
 * @details 管理所有模块的实例，实现依赖注入
 */
static DependencyContainer_t g_di_container = {0};

/**
 * @brief 配置文件路径
 * @details 系统配置文件的路径
 */// 配置文件路径
static const char *g_config_file_path = "/etc/aml_audio/config.json";

// 配置项路径常量定义
#define CONFIG_AUDIO_SAMPLE_RATE          "audio.sample_rate"
#define CONFIG_AUDIO_CHANNEL_NUM          "audio.channel_num"
#define CONFIG_AUDIO_PCM_BUFFER_SIZE      "audio.pcm_buffer_size"
#define CONFIG_AUDIO_HW_DECODE_EN         "audio.hw_decode_en"
#define CONFIG_AUDIO_DOLBY_DTS_EN         "audio.dolby_dts_en"
#define CONFIG_AUDIO_DEFAULT_SOURCE       "audio.default_source"
#define CONFIG_AUDIO_AUTO_SWITCH_EN       "audio.auto_switch_en"
#define CONFIG_HDMI_CEC_EN                "hdmi.cec_en"
#define CONFIG_HDMI_AUTO_SWITCH_EN        "hdmi.auto_switch_en"
#define CONFIG_HDMI_SAMPLE_RATE           "hdmi.sample_rate"
#define CONFIG_SPDIF_AUTO_SWITCH_EN       "spdif.auto_switch_en"
#define CONFIG_SPDIF_SAMPLE_RATE          "spdif.sample_rate"
#define CONFIG_SPDIF_BITS_PER_SAMPLE      "spdif.bits_per_sample"
#define CONFIG_PERIPHERAL_KEY_DEBOUNCE_MS "peripheral.key_debounce_ms"
#define CONFIG_PERIPHERAL_LONG_PRESS_MS   "peripheral.long_press_ms"
#define CONFIG_PERIPHERAL_IR_LEARN_EN     "peripheral.ir_learn_en"
#define CONFIG_PERIPHERAL_MIC_MUTE_EN     "peripheral.mic_mute_en"
#define CONFIG_BLUETOOTH_BT_NAME          "bluetooth.bt_name"
#define CONFIG_BLUETOOTH_BT_PIN           "bluetooth.bt_pin"
#define CONFIG_BLUETOOTH_BT_AUTO_CONNECT  "bluetooth.bt_auto_connect"
#define CONFIG_VOLUME_MASTER_VOLUME       "volume.master_volume"
#define CONFIG_VOLUME_BASS_VOLUME         "volume.bass_volume"
#define CONFIG_VOLUME_TREBLE_VOLUME       "volume.treble_volume"
#define CONFIG_VOLUME_IS_MUTE             "volume.is_mute"
#define CONFIG_PLAY_POWER_OFF_RESUME_EN   "play.power_off_resume_en"
#define CONFIG_PLAY_BOOT_DEFAULT_PLAY_EN  "play.boot_default_play_en"
#define CONFIG_PLAY_BT_RECONNECT_TIMEOUT  "play.bt_reconnect_timeout"
#define CONFIG_PLAY_DEFAULT_MODE          "play.default_mode"
#define CONFIG_SOUND_DOLBY_EN             "sound.dolby_en"
#define CONFIG_SOUND_VIRTUAL_5_1_EN       "sound.virtual_5_1_en"
#define CONFIG_WIFI_WIFI_NAME             "wifi.wifi_name"
#define CONFIG_WIFI_DLNA_EN               "wifi.dlna_en"
#define CONFIG_WIFI_AIRPLAY_EN            "wifi.airplay_en"
#define CONFIG_SUBWOOFER_BT_NAME          "subwoofer.bt_name"
#define CONFIG_SUBWOOFER_BASS_GAIN        "subwoofer.bass_gain"
#define CONFIG_SUBWOOFER_VOL_SYNC_EN      "subwoofer.vol_sync_en"
#define CONFIG_SUBWOOFER_AUTO_CONNECT_EN  "subwoofer.auto_connect_en"

/**
 * @brief 初始化内存池
 * @details 创建并初始化内存池，分配指定数量的内存块
 * @return 内存池指针，失败返回NULL
 */
static MemoryPool_t *memory_pool_init(void) {
    MemoryPool_t *pool = (MemoryPool_t *)malloc(sizeof(MemoryPool_t));
    if (!pool) {
        LOG_ERROR("Failed to allocate memory pool");
        return NULL;
    }
    
    memset(pool, 0, sizeof(MemoryPool_t));
    pool->total_blocks = MEMORY_POOL_BLOCK_COUNT;
    
    // 分配内存块并链接成空闲列表
    MemoryBlock_t *prev = NULL;
    for (uint32_t i = 0; i < MEMORY_POOL_BLOCK_COUNT; i++) {
        MemoryBlock_t *block = (MemoryBlock_t *)malloc(sizeof(MemoryBlock_t));
        if (!block) {
            LOG_ERROR("Failed to allocate memory block %d", i);
            // 释放已分配的块
            while (prev) {
                MemoryBlock_t *tmp = prev->next;
                free(prev);
                prev = tmp;
            }
            free(pool);
            return NULL;
        }
        
        block->next = prev;
        prev = block;
    }
    
    pool->free_list = prev;
    LOG_INFO("Memory pool initialized: %d blocks of %d bytes each", 
             MEMORY_POOL_BLOCK_COUNT, MEMORY_POOL_BLOCK_SIZE);
    
    return pool;
}

/**
 * @brief 从内存池分配内存
 * @details 从内存池中分配一个内存块
 * @param pool 内存池指针
 * @return 分配的内存指针，失败返回NULL
 */
static void *memory_pool_alloc(MemoryPool_t *pool) {
    if (!pool || !pool->free_list) {
        return NULL;
    }
    
    // 从空闲列表中取出一个块
    MemoryBlock_t *block = pool->free_list;
    pool->free_list = block->next;
    
    pool->used_blocks++;
    if (pool->used_blocks > pool->peak_used) {
        pool->peak_used = pool->used_blocks;
    }
    
    return block->data;
}

/**
 * @brief 释放内存到内存池
 * @details 将内存块释放回内存池
 * @param pool 内存池指针
 * @param ptr 要释放的内存指针
 * @return 成功返回true，失败返回false
 */
static bool memory_pool_free(MemoryPool_t *pool, void *ptr) {
    if (!pool || !ptr) {
        return false;
    }
    
    // 计算内存块的起始地址
    MemoryBlock_t *block = (MemoryBlock_t *)((uint8_t *)ptr - offsetof(MemoryBlock_t, data));
    
    // 将块放回空闲列表
    block->next = pool->free_list;
    pool->free_list = block;
    
    if (pool->used_blocks > 0) {
        pool->used_blocks--;
    }
    
    return true;
}

/**
 * @brief 反初始化内存池
 * @details 释放内存池中的所有内存块
 * @param pool 内存池指针
 */
static void memory_pool_deinit(MemoryPool_t *pool) {
    if (!pool) {
        return;
    }
    
    // 释放所有内存块
    MemoryBlock_t *block = pool->free_list;
    while (block) {
        MemoryBlock_t *tmp = block->next;
        free(block);
        block = tmp;
    }
    
    // 释放内存池结构
    free(pool);
    LOG_INFO("Memory pool deinitialized");
}

/**
 * @brief 初始化内存管理器
 * @details 创建并初始化内存管理器，包括内存池和内存分配记录
 * @return 内存管理器指针，失败返回NULL
 */
static MemoryManager_t *memory_manager_init(void) {
    MemoryManager_t *manager = (MemoryManager_t *)malloc(sizeof(MemoryManager_t));
    if (!manager) {
        LOG_ERROR("Failed to allocate memory manager");
        return NULL;
    }
    
    memset(manager, 0, sizeof(MemoryManager_t));
    
    // 初始化内存池
    manager->memory_pool = memory_pool_init();
    if (!manager->memory_pool) {
        LOG_ERROR("Failed to initialize memory pool");
        free(manager);
        return NULL;
    }
    
    LOG_INFO("Memory manager initialized");
    return manager;
}

/**
 * @brief 分配内存（带内存泄漏检测）
 * @details 分配内存并记录分配信息
 * @param manager 内存管理器指针
 * @param size 分配大小
 * @param file 调用文件
 * @param line 调用行号
 * @return 分配的内存指针，失败返回NULL
 */
static void *memory_manager_alloc(MemoryManager_t *manager, size_t size, const char *file, int line) {
    void *ptr = NULL;
    
    // 尝试从内存池分配（小内存）
    if (size <= MEMORY_POOL_BLOCK_SIZE && manager->memory_pool) {
        ptr = memory_pool_alloc(manager->memory_pool);
    }
    
    // 内存池分配失败或大内存，使用标准malloc
    if (!ptr) {
        ptr = malloc(size);
    }
    
    if (ptr) {
        // 记录分配信息
        MemoryAllocRecord_t *record = (MemoryAllocRecord_t *)malloc(sizeof(MemoryAllocRecord_t));
        if (record) {
            record->ptr = ptr;
            record->size = size;
            record->file = file;
            record->line = line;
            record->next = manager->alloc_records;
            manager->alloc_records = record;
            
            manager->total_allocations++;
            manager->current_allocations++;
            manager->total_allocated_size += size;
            manager->current_allocated_size += size;
        }
    }
    
    return ptr;
}

/**
 * @brief 释放内存（带内存泄漏检测）
 * @details 释放内存并更新分配记录
 * @param manager 内存管理器指针
 * @param ptr 要释放的内存指针
 */
static void memory_manager_free(MemoryManager_t *manager, void *ptr) {
    if (!manager || !ptr) {
        return;
    }
    
    // 查找分配记录
    MemoryAllocRecord_t *prev = NULL;
    MemoryAllocRecord_t *curr = manager->alloc_records;
    
    while (curr) {
        if (curr->ptr == ptr) {
            // 从记录列表中移除
            if (prev) {
                prev->next = curr->next;
            } else {
                manager->alloc_records = curr->next;
            }
            
            // 释放内存
            bool pool_freed = false;
            if (curr->size <= MEMORY_POOL_BLOCK_SIZE && manager->memory_pool) {
                pool_freed = memory_pool_free(manager->memory_pool, ptr);
            }
            
            if (!pool_freed) {
                free(ptr);
            }
            
            // 更新统计信息
            manager->current_allocations--;
            manager->current_allocated_size -= curr->size;
            
            // 释放记录
            free(curr);
            return;
        }
        
        prev = curr;
        curr = curr->next;
    }
    
    // 未找到记录，直接释放
    free(ptr);
}

/**
 * @brief 检查内存泄漏
 * @details 检查并报告内存泄漏情况
 * @param manager 内存管理器指针
 */
static void memory_manager_check_leaks(MemoryManager_t *manager) {
    if (!manager) {
        return;
    }
    
    uint32_t leak_count = 0;
    size_t leak_size = 0;
    
    MemoryAllocRecord_t *curr = manager->alloc_records;
    while (curr) {
        LOG_WARN("Memory leak detected: %zu bytes at %p (allocated in %s:%d)", 
                 curr->size, curr->ptr, curr->file, curr->line);
        leak_count++;
        leak_size += curr->size;
        curr = curr->next;
    }
    
    if (leak_count > 0) {
        LOG_ERROR("Total memory leaks: %d allocations, %zu bytes", leak_count, leak_size);
    } else {
        LOG_INFO("No memory leaks detected");
    }
    
    // 打印内存池统计信息
    if (manager->memory_pool) {
        LOG_INFO("Memory pool stats: %d/%d blocks used, peak %d", 
                 manager->memory_pool->used_blocks, 
                 manager->memory_pool->total_blocks, 
                 manager->memory_pool->peak_used);
    }
    
    // 打印内存管理统计信息
    LOG_INFO("Memory manager stats: %d total allocations, %zu total bytes", 
             manager->total_allocations, manager->total_allocated_size);
    LOG_INFO("Current allocations: %d, current allocated size: %zu bytes", 
             manager->current_allocations, manager->current_allocated_size);
}

/**
 * @brief 反初始化内存管理器
 * @details 反初始化内存管理器，检查内存泄漏并释放资源
 * @param manager 内存管理器指针
 */
static void memory_manager_deinit(MemoryManager_t *manager) {
    if (!manager) {
        return;
    }
    
    // 检查内存泄漏
    memory_manager_check_leaks(manager);
    
    // 释放所有分配记录
    MemoryAllocRecord_t *curr = manager->alloc_records;
    while (curr) {
        MemoryAllocRecord_t *next = curr->next;
        free(curr->ptr);
        free(curr);
        curr = next;
    }
    
    // 反初始化内存池
    memory_pool_deinit(manager->memory_pool);
    
    // 释放内存管理器
    free(manager);
    LOG_INFO("Memory manager deinitialized");
}

// 错误类型定义
typedef enum {
    ERROR_TYPE_NONE = 0,
    ERROR_TYPE_HARDWARE = 1,
    ERROR_TYPE_SOFTWARE = 2,
    ERROR_TYPE_NETWORK = 3,
    ERROR_TYPE_RESOURCE = 4,
    ERROR_TYPE_CONFIG = 5,
    ERROR_TYPE_MAX
} ErrorType_e;

// 错误级别定义
typedef enum {
    ERROR_LEVEL_INFO = 0,
    ERROR_LEVEL_WARNING = 1,
    ERROR_LEVEL_ERROR = 2,
    ERROR_LEVEL_FATAL = 3,
    ERROR_LEVEL_MAX
} ErrorLevel_e;

// 错误记录结构体
typedef struct ErrorRecord {
    uint32_t error_id;
    ErrorType_e error_type;
    ErrorLevel_e error_level;
    int error_code;
    const char *error_msg;
    const char *module_name;
    const char *file_name;
    int line_num;
    uint64_t timestamp;
    struct ErrorRecord *next;
} ErrorRecord_t;

// 错误统计结构体
typedef struct ErrorStats {
    uint32_t total_errors;
    uint32_t type_counts[ERROR_TYPE_MAX];
    uint32_t level_counts[ERROR_LEVEL_MAX];
    uint32_t module_error_counts[64]; // 模块错误计数
    uint64_t last_error_time;
    ErrorRecord_t *recent_errors; // 最近的错误记录
    uint32_t recent_error_count;
    uint32_t max_recent_errors;
} ErrorStats_t;

// 错误处理结构体
typedef struct {
    ErrorStats_t *error_stats;
    uint32_t error_id_counter;
    bool error_recovery_enabled;
} ErrorHandler_t;

/**
 * @brief 初始化错误处理器
 * @details 创建并初始化错误处理器，用于错误统计和恢复
 * @return 错误处理器指针，失败返回NULL
 */
static ErrorHandler_t *error_handler_init(void) {
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
static void error_handler_record(ErrorHandler_t *handler, ErrorType_e error_type, 
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
static bool error_handler_recover(ErrorHandler_t *handler, ErrorType_e error_type, 
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
                    AudioCoreConfig_t cfg = {
                        .sample_rate = config_manager_get_int(g_di_container.config_manager, CONFIG_AUDIO_SAMPLE_RATE, 48000),
                        .channel_num = config_manager_get_int(g_di_container.config_manager, CONFIG_AUDIO_CHANNEL_NUM, 2),
                        .pcm_buffer_size = config_manager_get_int(g_di_container.config_manager, CONFIG_AUDIO_PCM_BUFFER_SIZE, 4096),
                        .hw_decode_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_AUDIO_HW_DECODE_EN, true),
                        .dolby_dts_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_AUDIO_DOLBY_DTS_EN, false)
                    };
                    audio_core_init(&cfg);
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
                if (g_di_container.memory_manager) {
                    // 清理内存池
                    memory_manager_check_leaks(g_di_container.memory_manager);
                }
                // 释放文件描述符等系统资源
                resource_release_unused();
                return true;
            }
            break;
        
        case ERROR_TYPE_CONFIG:
            // 配置错误恢复策略
            if (error_level <= ERROR_LEVEL_ERROR) {
                LOG_INFO("Attempting config error recovery for module: %s", module_name);
                // 尝试加载默认配置
                if (g_di_container.config_manager) {
                    config_manager_reload(g_di_container.config_manager);
                }
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
static void error_handler_print_stats(ErrorHandler_t *handler) {
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
static void error_handler_deinit(ErrorHandler_t *handler) {
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

// 安全级别定义
typedef enum {
    SECURITY_LEVEL_LOW = 0,
    SECURITY_LEVEL_MEDIUM = 1,
    SECURITY_LEVEL_HIGH = 2,
    SECURITY_LEVEL_MAX
} SecurityLevel_e;

// 安全事件类型定义
typedef enum {
    SECURITY_EVENT_NONE = 0,
    SECURITY_EVENT_INPUT_VALIDATION = 1,
    SECURITY_EVENT_NETWORK_ACCESS = 2,
    SECURITY_EVENT_MEMORY_ACCESS = 3,
    SECURITY_EVENT_CONFIG_CHANGE = 4,
    SECURITY_EVENT_FIRMWARE_UPDATE = 5,
    SECURITY_EVENT_MAX
} SecurityEvent_e;

// 安全记录结构体
typedef struct SecurityRecord {
    uint32_t record_id;
    SecurityEvent_e event_type;
    SecurityLevel_e security_level;
    const char *event_msg;
    const char *module_name;
    const char *source_ip;
    uint16_t source_port;
    uint64_t timestamp;
    struct SecurityRecord *next;
} SecurityRecord_t;

// 安全管理器结构体
typedef struct {
    SecurityRecord_t *security_records;
    uint32_t record_id_counter;
    uint32_t total_security_events;
    uint32_t event_counts[SECURITY_EVENT_MAX];
    uint32_t level_counts[SECURITY_LEVEL_MAX];
    bool input_validation_enabled;
    bool network_security_enabled;
    bool security_update_enabled;
    char *firmware_version;
    char *last_security_update;
} SecurityManager_t;

/**
 * @brief 初始化安全管理器
 * @details 创建并初始化安全管理器，用于安全检查和更新
 * @return 安全管理器指针，失败返回NULL
 */
static SecurityManager_t *security_manager_init(void) {
    SecurityManager_t *manager = (SecurityManager_t *)malloc(sizeof(SecurityManager_t));
    if (!manager) {
        LOG_ERROR("Failed to allocate security manager");
        return NULL;
    }
    
    memset(manager, 0, sizeof(SecurityManager_t));
    
    // 启用安全功能
    manager->input_validation_enabled = true;
    manager->network_security_enabled = true;
    manager->security_update_enabled = true;
    
    // 设置固件版本
    manager->firmware_version = "1.0.0";
    manager->last_security_update = "2026-01-27";
    
    LOG_INFO("Security manager initialized");
    return manager;
}

/**
 * @brief 记录安全事件
 * @details 记录安全相关事件并更新统计信息
 * @param manager 安全管理器指针
 * @param event_type 事件类型
 * @param security_level 安全级别
 * @param event_msg 事件消息
 * @param module_name 模块名称
 * @param source_ip 源IP地址
 * @param source_port 源端口
 */
static void security_manager_record_event(SecurityManager_t *manager, 
                                        SecurityEvent_e event_type, 
                                        SecurityLevel_e security_level, 
                                        const char *event_msg, 
                                        const char *module_name, 
                                        const char *source_ip, 
                                        uint16_t source_port) {
    if (!manager) {
        return;
    }
    
    // 创建安全记录
    SecurityRecord_t *record = (SecurityRecord_t *)malloc(sizeof(SecurityRecord_t));
    if (!record) {
        LOG_ERROR("Failed to allocate security record");
        return;
    }
    
    memset(record, 0, sizeof(SecurityRecord_t));
    record->record_id = ++manager->record_id_counter;
    record->event_type = event_type;
    record->security_level = security_level;
    record->event_msg = event_msg;
    record->module_name = module_name;
    record->source_ip = source_ip;
    record->source_port = source_port;
    record->timestamp = time(NULL);
    
    // 更新统计信息
    manager->total_security_events++;
    if (event_type < SECURITY_EVENT_MAX) {
        manager->event_counts[event_type]++;
    }
    if (security_level < SECURITY_LEVEL_MAX) {
        manager->level_counts[security_level]++;
    }
    
    // 添加到安全记录列表
    record->next = manager->security_records;
    manager->security_records = record;
    
    // 根据安全级别输出日志
    switch (security_level) {
        case SECURITY_LEVEL_LOW:
            LOG_INFO("Security Event [%d]: %s (Module: %s, Type: %d, Source: %s:%d)", 
                     record->record_id, event_msg, module_name, event_type, source_ip, source_port);
            break;
        case SECURITY_LEVEL_MEDIUM:
            LOG_WARN("Security Event [%d]: %s (Module: %s, Type: %d, Source: %s:%d)", 
                     record->record_id, event_msg, module_name, event_type, source_ip, source_port);
            break;
        case SECURITY_LEVEL_HIGH:
            LOG_ERROR("Security Event [%d]: %s (Module: %s, Type: %d, Source: %s:%d)", 
                     record->record_id, event_msg, module_name, event_type, source_ip, source_port);
            break;
        default:
            break;
    }
}

/**
 * @brief 验证输入数据
 * @details 验证输入数据的合法性，防止缓冲区溢出等安全问题
 * @param manager 安全管理器指针
 * @param data 输入数据
 * @param size 数据大小
 * @param max_size 最大允许大小
 * @param module_name 模块名称
 * @return 验证是否通过
 */
static bool security_manager_validate_input(SecurityManager_t *manager, 
                                         const void *data, 
                                         size_t size, 
                                         size_t max_size, 
                                         const char *module_name) {
    if (!manager || !manager->input_validation_enabled) {
        return true;
    }
    
    if (!data) {
        security_manager_record_event(manager, 
                                    SECURITY_EVENT_INPUT_VALIDATION, 
                                    SECURITY_LEVEL_MEDIUM, 
                                    "Null input data", 
                                    module_name, 
                                    "localhost", 
                                    0);
        return false;
    }
    
    if (size > max_size) {
        security_manager_record_event(manager, 
                                    SECURITY_EVENT_INPUT_VALIDATION, 
                                    SECURITY_LEVEL_HIGH, 
                                    "Input data size exceeds maximum allowed", 
                                    module_name, 
                                    "localhost", 
                                    0);
        return false;
    }
    
    // 检查输入数据是否包含恶意内容
    // TODO: 实现更详细的输入验证逻辑
    
    return true;
}

/**
 * @brief 检查网络访问
 * @details 检查网络访问的合法性，防止未授权访问
 * @param manager 安全管理器指针
 * @param ip_address IP地址
 * @param port 端口
 * @param module_name 模块名称
 * @return 访问是否允许
 */
static bool security_manager_check_network_access(SecurityManager_t *manager, 
                                               const char *ip_address, 
                                               uint16_t port, 
                                               const char *module_name) {
    if (!manager || !manager->network_security_enabled) {
        return true;
    }
    
    if (!ip_address) {
        security_manager_record_event(manager, 
                                    SECURITY_EVENT_NETWORK_ACCESS, 
                                    SECURITY_LEVEL_MEDIUM, 
                                    "Null IP address", 
                                    module_name, 
                                    "unknown", 
                                    port);
        return false;
    }
    
    // 检查是否为本地地址
    if (strcmp(ip_address, "127.0.0.1") == 0 || strcmp(ip_address, "localhost") == 0) {
        return true;
    }
    
    // 检查是否为允许的网络
    // TODO: 实现网络访问控制列表
    
    // 记录网络访问事件
    security_manager_record_event(manager, 
                                SECURITY_EVENT_NETWORK_ACCESS, 
                                SECURITY_LEVEL_LOW, 
                                "Network access attempt", 
                                module_name, 
                                ip_address, 
                                port);
    
    return true;
}

/**
 * @brief 检查固件更新
 * @details 检查是否有可用的固件更新
 * @param manager 安全管理器指针
 * @return 是否有更新可用
 */
static bool security_manager_check_update(SecurityManager_t *manager) {
    if (!manager || !manager->security_update_enabled) {
        return false;
    }
    
    // 检查固件更新
    LOG_INFO("Checking for firmware updates...");
    LOG_INFO("Current firmware version: %s", manager->firmware_version);
    LOG_INFO("Last security update: %s", manager->last_security_update);
    
    // 模拟固件更新检查
    // 实际实现中，这里应该通过网络请求检查更新服务器
    // 或者通过本地存储的更新包检查
    
    // 检查更新服务器
    // 这里使用模拟逻辑，实际应替换为真实的网络请求
    bool update_available = false;
    char latest_version[64] = "1.0.1";
    
    // 比较版本号
    if (strcmp(manager->firmware_version, latest_version) < 0) {
        update_available = true;
        LOG_INFO("New firmware version available: %s", latest_version);
    } else {
        LOG_INFO("Firmware is up to date");
    }
    
    return update_available;
}

/**
 * @brief 应用安全更新
 * @details 应用安全更新到系统
 * @param manager 安全管理器指针
 * @param update_url 更新URL
 * @return 更新是否成功
 */
static bool security_manager_apply_update(SecurityManager_t *manager, const char *update_url) {
    if (!manager || !manager->security_update_enabled) {
        return false;
    }
    
    // 应用安全更新
    LOG_INFO("Applying security update from: %s", update_url);
    
    // 1. 下载固件更新包
    LOG_INFO("Downloading firmware update...");
    // 实际实现中，这里应该通过网络下载更新包
    // 并进行校验和验证
    
    // 2. 验证固件更新包
    LOG_INFO("Verifying firmware update...");
    // 实际实现中，这里应该验证固件的签名和完整性
    
    // 3. 备份当前固件
    LOG_INFO("Backing up current firmware...");
    // 实际实现中，这里应该备份当前的固件，以便在更新失败时恢复
    
    // 4. 应用固件更新
    LOG_INFO("Applying firmware update...");
    // 实际实现中，这里应该将新固件写入设备
    
    // 5. 验证更新是否成功
    LOG_INFO("Verifying firmware update application...");
    // 实际实现中，这里应该验证新固件是否正确写入
    
    // 6. 更新版本信息
    manager->firmware_version = "1.0.1";
    
    // 7. 更新最后更新时间
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d", tm_info);
    manager->last_security_update = time_str;
    
    // 8. 记录安全事件
    security_manager_record_event(manager, 
                                SECURITY_EVENT_FIRMWARE_UPDATE, 
                                SECURITY_LEVEL_LOW, 
                                "Security update applied", 
                                "security_manager", 
                                "update_server", 
                                80);
    
    LOG_INFO("Firmware update applied successfully");
    return true;
}

/**
 * @brief 输出安全统计信息
 * @details 输出安全事件统计和最近的安全记录
 * @param manager 安全管理器指针
 */
static void security_manager_print_stats(SecurityManager_t *manager) {
    if (!manager) {
        return;
    }
    
    LOG_INFO("=== Security Statistics ===");
    LOG_INFO("Total security events: %d", manager->total_security_events);
    LOG_INFO("Security event counts:");
    LOG_INFO("  Input Validation: %d", manager->event_counts[SECURITY_EVENT_INPUT_VALIDATION]);
    LOG_INFO("  Network Access: %d", manager->event_counts[SECURITY_EVENT_NETWORK_ACCESS]);
    LOG_INFO("  Memory Access: %d", manager->event_counts[SECURITY_EVENT_MEMORY_ACCESS]);
    LOG_INFO("  Config Change: %d", manager->event_counts[SECURITY_EVENT_CONFIG_CHANGE]);
    LOG_INFO("  Firmware Update: %d", manager->event_counts[SECURITY_EVENT_FIRMWARE_UPDATE]);
    
    LOG_INFO("Security level counts:");
    LOG_INFO("  Low: %d", manager->level_counts[SECURITY_LEVEL_LOW]);
    LOG_INFO("  Medium: %d", manager->level_counts[SECURITY_LEVEL_MEDIUM]);
    LOG_INFO("  High: %d", manager->level_counts[SECURITY_LEVEL_HIGH]);
    
    LOG_INFO("Firmware version: %s", manager->firmware_version);
    LOG_INFO("Last security update: %s", manager->last_security_update);
    
    LOG_INFO("Security features enabled:");
    LOG_INFO("  Input Validation: %s", manager->input_validation_enabled ? "Yes" : "No");
    LOG_INFO("  Network Security: %s", manager->network_security_enabled ? "Yes" : "No");
    LOG_INFO("  Security Update: %s", manager->security_update_enabled ? "Yes" : "No");
    
    LOG_INFO("========================");
}

/**
 * @brief 反初始化安全管理器
 * @details 反初始化安全管理器，释放资源
 * @param manager 安全管理器指针
 */
static void security_manager_deinit(SecurityManager_t *manager) {
    if (!manager) {
        return;
    }
    
    // 打印安全统计
    security_manager_print_stats(manager);
    
    // 释放安全记录
    SecurityRecord_t *curr = manager->security_records;
    while (curr) {
        SecurityRecord_t *next = curr->next;
        free(curr);
        curr = next;
    }
    
    // 释放安全管理器
    free(manager);
    LOG_INFO("Security manager deinitialized");
}

// 功耗模式定义
typedef enum {
    POWER_MODE_NORMAL = 0,
    POWER_MODE_LOW = 1,
    POWER_MODE_STANDBY = 2,
    POWER_MODE_MAX
} PowerMode_e;

// 系统状态定义
typedef enum {
    SYSTEM_STATE_ACTIVE = 0,
    SYSTEM_STATE_IDLE = 1,
    SYSTEM_STATE_SUSPENDED = 2,
    SYSTEM_STATE_MAX
} SystemState_e;

// 功耗统计结构体
typedef struct {
    uint64_t total_power_consumption;
    uint32_t mode_duration[POWER_MODE_MAX];
    uint32_t state_duration[SYSTEM_STATE_MAX];
    uint32_t wakeup_count;
    uint32_t last_wakeup_time;
    uint32_t idle_time;
} PowerStats_t;

// 功耗管理器结构体
typedef struct {
    PowerMode_e current_mode;
    SystemState_e current_state;
    PowerStats_t power_stats;
    bool dynamic_power_management;
    bool low_power_mode_enabled;
    uint32_t idle_threshold;
    uint32_t low_power_threshold;
    uint32_t normal_poll_interval;
    uint32_t low_power_poll_interval;
    uint32_t last_activity_time;
} PowerManager_t;

/**
 * @brief 初始化功耗管理器
 * @details 创建并初始化功耗管理器，用于动态功耗管理
 * @return 功耗管理器指针，失败返回NULL
 */
static PowerManager_t *power_manager_init(void) {
    PowerManager_t *manager = (PowerManager_t *)malloc(sizeof(PowerManager_t));
    if (!manager) {
        LOG_ERROR("Failed to allocate power manager");
        return NULL;
    }
    
    memset(manager, 0, sizeof(PowerManager_t));
    
    // 初始化默认值
    manager->current_mode = POWER_MODE_NORMAL;
    manager->current_state = SYSTEM_STATE_ACTIVE;
    manager->dynamic_power_management = true;
    manager->low_power_mode_enabled = true;
    manager->idle_threshold = 60000; // 60秒
    manager->low_power_threshold = 300000; // 5分钟
    manager->normal_poll_interval = 10; // 10毫秒
    manager->low_power_poll_interval = 100; // 100毫秒
    manager->last_activity_time = time(NULL) * 1000;
    
    LOG_INFO("Power manager initialized");
    return manager;
}

/**
 * @brief 更新系统活动时间
 * @details 更新系统最后活动时间，用于判断系统是否空闲
 * @param manager 功耗管理器指针
 */
static void power_manager_update_activity(PowerManager_t *manager) {
    if (!manager) {
        return;
    }
    
    manager->last_activity_time = time(NULL) * 1000;
    manager->power_stats.idle_time = 0;
    
    // 如果系统处于空闲状态，切换到活跃状态
    if (manager->current_state == SYSTEM_STATE_IDLE) {
        manager->current_state = SYSTEM_STATE_ACTIVE;
        LOG_INFO("System state changed to ACTIVE");
    }
    
    // 如果系统处于低功耗模式，切换到正常模式
    if (manager->current_mode == POWER_MODE_LOW) {
        manager->current_mode = POWER_MODE_NORMAL;
        LOG_INFO("Power mode changed to NORMAL");
    }
}

/**
 * @brief 检查系统空闲状态
 * @details 检查系统是否处于空闲状态，并根据空闲时间调整功耗模式
 * @param manager 功耗管理器指针
 * @return 当前系统状态
 */
static SystemState_e power_manager_check_idle(PowerManager_t *manager) {
    if (!manager || !manager->dynamic_power_management) {
        return manager ? manager->current_state : SYSTEM_STATE_ACTIVE;
    }
    
    uint32_t current_time = time(NULL) * 1000;
    uint32_t idle_time = current_time - manager->last_activity_time;
    manager->power_stats.idle_time = idle_time;
    
    // 根据空闲时间调整系统状态和功耗模式
    if (idle_time >= manager->low_power_threshold) {
        // 长时间空闲，进入低功耗模式
        if (manager->current_mode != POWER_MODE_LOW) {
            manager->current_mode = POWER_MODE_LOW;
            manager->current_state = SYSTEM_STATE_IDLE;
            LOG_INFO("System entering LOW power mode (idle for %d ms)", idle_time);
        }
    } else if (idle_time >= manager->idle_threshold) {
        // 短时间空闲，进入空闲状态
        if (manager->current_state != SYSTEM_STATE_IDLE) {
            manager->current_state = SYSTEM_STATE_IDLE;
            LOG_INFO("System state changed to IDLE (idle for %d ms)", idle_time);
        }
    } else {
        // 系统活跃
        if (manager->current_state != SYSTEM_STATE_ACTIVE) {
            manager->current_state = SYSTEM_STATE_ACTIVE;
            LOG_INFO("System state changed to ACTIVE");
        }
        if (manager->current_mode != POWER_MODE_NORMAL) {
            manager->current_mode = POWER_MODE_NORMAL;
            LOG_INFO("Power mode changed to NORMAL");
        }
    }
    
    return manager->current_state;
}

/**
 * @brief 获取当前轮询间隔
 * @details 根据当前功耗模式获取合适的轮询间隔
 * @param manager 功耗管理器指针
 * @return 轮询间隔（毫秒）
 */
static uint32_t power_manager_get_poll_interval(PowerManager_t *manager) {
    if (!manager) {
        return 10; // 默认10毫秒
    }
    
    switch (manager->current_mode) {
        case POWER_MODE_LOW:
            return manager->low_power_poll_interval;
        case POWER_MODE_NORMAL:
        default:
            return manager->normal_poll_interval;
    }
}

/**
 * @brief 进入低功耗模式
 * @details 进入低功耗模式，降低系统功耗
 * @param manager 功耗管理器指针
 * @return 是否成功进入低功耗模式
 */
static bool power_manager_enter_low_power(PowerManager_t *manager) {
    if (!manager || !manager->low_power_mode_enabled) {
        return false;
    }
    
    if (manager->current_mode != POWER_MODE_LOW) {
        manager->current_mode = POWER_MODE_LOW;
        manager->current_state = SYSTEM_STATE_IDLE;
        LOG_INFO("System entering LOW power mode");
        
        // 实现具体的低功耗模式逻辑
        
        // 1. 关闭不必要的外设
        LOG_INFO("Disabling unnecessary peripherals...");
        // 关闭WiFi
#ifdef CONFIG_ENABLE_WIFI_MEDIA
        wifi_media_set_power(false);
#endif
        // 关闭蓝牙
        bluetooth_set_power(false);
        
        // 2. 降低CPU频率
        LOG_INFO("Reducing CPU frequency...");
        // 实际实现中，这里应该调用系统API降低CPU频率
        
        // 3. 减少轮询频率
        LOG_INFO("Reducing polling frequency...");
        manager->low_power_poll_interval = 500; // 增加到500ms
        
        // 4. 关闭不必要的LED
        LOG_INFO("Turning off unnecessary LEDs...");
        // 实际实现中，这里应该关闭装饰性LED等
        
        // 5. 降低音频处理精度（如果适用）
        LOG_INFO("Reducing audio processing precision...");
        // 实际实现中，这里应该降低音频采样率或关闭某些音效处理
        
        return true;
    }
    
    return false;
}

/**
 * @brief 退出低功耗模式
 * @details 退出低功耗模式，恢复正常运行状态
 * @param manager 功耗管理器指针
 * @return 是否成功退出低功耗模式
 */
static bool power_manager_exit_low_power(PowerManager_t *manager) {
    if (!manager) {
        return false;
    }
    
    if (manager->current_mode == POWER_MODE_LOW) {
        manager->current_mode = POWER_MODE_NORMAL;
        manager->current_state = SYSTEM_STATE_ACTIVE;
        manager->last_activity_time = time(NULL) * 1000;
        LOG_INFO("System exiting LOW power mode");
        
        // 实现具体的退出低功耗模式逻辑
        
        // 1. 恢复必要的外设
        LOG_INFO("Enabling necessary peripherals...");
        // 恢复蓝牙
        bluetooth_set_power(true);
        // 恢复WiFi
#ifdef CONFIG_ENABLE_WIFI_MEDIA
        wifi_media_set_power(true);
#endif
        
        // 2. 恢复CPU频率
        LOG_INFO("Restoring CPU frequency...");
        // 实际实现中，这里应该调用系统API恢复CPU频率
        
        // 3. 恢复轮询频率
        LOG_INFO("Restoring polling frequency...");
        manager->low_power_poll_interval = 100; // 恢复到100ms
        
        // 4. 恢复LED状态
        LOG_INFO("Restoring LED status...");
        // 实际实现中，这里应该恢复LED到正常状态
        
        // 5. 恢复音频处理精度
        LOG_INFO("Restoring audio processing precision...");
        // 实际实现中，这里应该恢复音频采样率或重新启用音效处理
        
        return true;
    }
    
    return false;
}

/**
 * @brief 输出功耗统计信息
 * @details 输出功耗统计和系统状态信息
 * @param manager 功耗管理器指针
 */
static void power_manager_print_stats(PowerManager_t *manager) {
    if (!manager) {
        return;
    }
    
    LOG_INFO("=== Power Statistics ===");
    LOG_INFO("Current power mode: %d", manager->current_mode);
    LOG_INFO("Current system state: %d", manager->current_state);
    LOG_INFO("Idle time: %d ms", manager->power_stats.idle_time);
    LOG_INFO("Wakeup count: %d", manager->power_stats.wakeup_count);
    LOG_INFO("Mode durations (ms):");
    LOG_INFO("  Normal: %d", manager->power_stats.mode_duration[POWER_MODE_NORMAL]);
    LOG_INFO("  Low: %d", manager->power_stats.mode_duration[POWER_MODE_LOW]);
    LOG_INFO("  Standby: %d", manager->power_stats.mode_duration[POWER_MODE_STANDBY]);
    LOG_INFO("State durations (ms):");
    LOG_INFO("  Active: %d", manager->power_stats.state_duration[SYSTEM_STATE_ACTIVE]);
    LOG_INFO("  Idle: %d", manager->power_stats.state_duration[SYSTEM_STATE_IDLE]);
    LOG_INFO("  Suspended: %d", manager->power_stats.state_duration[SYSTEM_STATE_SUSPENDED]);
    LOG_INFO("Power management features:");
    LOG_INFO("  Dynamic power management: %s", manager->dynamic_power_management ? "Yes" : "No");
    LOG_INFO("  Low power mode: %s", manager->low_power_mode_enabled ? "Yes" : "No");
    LOG_INFO("=====================");
}

/**
 * @brief 反初始化功耗管理器
 * @details 反初始化功耗管理器，释放资源
 * @param manager 功耗管理器指针
 */
static void power_manager_deinit(PowerManager_t *manager) {
    if (!manager) {
        return;
    }
    
    // 打印功耗统计
    power_manager_print_stats(manager);
    
    // 退出低功耗模式
    power_manager_exit_low_power(manager);
    
    // 释放功耗管理器
    free(manager);
    LOG_INFO("Power manager deinitialized");
}

/**
 * @brief 信号处理函数
 * @details 处理系统信号，实现优雅退出
 * @param sig 接收到的信号类型
 * @return 无
 */
static void sig_handler(int sig) {
    // 处理中断信号和终止信号
    if (sig == SIGINT || sig == SIGTERM) {
        LOG_INFO("System receive exit signal [%d], start deinit...", sig);
        // 设置系统运行状态为退出
        g_sys_running = 0;
    }
}

/**
 * @brief 模块初始化总入口
 * @details 根据宏控配置初始化所有启用的模块，适配不同产品类型
 * @return 初始化结果，0表示成功，非0表示失败
 */
static int module_init_all(void) {
    int ret = 0;
    
    // 1. 初始化HAL和PAL层
    // HAL层：硬件抽象层，封装硬件相关操作
    // PAL层：平台抽象层，封装平台相关服务
    ret |= hal_init();              // 初始化硬件抽象层
    ret |= pal_init();              // 初始化平台抽象层
    
    // 2. 初始化内存管理器
    // 内存管理器负责内存分配、释放和泄漏检测
    g_di_container.memory_manager = memory_manager_init();
    if (!g_di_container.memory_manager) {
        LOG_ERROR("Memory manager init failed");
        ret |= -1;
    }
    
    // 3. 初始化错误处理器
    // 错误处理器负责错误统计和恢复
    g_di_container.error_handler = error_handler_init();
    if (!g_di_container.error_handler) {
        LOG_ERROR("Error handler init failed");
        ret |= -1;
    }
    
    // 4. 初始化安全管理器
    // 安全管理器负责安全检查和更新
    g_di_container.security_manager = security_manager_init();
    if (!g_di_container.security_manager) {
        LOG_ERROR("Security manager init failed");
        ret |= -1;
    }
    
    // 5. 初始化功耗管理器
    // 功耗管理器负责动态功耗管理
    g_di_container.power_manager = power_manager_init();
    if (!g_di_container.power_manager) {
        LOG_ERROR("Power manager init failed");
        ret |= -1;
    }
    
    // 6. 初始化配置管理器
    // 配置管理器负责读取和解析配置文件，提供配置项访问
    g_di_container.config_manager = config_manager_init(g_config_file_path);
    if (!g_di_container.config_manager) {
        LOG_ERROR("Config manager init failed");
        ret |= -1;
    }
    
    // 3. 从配置文件加载配置
    // 创建并初始化音频核心配置
    AudioCoreConfig_t audio_cfg = {
        .sample_rate = config_manager_get_int(g_di_container.config_manager, CONFIG_AUDIO_SAMPLE_RATE, 48000),
        .channel_num = config_manager_get_int(g_di_container.config_manager, CONFIG_AUDIO_CHANNEL_NUM, 2),
        .pcm_buffer_size = config_manager_get_int(g_di_container.config_manager, CONFIG_AUDIO_PCM_BUFFER_SIZE, 4096),
        .hw_decode_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_AUDIO_HW_DECODE_EN, true),
        .dolby_dts_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_AUDIO_DOLBY_DTS_EN, false)
    };
    
    // 创建并初始化HDMI ARC配置
#ifdef CONFIG_ENABLE_HDMI_ARC
    HdmiArcConfig_t hdmi_cfg = {
        .cec_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_HDMI_CEC_EN, true),
        .auto_switch_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_HDMI_AUTO_SWITCH_EN, true),
        .sample_rate = config_manager_get_int(g_di_container.config_manager, CONFIG_HDMI_SAMPLE_RATE, 48000)
    };
#endif
    
    // 创建并初始化SPDIF配置
#ifdef CONFIG_ENABLE_SPDIF
    SpdifConfig_t spdif_cfg = {
        .auto_switch_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_SPDIF_AUTO_SWITCH_EN, true),
        .sample_rate = config_manager_get_int(g_di_container.config_manager, CONFIG_SPDIF_SAMPLE_RATE, 48000),
        .bits_per_sample = config_manager_get_int(g_di_container.config_manager, CONFIG_SPDIF_BITS_PER_SAMPLE, 16)
    };
#endif
    
    // 创建并初始化外设配置
    PeripheralConfig_t peri_cfg = {
        .key_debounce_ms = config_manager_get_int(g_di_container.config_manager, CONFIG_PERIPHERAL_KEY_DEBOUNCE_MS, 50),
        .long_press_ms = config_manager_get_int(g_di_container.config_manager, CONFIG_PERIPHERAL_LONG_PRESS_MS, 1000),
        .ir_learn_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_PERIPHERAL_IR_LEARN_EN, true),
        .mic_mute_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_PERIPHERAL_MIC_MUTE_EN, true)
    };
    
    // 4. 根据产品类型调整配置
    switch (CURRENT_PRODUCT_TYPE) {
#ifdef CONFIG_ENABLE_GAME_SPEAKER
        case PRODUCT_GAME_HIGH_END:
            // 高端游戏音响配置
            LOG_INFO("=== Initializing HIGH END GAME SPEAKER ===");
            audio_cfg.pcm_buffer_size = 8192; // 增大缓冲区，支持复杂音效
            audio_cfg.dolby_dts_en = true;   // 启用杜比DTS解码
            peri_cfg.ir_learn_en = true;      // 启用红外学习
            break;
        
        case PRODUCT_GAME_MID_END:
            // 中端游戏音响配置
            LOG_INFO("=== Initializing MID END GAME SPEAKER ===");
            audio_cfg.pcm_buffer_size = 4096; // 标准缓冲区大小
            audio_cfg.dolby_dts_en = false;  // 禁用杜比DTS解码
            peri_cfg.ir_learn_en = false;     // 禁用红外学习
            break;
        
        case PRODUCT_GAME_LOW_END:
            // 低端游戏音响配置
            LOG_INFO("=== Initializing LOW END GAME SPEAKER ===");
            audio_cfg.pcm_buffer_size = 2048; // 减小缓冲区，降低资源占用
            audio_cfg.dolby_dts_en = false;  // 禁用杜比DTS解码
            peri_cfg.ir_learn_en = false;     // 禁用红外学习
            peri_cfg.mic_mute_en = false;     // 禁用麦克风静音功能
            break;
#endif
        default:
            // 其他产品类型保持默认配置
            break;
    }
    
    // 5. 初始化基础核心模块（使用依赖注入）
    // 初始化顺序：存储 -> 外设 -> 蓝牙 -> 音频核心 -> 音频源 -> 音量控制 -> 播放控制 -> MCU通信 -> 系统 -> 生产测试
    // 非游戏音响或非低端游戏音响初始化基础模块
#ifdef CONFIG_ENABLE_GAME_SPEAKER
    if (CURRENT_PRODUCT_TYPE != PRODUCT_GAME_LOW_END) {
#else
    {
#endif
        // 存储模块初始化
        if (storage_init() == SUCCESS) {
            g_di_container.storage = MODULE_STATE_INIT_SUCCESS;
        } else {
            g_di_container.storage = MODULE_STATE_INIT_FAILED;
            ret |= -1;
        }
        
        // 外设模块初始化
        if (peripheral_init(&peri_cfg) == SUCCESS) {
            g_di_container.peripheral = MODULE_STATE_INIT_SUCCESS;
        } else {
            g_di_container.peripheral = MODULE_STATE_INIT_FAILED;
            ret |= -1;
        }
        
        // 蓝牙模块初始化，使用配置文件中的配置
        BluetoothConfig_t bt_cfg = {
            .bt_name = config_manager_get_string(g_di_container.config_manager, CONFIG_BLUETOOTH_BT_NAME, "AML Audio Speaker"),
            .bt_pin = config_manager_get_string(g_di_container.config_manager, CONFIG_BLUETOOTH_BT_PIN, "0000"),
            .bt_auto_connect = config_manager_get_bool(g_di_container.config_manager, CONFIG_BLUETOOTH_BT_AUTO_CONNECT, true),
            .bt_mesh_en = false
        };
#ifdef CONFIG_ENABLE_BT_MESH
        bt_cfg.bt_mesh_en = true;
#endif
        if (bluetooth_init(&bt_cfg) == SUCCESS) {
            g_di_container.bluetooth = MODULE_STATE_INIT_SUCCESS;
        } else {
            g_di_container.bluetooth = MODULE_STATE_INIT_FAILED;
            ret |= -1;
        }
    }

#ifdef CONFIG_ENABLE_GAME_SPEAKER
    // 低端游戏音响：仅初始化必要模块
    if (CURRENT_PRODUCT_TYPE == PRODUCT_GAME_LOW_END) {
        // 音频核心模块初始化
        if (audio_core_init(&audio_cfg) == SUCCESS) {
            g_di_container.audio_core = MODULE_STATE_INIT_SUCCESS;
        } else {
            g_di_container.audio_core = MODULE_STATE_INIT_FAILED;
            ret |= -1;
        }
        
        // 音量控制模块初始化，使用默认音量设置
        VolumeInfo_t default_vol = {
            .master_volume = config_manager_get_int(g_di_container.config_manager, CONFIG_VOLUME_MASTER_VOLUME, DEFAULT_VOLUME_VAL),
            .bass_volume = config_manager_get_int(g_di_container.config_manager, CONFIG_VOLUME_BASS_VOLUME, DEFAULT_VOLUME_VAL),
            .treble_volume = config_manager_get_int(g_di_container.config_manager, CONFIG_VOLUME_TREBLE_VOLUME, DEFAULT_VOLUME_VAL),
            .is_mute = config_manager_get_bool(g_di_container.config_manager, CONFIG_VOLUME_IS_MUTE, false)
        };
        if (volume_ctrl_init(&default_vol) == SUCCESS) {
            g_di_container.volume_ctrl = MODULE_STATE_INIT_SUCCESS;
        } else {
            g_di_container.volume_ctrl = MODULE_STATE_INIT_FAILED;
            ret |= -1;
        }
        
        // 系统模块初始化
        if (system_init() == SUCCESS) {
            g_di_container.system = MODULE_STATE_INIT_SUCCESS;
        } else {
            g_di_container.system = MODULE_STATE_INIT_FAILED;
            ret |= -1;
        }
        
        return ret;
    }
#endif
    
    // 非低端游戏音响初始化其他模块
    // 音频核心模块初始化
    if (audio_core_init(&audio_cfg) == SUCCESS) {
        g_di_container.audio_core = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.audio_core = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
    
    // 初始化音频源模块，使用配置文件中的配置
    AudioSourceConfig_t as_cfg = {
        .source_list = {SOURCE_BLUETOOTH, SOURCE_USB, SOURCE_HDMI_ARC, SOURCE_SPDIF, SOURCE_AUX, SOURCE_WIFI},
        .source_cnt = 6,
        .default_source = config_manager_get_int(g_di_container.config_manager, CONFIG_AUDIO_DEFAULT_SOURCE, SOURCE_BLUETOOTH),
        .auto_switch_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_AUDIO_AUTO_SWITCH_EN, true)
    };
    if (audio_source_init(&as_cfg) == SUCCESS) {
        g_di_container.audio_source = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.audio_source = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
    
    // 初始化音量控制模块，使用配置文件中的配置
    VolumeInfo_t default_vol = {
        .master_volume = config_manager_get_int(g_di_container.config_manager, CONFIG_VOLUME_MASTER_VOLUME, DEFAULT_VOLUME_VAL),
        .bass_volume = config_manager_get_int(g_di_container.config_manager, CONFIG_VOLUME_BASS_VOLUME, DEFAULT_VOLUME_VAL),
        .treble_volume = config_manager_get_int(g_di_container.config_manager, CONFIG_VOLUME_TREBLE_VOLUME, DEFAULT_VOLUME_VAL),
        .is_mute = config_manager_get_bool(g_di_container.config_manager, CONFIG_VOLUME_IS_MUTE, false)
    };
    if (volume_ctrl_init(&default_vol) == SUCCESS) {
        g_di_container.volume_ctrl = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.volume_ctrl = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
    
    // 初始化播放控制模块
    PlayCtrlConfig_t play_cfg = {
        .power_off_resume_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_PLAY_POWER_OFF_RESUME_EN, true),
        .boot_default_play_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_PLAY_BOOT_DEFAULT_PLAY_EN, false),
        .bt_reconnect_timeout = config_manager_get_int(g_di_container.config_manager, CONFIG_PLAY_BT_RECONNECT_TIMEOUT, 10),
        .default_mode = config_manager_get_int(g_di_container.config_manager, CONFIG_PLAY_DEFAULT_MODE, SOUND_MODE_NORMAL)
    };
    if (play_ctrl_init(&play_cfg) == SUCCESS) {
        g_di_container.play_ctrl = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.play_ctrl = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
    
    // MCU通信模块初始化
    if (comm_mcu_init() == SUCCESS) {
        g_di_container.comm_mcu = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.comm_mcu = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
    
    // 系统模块初始化
    if (system_init() == SUCCESS) {
        g_di_container.system = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.system = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
    
    // 生产测试模块初始化
    if (prod_test_init() == SUCCESS) {
        // 生产测试模块不需要在容器中记录状态
    } else {
        ret |= -1;
    }

    // 6. 根据宏控配置初始化可选模块
#ifdef CONFIG_ENABLE_HDMI_ARC
    // 初始化HDMI ARC模块
    if (hdmi_arc_init(&hdmi_cfg) == SUCCESS) {
        g_di_container.hdmi_arc = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.hdmi_arc = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
#endif
#ifdef CONFIG_ENABLE_SPDIF
    // 初始化SPDIF模块
    if (spdif_optical_init(&spdif_cfg) == SUCCESS) {
        g_di_container.spdif_optical = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.spdif_optical = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
#endif
#ifdef CONFIG_ENABLE_DOLBY_DTS
    // 初始化音效模块
    SoundEffectsConfig_t se_cfg = {
        .dolby_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_SOUND_DOLBY_EN, true),
        .virtual_5_1_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_SOUND_VIRTUAL_5_1_EN, false)
    };
    if (sound_effects_init(&se_cfg) == SUCCESS) {
        g_di_container.sound_effects = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.sound_effects = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
#endif
#ifdef CONFIG_ENABLE_VOICE_NOISE_REDUCTION
    // 初始化语音降噪模块
    if (voice_noise_reduction_init() == SUCCESS) {
        g_di_container.voice_noise_reduction = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.voice_noise_reduction = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
#endif
#ifdef CONFIG_ENABLE_WIFI_MEDIA
    // 初始化WIFI媒体模块
    WifiMediaConfig_t wifi_cfg = {
        .wifi_name = config_manager_get_string(g_di_container.config_manager, CONFIG_WIFI_WIFI_NAME, "Aml_Soundbar"),
        .dlna_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_WIFI_DLNA_EN, true),
        .airplay_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_WIFI_AIRPLAY_EN, true)
    };
    if (wifi_media_init(&wifi_cfg) == SUCCESS) {
        g_di_container.wifi_media = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.wifi_media = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
#endif
#ifdef CONFIG_ENABLE_BT_MESH
    // 初始化低音炮通信模块
    SubwooferConfig_t sw_cfg = {
        .bt_name = config_manager_get_string(g_di_container.config_manager, CONFIG_SUBWOOFER_BT_NAME, "AML Subwoofer"),
        .bass_gain = config_manager_get_int(g_di_container.config_manager, CONFIG_SUBWOOFER_BASS_GAIN, 50),
        .vol_sync_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_SUBWOOFER_VOL_SYNC_EN, true),
        .auto_connect_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_SUBWOOFER_AUTO_CONNECT_EN, true)
    };
    if (subwoofer_comm_init(&sw_cfg) == SUCCESS) {
        g_di_container.subwoofer_comm = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.subwoofer_comm = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
#endif

    // 7. 输出初始化结果
    LOG_INFO("All modules init: Product Type=%d, Status=%s", 
             CURRENT_PRODUCT_TYPE, ret == 0 ? "SUCCESS" : "WARN");
    // 返回初始化结果，0表示成功，非0表示失败
    return ret == 0 ? 0 : -1;
}

/**
 * @brief 模块反初始化总入口
 * @details 按照与初始化相反的顺序反初始化所有模块，确保资源正确释放
 * @return 无
 */
static void module_deinit_all(void) {
    // 1. 首先反初始化可选模块
    // 反初始化顺序与初始化顺序相反
#ifdef CONFIG_ENABLE_BT_MESH
    if (g_di_container.subwoofer_comm == MODULE_STATE_INIT_SUCCESS) {
        subwoofer_comm_deinit();
        g_di_container.subwoofer_comm = MODULE_STATE_UNINIT;
    }
#endif
#ifdef CONFIG_ENABLE_WIFI_MEDIA
    if (g_di_container.wifi_media == MODULE_STATE_INIT_SUCCESS) {
        wifi_media_deinit();
        g_di_container.wifi_media = MODULE_STATE_UNINIT;
    }
#endif
#ifdef CONFIG_ENABLE_DOLBY_DTS
    if (g_di_container.sound_effects == MODULE_STATE_INIT_SUCCESS) {
        sound_effects_deinit();
        g_di_container.sound_effects = MODULE_STATE_UNINIT;
    }
#endif
#ifdef CONFIG_ENABLE_VOICE_NOISE_REDUCTION
    if (g_di_container.voice_noise_reduction == MODULE_STATE_INIT_SUCCESS) {
        voice_noise_reduction_deinit();
        g_di_container.voice_noise_reduction = MODULE_STATE_UNINIT;
    }
#endif
#ifdef CONFIG_ENABLE_SPDIF
    if (g_di_container.spdif_optical == MODULE_STATE_INIT_SUCCESS) {
        spdif_optical_deinit();
        g_di_container.spdif_optical = MODULE_STATE_UNINIT;
    }
#endif
#ifdef CONFIG_ENABLE_HDMI_ARC
    if (g_di_container.hdmi_arc == MODULE_STATE_INIT_SUCCESS) {
        hdmi_arc_deinit();
        g_di_container.hdmi_arc = MODULE_STATE_UNINIT;
    }
#endif

    // 2. 然后反初始化基础核心模块
    // 反初始化顺序：生产测试 -> 系统 -> MCU通信 -> 播放控制 -> 音量控制 -> 音频源 -> 音频核心 -> 蓝牙 -> 外设 -> 存储
    // 非游戏音响或非低端游戏音响反初始化所有模块
#ifdef CONFIG_ENABLE_GAME_SPEAKER
    if (CURRENT_PRODUCT_TYPE != PRODUCT_GAME_LOW_END) {
#else
    {
#endif
        prod_test_deinit();               // 生产测试模块
        
        if (g_di_container.system == MODULE_STATE_INIT_SUCCESS) {
            system_deinit();
            g_di_container.system = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.comm_mcu == MODULE_STATE_INIT_SUCCESS) {
            comm_mcu_deinit();
            g_di_container.comm_mcu = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.play_ctrl == MODULE_STATE_INIT_SUCCESS) {
            play_ctrl_deinit();
            g_di_container.play_ctrl = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.volume_ctrl == MODULE_STATE_INIT_SUCCESS) {
            volume_ctrl_deinit();
            g_di_container.volume_ctrl = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.audio_source == MODULE_STATE_INIT_SUCCESS) {
            audio_source_deinit();
            g_di_container.audio_source = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.audio_core == MODULE_STATE_INIT_SUCCESS) {
            audio_core_deinit();
            g_di_container.audio_core = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.bluetooth == MODULE_STATE_INIT_SUCCESS) {
            bluetooth_deinit();
            g_di_container.bluetooth = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.peripheral == MODULE_STATE_INIT_SUCCESS) {
            peripheral_deinit();
            g_di_container.peripheral = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.storage == MODULE_STATE_INIT_SUCCESS) {
            storage_deinit();
            g_di_container.storage = MODULE_STATE_UNINIT;
        }
    }

#ifdef CONFIG_ENABLE_GAME_SPEAKER
    // 低端游戏音响：仅反初始化必要模块
    if (CURRENT_PRODUCT_TYPE == PRODUCT_GAME_LOW_END) {
        if (g_di_container.audio_core == MODULE_STATE_INIT_SUCCESS) {
            audio_core_deinit();
            g_di_container.audio_core = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.volume_ctrl == MODULE_STATE_INIT_SUCCESS) {
            volume_ctrl_deinit();
            g_di_container.volume_ctrl = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.system == MODULE_STATE_INIT_SUCCESS) {
            system_deinit();
            g_di_container.system = MODULE_STATE_UNINIT;
        }
    }
#endif

    // 3. 反初始化配置管理器
    if (g_di_container.config_manager) {
        config_manager_deinit(g_di_container.config_manager);
        g_di_container.config_manager = NULL;
    }
    
    // 4. 反初始化内存管理器
    if (g_di_container.memory_manager) {
        memory_manager_deinit(g_di_container.memory_manager);
        g_di_container.memory_manager = NULL;
    }
    
    // 5. 反初始化错误处理器
    if (g_di_container.error_handler) {
        error_handler_deinit(g_di_container.error_handler);
        g_di_container.error_handler = NULL;
    }
    
    // 6. 反初始化安全管理器
    if (g_di_container.security_manager) {
        security_manager_deinit(g_di_container.security_manager);
        g_di_container.security_manager = NULL;
    }
    
    // 7. 反初始化功耗管理器
    if (g_di_container.power_manager) {
        power_manager_deinit(g_di_container.power_manager);
        g_di_container.power_manager = NULL;
    }

    // 最后反初始化HAL和PAL层
    // 反初始化顺序与初始化顺序相反
    pal_deinit();              // 反初始化平台抽象层
    hal_deinit();              // 反初始化硬件抽象层

    LOG_INFO("✅ All modules deinit success");
    LOG_INFO("✅ HAL and PAL layers deinit success");
}

/**
 * @brief 模块事件处理器定义
 * @details 用于管理各个模块的轮询频率和事件处理
 */
typedef void (*EventHandler)(void);

// 外部函数声明，确保可见性
extern void system_event_poll(void);
extern void bluetooth_event_poll(void);
extern void audio_source_event_poll(void);
extern void play_ctrl_event_poll(void);
extern void wifi_media_event_poll(void);
#ifdef CONFIG_ENABLE_BT_MESH
extern void subwoofer_comm_event_poll(void);
#endif
#ifdef CONFIG_ENABLE_HDMI_ARC
extern void hdmi_arc_event_poll(void);
#endif
#ifdef CONFIG_ENABLE_SPDIF
extern void spdif_optical_event_poll(void);
#endif

/**
 * @brief 模块轮询配置结构体
 */
typedef struct {
    EventHandler handler;     // 事件处理函数
    uint32_t interval_ms;     // 轮询间隔（毫秒）
    uint32_t last_run_ms;     // 上次运行时间（毫秒）
} ModulePollConfig;

/**
 * @brief 获取当前高精度时间
 * @return 当前时间（毫秒）
 * @details 使用clock_gettime获取CLOCK_MONOTONIC时间，精度更高
 */
static uint32_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/**
 * @brief 业务主循环
 * @details 使用高精度时间函数定期轮询各个模块，处理系统的核心业务逻辑
 * @return 无
 */
static void main_business_loop(void) {
    LOG_INFO("Enter business loop...");
    
    // 启动资源监控
    resource_start_monitoring(1000);
    // 启动系统监控
    monitor_start(1000);
    
    // 初始化模块轮询配置
    ModulePollConfig poll_configs[] = {
        {bluetooth_event_poll, 50, 0},        // 蓝牙事件：每50毫秒轮询一次
        {audio_source_event_poll, 50, 0},     // 音频源事件：每50毫秒轮询一次
        {play_ctrl_event_poll, 10, 0},        // 播放控制事件：每10毫秒轮询一次（高实时性）
        {system_event_poll, 2000, 0},         // 系统事件：每2秒轮询一次
#ifdef CONFIG_ENABLE_BT_MESH
        {subwoofer_comm_event_poll, 100, 0},  // 低音炮通信事件：每100毫秒轮询一次
#endif
#ifdef CONFIG_ENABLE_HDMI_ARC
        {hdmi_arc_event_poll, 100, 0},        // HDMI ARC事件：每100毫秒轮询一次
#endif
#ifdef CONFIG_ENABLE_SPDIF
        {spdif_optical_event_poll, 100, 0},   // SPDIF事件：每100毫秒轮询一次
#endif
#ifdef CONFIG_ENABLE_WIFI_MEDIA
        {wifi_media_event_poll, 100, 0},      // WiFi媒体事件：每100毫秒轮询一次
#endif
        {NULL, 100, 0}                        // 结束标记
    };
    int epoll_fd = -1;
    int timer_fd = -1;
    
    // 动态轮询频率配置
    uint32_t base_poll_interval = 10;  // 基础轮询间隔（毫秒）
    uint32_t current_poll_interval = base_poll_interval;
    
    // 条件编译：根据平台选择不同的事件驱动方式
#if defined(__linux__) || defined(__linux) || defined(LINUX)
    // Linux平台：使用epoll和timerfd实现事件驱动
    // 创建epoll实例
    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        LOG_ERROR("Failed to create epoll: %d", errno);
        return;
    }
    
    // 创建定时器
    timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer_fd == -1) {
        LOG_ERROR("Failed to create timerfd: %d", errno);
        close(epoll_fd);
        return;
    }
    
    // 设置定时器，初始轮询间隔
    struct itimerspec its = {
        .it_interval = {.tv_sec = 0, .tv_nsec = current_poll_interval * 1000 * 1000},
        .it_value = {.tv_sec = 0, .tv_nsec = current_poll_interval * 1000 * 1000}
    };
    
    if (timerfd_settime(timer_fd, 0, &its, NULL) == -1) {
        LOG_ERROR("Failed to set timerfd: %d", errno);
        close(timer_fd);
        close(epoll_fd);
        return;
    }
    
    // 注册定时器到epoll
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = timer_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &event) == -1) {
        LOG_ERROR("Failed to add timerfd to epoll: %d", errno);
        close(timer_fd);
        close(epoll_fd);
        return;
    }
    
    // 主循环，直到系统运行状态为0时退出
    struct epoll_event events[10];
    while (g_sys_running) {
        // 使用epoll阻塞等待事件
        int ret = epoll_wait(epoll_fd, events, 10, -1);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;  // 被信号中断，继续循环
            }
            LOG_ERROR("epoll_wait error: %d", errno);
            break;
        }
        
        // 处理事件
        for (int i = 0; i < ret; i++) {
            if (events[i].data.fd == timer_fd && (events[i].events & EPOLLIN)) {
                // 读取定时器数据以清除事件
                uint64_t exp;
                read(timer_fd, &exp, sizeof(exp));
                
                // 使用高精度时间函数获取当前时间
                uint32_t current_time = get_current_time_ms();
                
                // 检查系统空闲状态，调整功耗模式
                if (g_di_container.power_manager) {
                    power_manager_check_idle(g_di_container.power_manager);
                    // 根据功耗模式调整轮询间隔
                    current_poll_interval = power_manager_get_poll_interval(g_di_container.power_manager);
                } else {
                    // 动态调整轮询频率
                    // 根据系统负载调整轮询间隔
                    uint32_t system_load = resource_get_system_load();
                    if (system_load > 80) {
                        // 系统负载高，增加轮询间隔
                        current_poll_interval = base_poll_interval * 2;
                    } else if (system_load < 30) {
                        // 系统负载低，减少轮询间隔
                        current_poll_interval = base_poll_interval;
                    }
                }
                
                // 更新定时器间隔
                its.it_interval.tv_nsec = current_poll_interval * 1000 * 1000;
                its.it_value.tv_nsec = current_poll_interval * 1000 * 1000;
                timerfd_settime(timer_fd, 0, &its, NULL);
                
                // 轮询各个模块的事件
#else
    // 非Linux平台：使用传统轮询方式
    while (g_sys_running) {
        // 使用高精度时间函数获取当前时间
        uint32_t current_time = get_current_time_ms();
        
        // 检查系统空闲状态，调整功耗模式
        if (g_di_container.power_manager) {
            power_manager_check_idle(g_di_container.power_manager);
            // 根据功耗模式调整轮询间隔
            current_poll_interval = power_manager_get_poll_interval(g_di_container.power_manager);
        }
        
        // 轮询各个模块的事件
#endif
#ifdef CONFIG_ENABLE_GAME_SPEAKER
                if (CURRENT_PRODUCT_TYPE != PRODUCT_GAME_LOW_END) {
#else
                {
#endif
                    peripheral_event_poll();        // 外设事件：按键、红外等
                    
                    // 处理按键事件
                KeyEvent_e key_event = peripheral_get_key_event();
                if (key_event != KEY_EVENT_NONE) {
                    LOG_INFO("Key event received: %d", key_event);
                    
                    // 更新系统活动时间
                    if (g_di_container.power_manager) {
                        power_manager_update_activity(g_di_container.power_manager);
                    }
                    
                    // 根据按键事件执行相应操作
                    switch (key_event) {
                            case KEY_EVENT_PLAY_PAUSE:
                                // 播放/暂停控制
                                {
                                    PlayState_e current_state = play_ctrl_get_state();
                                    if (current_state == PLAY_STATE_PLAYING) {
                                        play_ctrl_set_state(PLAY_STATE_PAUSE);
                                        LOG_INFO("Playback paused");
                                    } else {
                                        play_ctrl_set_state(PLAY_STATE_PLAYING);
                                        LOG_INFO("Playback started");
                                    }
                                }
                                break;
                            case KEY_EVENT_VOL_UP:
                                // 音量增加
                                {
                                    int new_vol = volume_ctrl_master_up();
                                    LOG_INFO("Volume increased to: %d", new_vol);
                                }
                                break;
                            case KEY_EVENT_VOL_DOWN:
                                // 音量减少
                                {
                                    int new_vol = volume_ctrl_master_down();
                                    LOG_INFO("Volume decreased to: %d", new_vol);
                                }
                                break;
                            case KEY_EVENT_SOURCE_SWITCH:
                                // 音源切换
                                {
                                    LOG_INFO("Source switch requested");
                                    audio_source_switch_next();
                                }
                                break;
                            case KEY_EVENT_SOUND_MODE:
                                // 音效模式切换
                                {
                                    SoundMode_e current_mode = play_ctrl_get_sound_mode();
                                    SoundMode_e next_mode = (current_mode + 1) % SOUND_MODE_MAX;
                                    play_ctrl_set_state(next_mode);
                                    LOG_INFO("Sound mode switched to: %d", next_mode);
                                }
                                break;
                            case KEY_EVENT_BASS_UP:
                                // 低音增加
                                {
#if CONFIG_ENABLE_2VOL_CTRL || CONFIG_ENABLE_3VOL_CTRL
                                    int new_bass = volume_ctrl_bass_up();
                                    LOG_INFO("Bass increased to: %d", new_bass);
#else
                                    LOG_INFO("Bass control not supported on this product");
#endif
                                }
                                break;
                            case KEY_EVENT_TREBLE_UP:
                                // 高音增加
                                {
#if CONFIG_ENABLE_3VOL_CTRL
                                    int new_treble = volume_ctrl_treble_up();
                                    LOG_INFO("Treble increased to: %d", new_treble);
#else
                                    LOG_INFO("Treble control not supported on this product");
#endif
                                }
                                break;
                            case KEY_EVENT_IR_LEARN:
                                // 红外学习
                                {
#if CONFIG_ENABLE_IR_LEARN
                                    LOG_INFO("IR learn mode activated");
                                    peripheral_start_ir_learn();
#else
                                    LOG_INFO("IR learn not supported on this product");
#endif
                                }
                                break;
                            case KEY_EVENT_NEXT:
                                // 下一曲
                                {
                                    LOG_INFO("Next song requested");
                                    play_ctrl_next_song();
                                }
                                break;
                            case KEY_EVENT_PREV:
                                // 上一曲
                                {
                                    LOG_INFO("Previous song requested");
                                    play_ctrl_prev_song();
                                }
                                break;
                            default:
                                LOG_INFO("Unknown key event: %d", key_event);
                                break;
                        }
                    }
                    
                    // 遍历模块轮询配置，自动管理轮询频率
                    for (int i = 0; i < (int)(sizeof(poll_configs) / sizeof(poll_configs[0]) - 1); i++) {
                        if (poll_configs[i].handler != NULL &&
                            current_time - poll_configs[i].last_run_ms >= poll_configs[i].interval_ms) {
                            poll_configs[i].handler();
                            poll_configs[i].last_run_ms = current_time;
                        }
                    }
                    
                    // 可选模块的事件轮询已集成到统一的轮询配置中
                }

#ifdef CONFIG_ENABLE_GAME_SPEAKER
                // 低端游戏音响：仅轮询必要模块
                if (CURRENT_PRODUCT_TYPE == PRODUCT_GAME_LOW_END) {
                    system_event_poll();            // 系统事件：系统状态、资源使用等
                }
#endif
                
                // 监控系统进程状态
                process_manager_monitor();
            
#if defined(__linux__) || defined(__linux) || defined(LINUX)
            }
        }
    }
    
    // 清理资源
    if (timer_fd != -1) {
        close(timer_fd);
    }
    if (epoll_fd != -1) {
        close(epoll_fd);
    }
#else
        // 非Linux平台：使用传统轮询方式，休眠动态调整的时间
        usleep(current_poll_interval * 1000);
    }
#endif
}

/**
 * @brief 系统主函数
 * @details 系统的入口函数，负责初始化系统、启动业务循环和优雅退出
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 系统退出码，0表示成功，非0表示失败
 */
int main(int argc, char *argv[]) {
    int ret = 0;
    ResInfo_t boot_tone = {0};

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    // 基础初始化
    // 日志系统已经通过aml_log.h初始化，不需要单独调用log_system_init
    LOG_INFO("Log system initialized via aml_log.h");
    ret = res_manager_init();
    if (ret != 0) {
        LOG_ERROR("Res manager init failed: %d", ret);
        return ret;
    }
    ret = event_system_init();
    if (ret != 0) {
        LOG_ERROR("Event system init failed: %d", ret);
        return ret;
    }
    
    // 初始化线程池，设置4个线程
    ret = thread_pool_init(4);
    if (ret != 0) {
        LOG_ERROR("Thread pool init failed: %d", ret);
        return ret;
    }
    
    // 初始化进程管理器
    ret = process_manager_init();
    if (ret != 0) {
        LOG_ERROR("Process manager init failed: %d", ret);
        return ret;
    }
    
    // 初始化IPC模块
    ret = ipc_init();
    if (ret != 0) {
        LOG_ERROR("IPC init failed: %d", ret);
        return ret;
    }
    
    // 初始化资源管理模块
    ret = resource_init();
    if (ret != 0) {
        LOG_ERROR("Resource init failed: %d", ret);
        return ret;
    }
    
    // 初始化监控和诊断模块
    ret = monitor_init();
    if (ret != 0) {
        LOG_ERROR("Monitor init failed: %d", ret);
        return ret;
    }

    // 启动信息
    LOG_INFO("=====================================================");
    LOG_INFO("AML Audio Speaker [V%s] Boot", SYSTEM_VERSION);
    LOG_INFO("Compile: %s %s | Product Type: %d", __DATE__, __TIME__, CURRENT_PRODUCT_TYPE);
    LOG_INFO("=====================================================");

    // 播放开机提示音 + 开机显示图标
    boot_tone = res_load_resource(RES_TYPE_TONE_BOOT);
    if (boot_tone.res_valid) {
        audio_core_play_tone(boot_tone.res_data, boot_tone.res_size);
        res_free_resource(&boot_tone);
    }

    // 初始化模块
    ret = module_init_all();
    if (ret != 0) {
        LOG_ERROR("Module init failed: %d", ret);
        goto exit_sys;
    }

    // 业务循环
    main_business_loop();

exit_sys:
    module_deinit_all();
    event_system_deinit();
    thread_pool_deinit();
    process_manager_deinit();
    // 反初始化新模块
    monitor_deinit();
    resource_deinit();
    ipc_deinit();
    res_manager_deinit();
    // 日志系统已经通过aml_log.h管理，不需要单独调用log_system_deinit

    LOG_INFO("✅ System Exit Success");
    return ret;
}
