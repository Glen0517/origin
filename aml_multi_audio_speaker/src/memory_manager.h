/**
 * @file memory_manager.h
 * @brief 内存管理模块头文件
 * @details 提供内存池、内存分配、内存泄漏检测等功能
 * @author AML Audio Team
 * @date 2026-01-28
 */

#ifndef __MEMORY_MANAGER_H__
#define __MEMORY_MANAGER_H__

#include "../lib/flac/common_def.h"
#include "../lib/flac/logger.h"

// 内存池配置结构体
typedef struct {
    size_t block_size;      // 内存块大小
    uint32_t block_count;    // 内存块数量
    bool enable_memory_pool; // 是否启用内存池
} MemoryPoolConfig_t;

// 内存使用统计结构体
typedef struct {
    uint32_t current_allocations;     // 当前分配次数
    size_t current_allocated_size;    // 当前分配大小
    uint32_t total_allocations;       // 总分配次数
    size_t total_allocated_size;      // 总分配大小
    uint32_t peak_allocations;        // 峰值分配次数
    size_t peak_allocated_size;       // 峰值分配大小
    uint32_t memory_pool_used_blocks; // 内存池使用块数
    uint32_t memory_pool_total_blocks; // 内存池总块数
} MemoryUsageStats_t;

// 内存块结构体
typedef struct MemoryBlock {
    struct MemoryBlock *next;
    uint8_t data[];
} MemoryBlock_t;

// 内存池结构体
typedef struct {
    MemoryBlock_t *free_list;
    uint32_t total_blocks;
    uint32_t used_blocks;
    uint32_t peak_used;
    size_t block_size;
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
    uint32_t peak_allocations;
    size_t peak_allocated_size;
    MemoryPoolConfig_t pool_config;
    bool memory_monitoring_enabled;
} MemoryManager_t;

/**
 * @brief 初始化内存管理器
 * @details 创建并初始化内存管理器，包括内存池和内存分配记录
 * @param config 内存池配置，为NULL时使用默认配置
 * @return 内存管理器指针，失败返回NULL
 */
MemoryManager_t *memory_manager_init(MemoryPoolConfig_t *config);

/**
 * @brief 分配内存（带内存泄漏检测）
 * @details 分配内存并记录分配信息
 * @param manager 内存管理器指针
 * @param size 分配大小
 * @param file 调用文件
 * @param line 调用行号
 * @return 分配的内存指针，失败返回NULL
 */
void *memory_manager_alloc(MemoryManager_t *manager, size_t size, const char *file, int line);

/**
 * @brief 释放内存（带内存泄漏检测）
 * @details 释放内存并更新分配记录
 * @param manager 内存管理器指针
 * @param ptr 要释放的内存指针
 */
void memory_manager_free(MemoryManager_t *manager, void *ptr);

/**
 * @brief 检查内存泄漏
 * @details 检查并报告内存泄漏情况
 * @param manager 内存管理器指针
 */
void memory_manager_check_leaks(MemoryManager_t *manager);

/**
 * @brief 反初始化内存管理器
 * @details 反初始化内存管理器，检查内存泄漏并释放资源
 * @param manager 内存管理器指针
 */
void memory_manager_deinit(MemoryManager_t *manager);

/**
 * @brief 获取内存使用统计信息
 * @details 获取内存使用情况的统计信息
 * @param manager 内存管理器指针
 * @param stats 内存使用统计结构体指针
 */
void memory_manager_get_stats(MemoryManager_t *manager, MemoryUsageStats_t *stats);

/**
 * @brief 监控内存使用情况
 * @details 监控内存使用情况，当内存使用超过阈值时输出警告
 * @param manager 内存管理器指针
 * @param usage_threshold 内存使用阈值（0-100，表示百分比）
 */
void memory_manager_monitor_usage(MemoryManager_t *manager, uint8_t usage_threshold);

#endif // __MEMORY_MANAGER_H__
