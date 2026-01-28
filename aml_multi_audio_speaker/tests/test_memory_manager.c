/**
 * @file test_memory_manager.c
 * @brief 内存管理模块单元测试
 * @details 测试内存池的初始化、分配、释放和监控功能
 * @author AML Audio Team
 * @date 2026-01-28
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/memory_manager.h"

/**
 * @brief 测试内存池初始化
 * @return 测试是否通过
 */
bool test_memory_pool_init(void) {
    printf("=== 测试内存池初始化 ===\n");
    
    // 创建内存池配置
    MemoryPoolConfig_t config = {
        .block_size = 64,
        .block_count = 100,
        .enable_memory_pool = true
    };
    
    // 初始化内存池
    MemoryPool_t *pool = memory_pool_init(&config);
    if (!pool) {
        printf("❌ 内存池初始化失败\n");
        return false;
    }
    
    printf("✅ 内存池初始化成功\n");
    printf("  块大小: %zu\n", pool->block_size);
    printf("  块数量: %zu\n", pool->total_blocks);
    printf("  已使用块: %zu\n", pool->used_blocks);
    
    // 释放内存池
    memory_pool_deinit(pool);
    printf("✅ 内存池释放成功\n");
    
    return true;
}

/**
 * @brief 测试内存分配和释放
 * @return 测试是否通过
 */
bool test_memory_allocation(void) {
    printf("\n=== 测试内存分配和释放 ===\n");
    
    // 初始化内存管理器
    MemoryManager_t *manager = memory_manager_init(NULL);
    if (!manager) {
        printf("❌ 内存管理器初始化失败\n");
        return false;
    }
    
    printf("✅ 内存管理器初始化成功\n");
    
    // 分配内存
    void *ptr1 = memory_manager_alloc(manager, 100);
    if (!ptr1) {
        printf("❌ 内存分配失败\n");
        memory_manager_deinit(manager);
        return false;
    }
    
    printf("✅ 内存分配成功: %p\n", ptr1);
    
    // 写入数据
    strcpy((char *)ptr1, "Test memory allocation");
    printf("✅ 内存写入成功: %s\n", (char *)ptr1);
    
    // 分配更多内存
    void *ptr2 = memory_manager_alloc(manager, 200);
    if (!ptr2) {
        printf("❌ 第二次内存分配失败\n");
        memory_manager_free(manager, ptr1);
        memory_manager_deinit(manager);
        return false;
    }
    
    printf("✅ 第二次内存分配成功: %p\n", ptr2);
    
    // 释放内存
    memory_manager_free(manager, ptr1);
    printf("✅ 内存释放成功\n");
    
    memory_manager_free(manager, ptr2);
    printf("✅ 第二次内存释放成功\n");
    
    // 测试内存泄漏检测
    memory_manager_check_leaks(manager);
    printf("✅ 内存泄漏检测完成\n");
    
    // 释放内存管理器
    memory_manager_deinit(manager);
    printf("✅ 内存管理器释放成功\n");
    
    return true;
}

/**
 * @brief 测试内存使用监控
 * @return 测试是否通过
 */
