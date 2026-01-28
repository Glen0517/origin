/**
 * @file memory_manager.c
 * @brief 内存管理模块实现
 * @details 提供内存池、内存分配、内存泄漏检测等功能
 * @author AML Audio Team
 * @date 2026-01-28
 */

#include "memory_manager.h"

// 默认内存池配置
#define DEFAULT_MEMORY_POOL_BLOCK_SIZE 256
#define DEFAULT_MEMORY_POOL_BLOCK_COUNT 1024
#define DEFAULT_MEMORY_POOL_ENABLED true

/**
 * @brief 初始化内存池
 * @details 创建并初始化内存池，分配指定数量的内存块
 * @param config 内存池配置
 * @return 内存池指针，失败返回NULL
 */
static MemoryPool_t *memory_pool_init(MemoryPoolConfig_t *config) {
    MemoryPool_t *pool = (MemoryPool_t *)malloc(sizeof(MemoryPool_t));
    if (!pool) {
        LOG_ERROR("Failed to allocate memory pool");
        return NULL;
    }
    
    memset(pool, 0, sizeof(MemoryPool_t));
    
    // 使用配置或默认值
    size_t block_size = config ? config->block_size : DEFAULT_MEMORY_POOL_BLOCK_SIZE;
    uint32_t block_count = config ? config->block_count : DEFAULT_MEMORY_POOL_BLOCK_COUNT;
    
    pool->total_blocks = block_count;
    pool->block_size = block_size;
    
    // 分配内存块并链接成空闲列表
    MemoryBlock_t *prev = NULL;
    for (uint32_t i = 0; i < block_count; i++) {
        // 动态分配内存块，包含数据区域
        MemoryBlock_t *block = (MemoryBlock_t *)malloc(sizeof(MemoryBlock_t) + block_size);
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
             block_count, block_size);
    
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
 * @param config 内存池配置，为NULL时使用默认配置
 * @return 内存管理器指针，失败返回NULL
 */
MemoryManager_t *memory_manager_init(MemoryPoolConfig_t *config) {
    MemoryManager_t *manager = (MemoryManager_t *)malloc(sizeof(MemoryManager_t));
    if (!manager) {
        LOG_ERROR("Failed to allocate memory manager");
        return NULL;
    }
    
    memset(manager, 0, sizeof(MemoryManager_t));
    
    // 设置默认配置
    if (!config) {
        manager->pool_config.block_size = DEFAULT_MEMORY_POOL_BLOCK_SIZE;
        manager->pool_config.block_count = DEFAULT_MEMORY_POOL_BLOCK_COUNT;
        manager->pool_config.enable_memory_pool = DEFAULT_MEMORY_POOL_ENABLED;
    } else {
        manager->pool_config = *config;
    }
    
    // 初始化内存池
    if (manager->pool_config.enable_memory_pool) {
        manager->memory_pool = memory_pool_init(&manager->pool_config);
        if (!manager->memory_pool) {
            LOG_ERROR("Failed to initialize memory pool");
            free(manager);
            return NULL;
        }
    }
    
    // 启用内存监控
    manager->memory_monitoring_enabled = true;
    
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
void *memory_manager_alloc(MemoryManager_t *manager, size_t size, const char *file, int line) {
    if (!manager) {
        return NULL;
    }
    
    void *ptr = NULL;
    
    // 尝试从内存池分配（小内存）
    if (manager->pool_config.enable_memory_pool && manager->memory_pool && 
        size <= manager->pool_config.block_size) {
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
            
            // 更新峰值统计
            if (manager->current_allocations > manager->peak_allocations) {
                manager->peak_allocations = manager->current_allocations;
            }
            if (manager->current_allocated_size > manager->peak_allocated_size) {
                manager->peak_allocated_size = manager->current_allocated_size;
            }
            
            // 监控内存使用
            if (manager->memory_monitoring_enabled) {
                memory_manager_monitor_usage(manager, 80); // 80% 阈值
            }
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
void memory_manager_free(MemoryManager_t *manager, void *ptr) {
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
            if (manager->pool_config.enable_memory_pool && manager->memory_pool && 
                curr->size <= manager->pool_config.block_size) {
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
void memory_manager_check_leaks(MemoryManager_t *manager) {
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
    LOG_INFO("Peak allocations: %d, peak allocated size: %zu bytes", 
             manager->peak_allocations, manager->peak_allocated_size);
}

/**
 * @brief 获取内存使用统计信息
 * @details 获取内存使用情况的统计信息
 * @param manager 内存管理器指针
 * @param stats 内存使用统计结构体指针
 */
void memory_manager_get_stats(MemoryManager_t *manager, MemoryUsageStats_t *stats) {
    if (!manager || !stats) {
        return;
    }
    
    memset(stats, 0, sizeof(MemoryUsageStats_t));
    
    stats->current_allocations = manager->current_allocations;
    stats->current_allocated_size = manager->current_allocated_size;
    stats->total_allocations = manager->total_allocations;
    stats->total_allocated_size = manager->total_allocated_size;
    stats->peak_allocations = manager->peak_allocations;
    stats->peak_allocated_size = manager->peak_allocated_size;
    
    if (manager->memory_pool) {
        stats->memory_pool_used_blocks = manager->memory_pool->used_blocks;
        stats->memory_pool_total_blocks = manager->memory_pool->total_blocks;
    }
}

/**
 * @brief 监控内存使用情况
 * @details 监控内存使用情况，当内存使用超过阈值时输出警告
 * @param manager 内存管理器指针
 * @param usage_threshold 内存使用阈值（0-100，表示百分比）
 */
void memory_manager_monitor_usage(MemoryManager_t *manager, uint8_t usage_threshold) {
    if (!manager || !manager->memory_monitoring_enabled) {
        return;
    }
    
    // 计算内存使用百分比（基于内存池总大小）
    if (manager->memory_pool) {
        size_t memory_pool_total_size = manager->memory_pool->total_blocks * manager->memory_pool->block_size;
        size_t memory_pool_used_size = manager->memory_pool->used_blocks * manager->memory_pool->block_size;
        
        if (memory_pool_total_size > 0) {
            uint8_t usage_percent = (uint8_t)((memory_pool_used_size * 100) / memory_pool_total_size);
            if (usage_percent >= usage_threshold) {
                LOG_WARN("Memory pool usage high: %d%% (used: %zu bytes, total: %zu bytes)", 
                         usage_percent, memory_pool_used_size, memory_pool_total_size);
            }
        }
    }
}

/**
 * @brief 反初始化内存管理器
 * @details 反初始化内存管理器，检查内存泄漏并释放资源
 * @param manager 内存管理器指针
 */
void memory_manager_deinit(MemoryManager_t *manager) {
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
    if (manager->memory_pool) {
        memory_pool_deinit(manager->memory_pool);
    }
    
    // 释放内存管理器
    free(manager);
    LOG_INFO("Memory manager deinitialized");
}
