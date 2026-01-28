/**
 * @file memory_manager.c
 * @brief 内存管理模块实现
 * @details 提供内存池、内存分配、内存泄漏检测等功能
 * @author AML Audio Team
 * @date 2026-01-28
 */

#include "memory_manager.h"

// 内存池配置
#define MEMORY_POOL_BLOCK_SIZE 256
#define MEMORY_POOL_BLOCK_COUNT 1024

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
MemoryManager_t *memory_manager_init(void) {
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
void *memory_manager_alloc(MemoryManager_t *manager, size_t size, const char *file, int line) {
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
    memory_pool_deinit(manager->memory_pool);
    
    // 释放内存管理器
    free(manager);
    LOG_INFO("Memory manager deinitialized");
}