bool test_memory_monitoring(void) {
    printf("\n=== 测试内存使用监控 ===\n");
    
    // 初始化内存管理器
    MemoryManager_t *manager = memory_manager_init(NULL);
    if (!manager) {
        printf("❌ 内存管理器初始化失败\n");
        return false;
    }
    
    // 分配内存
    void *ptr1 = memory_manager_alloc(manager, 100);
    void *ptr2 = memory_manager_alloc(manager, 200);
    void *ptr3 = memory_manager_alloc(manager, 300);
    
    if (!ptr1 || !ptr2 || !ptr3) {
        printf("❌ 内存分配失败\n");
        memory_manager_deinit(manager);
        return false;
    }
    
    // 获取内存使用统计
    MemoryUsageStats_t stats;
    memory_manager_get_stats(manager, &stats);
    
    printf("✅ 内存使用统计\n");
    printf("  当前使用: %zu bytes\n", stats.current_usage);
    printf("  峰值使用: %zu bytes\n", stats.peak_usage);
    printf("  分配次数: %zu\n", stats.allocation_count);
    printf("  释放次数: %zu\n", stats.free_count);
    
    // 监控内存使用
    memory_manager_monitor_usage(manager, true);
    printf("✅ 内存使用监控开启\n");
    
    // 释放内存
    memory_manager_free(manager, ptr1);
    memory_manager_free(manager, ptr2);
    memory_manager_free(manager, ptr3);
    
    // 再次获取统计
    memory_manager_get_stats(manager, &stats);
    printf("✅ 内存释放后统计\n");
    printf("  当前使用: %zu bytes\n", stats.current_usage);
    printf("  峰值使用: %zu bytes\n", stats.peak_usage);
    
    // 释放内存管理器
    memory_manager_deinit(manager);
    printf("✅ 内存管理器释放成功\n");
    
    return true;
}

/**
 * @brief 测试内存池边界情况
 * @return 测试是否通过
 */
bool test_memory_pool_boundaries(void) {
    printf("\n=== 测试内存池边界情况 ===\n");
    
    // 创建小内存池配置
    MemoryPoolConfig_t config = {
        .block_size = 32,
        .block_count = 5,
        .enable_memory_pool = true
    };
    
    // 初始化内存池
    MemoryPool_t *pool = memory_pool_init(&config);
    if (!pool) {
        printf("❌ 小内存池初始化失败\n");
        return false;
    }
    
    printf("✅ 小内存池初始化成功\n");
    
    // 分配所有块
    void *ptrs[5];
    for (int i = 0; i < 5; i++) {
        ptrs[i] = memory_pool_alloc(pool);
        if (!ptrs[i]) {
            printf("❌ 第 %d 次内存分配失败\n", i+1);
            // 释放已分配的内存
            for (int j = 0; j < i; j++) {
                memory_pool_free(pool, ptrs[j]);
            }
            memory_pool_deinit(pool);
            return false;
        }
        printf("✅ 第 %d 次内存分配成功: %p\n", i+1, ptrs[i]);
    }
    
    // 测试内存池已满的情况
    void *ptr6 = memory_pool_alloc(pool);
    if (ptr6) {
        printf("❌ 内存池已满时不应分配成功\n");
        memory_pool_free(pool, ptr6);
        // 释放所有内存
        for (int i = 0; i < 5; i++) {
            memory_pool_free(pool, ptrs[i]);
        }
        memory_pool_deinit(pool);
        return false;
    }
    
    printf("✅ 内存池已满测试通过\n");
    
    // 释放所有内存
    for (int i = 0; i < 5; i++) {
        memory_pool_free(pool, ptrs[i]);
        printf("✅ 第 %d 次内存释放成功\n", i+1);
    }
    
    // 释放内存池
    memory_pool_deinit(pool);
    printf("✅ 内存池释放成功\n");
    
    return true;
}

/**
 * @brief 主测试函数
 * @return 测试结果
 */
int main(void) {
    printf("开始内存管理模块单元测试\n\n");
    
    bool result1 = test_memory_pool_init();
    bool result2 = test_memory_allocation();
    bool result3 = test_memory_monitoring();
    bool result4 = test_memory_pool_boundaries();
    
    printf("\n=== 测试结果汇总 ===\n");
    printf("内存池初始化测试: %s\n", result1 ? "✅ 通过" : "❌ 失败");
    printf("内存分配释放测试: %s\n", result2 ? "✅ 通过" : "❌ 失败");
    printf("内存使用监控测试: %s\n", result3 ? "✅ 通过" : "❌ 失败");
    printf("内存池边界测试: %s\n", result4 ? "✅ 通过" : "❌ 失败");
    
    if (result1 && result2 && result3 && result4) {
        printf("\n🎉 所有测试通过!\n");
        return 0;
    } else {
        printf("\n❌ 部分测试失败!\n");
        return 1;
    }
}
