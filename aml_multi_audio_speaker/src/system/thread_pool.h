/**
 * @file thread_pool.h
 * @brief 线程池管理模块
 * @details 提供线程池功能，统一管理线程创建、销毁和任务调度
 * @author AML Audio Team
 * @date 2026-01-22
 */

#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

#include "common_def.h"

/**
 * @brief 线程任务回调函数类型
 */
typedef void (*ThreadPoolTaskFunc)(void *arg);

/**
 * @brief 线程优先级
 */
typedef enum {
    THREAD_PRIORITY_LOW = 0,    // 低优先级
    THREAD_PRIORITY_NORMAL = 1,  // 正常优先级
    THREAD_PRIORITY_HIGH = 2     // 高优先级
} ThreadPriority_t;

/**
 * @brief 线程池初始化
 * @param thread_count 线程池大小
 * @return SUCCESS/FAILURE
 */
int thread_pool_init(int thread_count);

/**
 * @brief 线程池反初始化
 * @return SUCCESS/FAILURE
 */
int thread_pool_deinit(void);

/**
 * @brief 向线程池提交任务
 * @param func 任务回调函数
 * @param arg 任务参数
 * @param priority 任务优先级
 * @return SUCCESS/FAILURE
 */
int thread_pool_submit(ThreadPoolTaskFunc func, void *arg, ThreadPriority_t priority);

/**
 * @brief 获取线程池状态
 * @param pool_name 线程池名称
 * @param active_threads 活跃线程数
 * @param pending_tasks 待处理任务数
 * @return SUCCESS/FAILURE
 */
int thread_pool_get_status(const char *pool_name, int *active_threads, int *pending_tasks);

/**
 * @brief 设置线程池大小
 * @param pool_name 线程池名称
 * @param thread_count 新的线程池大小
 * @return SUCCESS/FAILURE
 */
int thread_pool_set_size(const char *pool_name, int thread_count);

/**
 * @brief 创建线程池
 * @param name 线程池名称
 * @param min_threads 最小线程数
 * @param max_threads 最大线程数
 * @param initial_threads 初始线程数
 * @return SUCCESS/FAILURE
 */
int thread_pool_create(const char *name, int min_threads, int max_threads, int initial_threads);

/**
 * @brief 销毁线程池
 * @param name 线程池名称
 * @return SUCCESS/FAILURE
 */
int thread_pool_destroy(const char *name);

/**
 * @brief 向指定线程池提交任务
 * @param pool_name 线程池名称
 * @param func 任务回调函数
 * @param arg 任务参数
 * @param priority 任务优先级
 * @return SUCCESS/FAILURE
 */
int thread_pool_submit_to(const char *pool_name, ThreadPoolTaskFunc func, void *arg, ThreadPriority_t priority);

/**
 * @brief 获取默认线程池名称
 * @param name 输出参数，默认线程池名称
 * @return SUCCESS/FAILURE
 */
int thread_pool_get_default(const char **name);

/**
 * @brief 设置默认线程池
 * @param name 线程池名称
 * @return SUCCESS/FAILURE
 */
int thread_pool_set_default(const char *name);

#endif /* __THREAD_POOL_H__ */