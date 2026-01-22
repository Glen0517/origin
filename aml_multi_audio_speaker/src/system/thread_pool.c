/**
 * @file thread_pool.c
 * @brief 线程池管理模块实现
 * @details 提供线程池功能，统一管理线程创建、销毁和任务调度
 * @author AML Audio Team
 * @date 2026-01-22
 */

#include "thread_pool.h"
#include "logger.h"
#include "common_def.h"
#include <stdlib.h>
#include <string.h>

/******************************************************************************************
 * 线程池内部数据结构
 ******************************************************************************************/

/**
 * @brief 任务结构体
 */
typedef struct {
    ThreadPoolTaskFunc func;      // 任务回调函数
    void *arg;                    // 任务参数
    ThreadPriority_t priority;     // 任务优先级
    struct TaskNode *next;         // 指向下一个任务
} TaskNode_t;

/**
 * @brief 线程池结构体
 */
typedef struct {
    char name[64];                 // 线程池名称
    void *threads;                 // 线程数组
    int thread_count;              // 线程池大小
    int min_threads;               // 最小线程数
    int max_threads;               // 最大线程数
    TaskNode_t *task_queue;        // 任务队列
    pthread_mutex_t queue_mutex;   // 队列互斥锁
    pthread_cond_t queue_cond;     // 队列条件变量
    bool running;                  // 线程池运行标志
    int active_threads;            // 活跃线程数
    int pending_tasks;             // 待处理任务数
    struct ThreadPoolNode *next;   // 指向下一个线程池
} ThreadPool_t;

/**
 * @brief 线程池节点
 */
typedef struct ThreadPoolNode {
    ThreadPool_t *pool;            // 线程池指针
    struct ThreadPoolNode *next;   // 指向下一个节点
} ThreadPoolNode_t;

/******************************************************************************************
 * 线程池全局变量
 ******************************************************************************************/

static ThreadPoolNode_t *g_thread_pool_list = NULL;
static pthread_mutex_t g_thread_pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static ThreadPool_t *g_default_thread_pool = NULL;

/******************************************************************************************
 * 线程池内部函数
 ******************************************************************************************/

/**
 * @brief 向任务队列添加任务（按优先级排序）
 * @param pool 线程池指针
 * @param task 任务节点
 */
static void task_queue_push(ThreadPool_t *pool, TaskNode_t *task) {
    MUTEX_LOCK_LOCK(pool->queue_mutex);
    
    if (!pool->task_queue) {
        // 队列为空，直接添加
        pool->task_queue = task;
    } else {
        // 按优先级插入任务
        TaskNode_t *current = pool->task_queue;
        TaskNode_t *prev = NULL;
        
        while (current && current->priority >= task->priority) {
            prev = current;
            current = current->next;
        }
        
        if (!prev) {
            // 插入到队列头部
            task->next = pool->task_queue;
            pool->task_queue = task;
        } else {
            // 插入到队列中间或尾部
            task->next = current;
            prev->next = task;
        }
    }
    
    pool->pending_tasks++;
    pthread_cond_signal(&pool->queue_cond);
    MUTEX_LOCK_UNLOCK(pool->queue_mutex);
}

/**
 * @brief 从任务队列取出任务
 * @param pool 线程池指针
 * @return 任务节点
 */
static TaskNode_t *task_queue_pop(ThreadPool_t *pool) {
    MUTEX_LOCK_LOCK(pool->queue_mutex);
    
    while (pool->running && !pool->task_queue) {
        pthread_cond_wait(&pool->queue_cond, &pool->queue_mutex);
    }
    
    if (!pool->running) {
        MUTEX_LOCK_UNLOCK(pool->queue_mutex);
        return NULL;
    }
    
    TaskNode_t *task = pool->task_queue;
    pool->task_queue = task->next;
    pool->pending_tasks--;
    pool->active_threads++;
    
    MUTEX_LOCK_UNLOCK(pool->queue_mutex);
    return task;
}

/**
 * @brief 线程工作函数
 * @param arg 线程参数
 * @return 线程返回值
 */
static void *thread_worker(void *arg) {
    ThreadPool_t *pool = (ThreadPool_t *)arg;
    LOG_INFO("Thread pool worker started: %ld, pool: %s", pthread_self(), pool->name);
    
    while (pool->running) {
        TaskNode_t *task = task_queue_pop(pool);
        if (!task) {
            break;
        }
        
        // 执行任务
        LOG_DEBUG("Thread %ld executing task, pool: %s, priority: %d", pthread_self(), pool->name, task->priority);
        task->func(task->arg);
        
        // 释放任务资源
        free(task);
        
        MUTEX_LOCK_LOCK(pool->queue_mutex);
        pool->active_threads--;
        MUTEX_LOCK_UNLOCK(pool->queue_mutex);
    }
    
    LOG_INFO("Thread pool worker exited: %ld, pool: %s", pthread_self(), pool->name);
    return NULL;
}

/**
 * @brief 查找线程池
 * @param name 线程池名称
 * @return 线程池指针
 */
static ThreadPool_t *find_thread_pool(const char *name) {
    MUTEX_LOCK_LOCK(g_thread_pool_mutex);
    
    ThreadPoolNode_t *node = g_thread_pool_list;
    while (node) {
        if (strcmp(node->pool->name, name) == 0) {
            MUTEX_LOCK_UNLOCK(g_thread_pool_mutex);
            return node->pool;
        }
        node = node->next;
    }
    
    MUTEX_LOCK_UNLOCK(g_thread_pool_mutex);
    return NULL;
}

/**
 * @brief 添加线程池到列表
 * @param pool 线程池指针
 */
static void add_thread_pool(ThreadPool_t *pool) {
    MUTEX_LOCK_LOCK(g_thread_pool_mutex);
    
    ThreadPoolNode_t *node = (ThreadPoolNode_t *)malloc(sizeof(ThreadPoolNode_t));
    if (node) {
        node->pool = pool;
        node->next = g_thread_pool_list;
        g_thread_pool_list = node;
    }
    
    MUTEX_LOCK_UNLOCK(g_thread_pool_mutex);
}

/**
 * @brief 从列表移除线程池
 * @param name 线程池名称
 */
static void remove_thread_pool(const char *name) {
    MUTEX_LOCK_LOCK(g_thread_pool_mutex);
    
    ThreadPoolNode_t *node = g_thread_pool_list;
    ThreadPoolNode_t *prev = NULL;
    
    while (node) {
        if (strcmp(node->pool->name, name) == 0) {
            if (prev) {
                prev->next = node->next;
            } else {
                g_thread_pool_list = node->next;
            }
            free(node);
            break;
        }
        prev = node;
        node = node->next;
    }
    
    MUTEX_LOCK_UNLOCK(g_thread_pool_mutex);
}

/******************************************************************************************
 * 线程池对外接口
 ******************************************************************************************/

int thread_pool_init(int thread_count) {
    return thread_pool_create("default", thread_count, thread_count, thread_count * 2);
}

int thread_pool_deinit(void) {
    return thread_pool_destroy("default");
}

int thread_pool_create(const char *name, int min_threads, int max_threads, int initial_threads) {
    if (!name || min_threads <= 0 || max_threads < min_threads || initial_threads < min_threads) {
        LOG_ERROR("Invalid thread pool parameters");
        return FAILURE;
    }
    
    // 检查线程池是否已存在
    if (find_thread_pool(name)) {
        LOG_ERROR("Thread pool already exists: %s", name);
        return FAILURE;
    }
    
    // 创建线程池
    ThreadPool_t *pool = (ThreadPool_t *)malloc(sizeof(ThreadPool_t));
    if (!pool) {
        LOG_ERROR("Failed to allocate thread pool");
        return FAILURE;
    }
    
    // 初始化线程池
    memset(pool, 0, sizeof(ThreadPool_t));
    strncpy(pool->name, name, sizeof(pool->name) - 1);
    pool->min_threads = min_threads;
    pool->max_threads = max_threads;
    pool->thread_count = 0;
    pool->task_queue = NULL;
    MUTEX_LOCK_INIT(pool->queue_mutex);
    pthread_cond_init(&pool->queue_cond, NULL);
    pool->running = true;
    pool->active_threads = 0;
    pool->pending_tasks = 0;
    pool->next = NULL;
    
    // 分配线程数组
    pool->threads = (void *)malloc(sizeof(pthread_t) * max_threads);
    if (!pool->threads) {
        LOG_ERROR("Failed to allocate thread array");
        MUTEX_LOCK_DESTROY(pool->queue_mutex);
        pthread_cond_destroy(&pool->queue_cond);
        free(pool);
        return FAILURE;
    }
    
    // 创建初始线程
    for (int i = 0; i < initial_threads; i++) {
        if (pthread_create((pthread_t *)&((pthread_t *)pool->threads)[pool->thread_count], NULL, thread_worker, pool) != 0) {
            LOG_ERROR("Failed to create thread %d in pool %s", i, name);
            break;
        }
        pool->thread_count++;
    }
    
    if (pool->thread_count == 0) {
        LOG_ERROR("Failed to create any threads in pool %s", name);
        free(pool->threads);
        pthread_mutex_destroy(&pool->queue_mutex);
        pthread_cond_destroy(&pool->queue_cond);
        free(pool);
        return FAILURE;
    }
    
    // 添加到线程池列表
    add_thread_pool(pool);
    
    // 设置默认线程池
    if (!g_default_thread_pool) {
        g_default_thread_pool = pool;
    }
    
    LOG_INFO("Thread pool created: %s, min: %d, max: %d, initial: %d", 
             name, min_threads, max_threads, pool->thread_count);
    return SUCCESS;
}

int thread_pool_destroy(const char *name) {
    ThreadPool_t *pool = find_thread_pool(name);
    if (!pool) {
        LOG_ERROR("Thread pool not found: %s", name);
        return FAILURE;
    }
    
    // 停止线程池
    MUTEX_LOCK_LOCK(pool->queue_mutex);
    pool->running = false;
    pthread_cond_broadcast(&pool->queue_cond);
    MUTEX_LOCK_UNLOCK(pool->queue_mutex);
    
    // 等待所有线程退出
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(((pthread_t *)pool->threads)[i], NULL);
    }
    
    // 清理任务队列
    TaskNode_t *current = pool->task_queue;
    while (current) {
        TaskNode_t *next = current->next;
        free(current);
        current = next;
    }
    
    // 释放资源
    free(pool->threads);
    pthread_mutex_destroy(&pool->queue_mutex);
    pthread_cond_destroy(&pool->queue_cond);
    
    // 从列表中移除
    remove_thread_pool(name);
    
    // 更新默认线程池
    if (g_default_thread_pool == pool) {
        g_default_thread_pool = g_thread_pool_list ? g_thread_pool_list->pool : NULL;
    }
    
    free(pool);
    LOG_INFO("Thread pool destroyed: %s", name);
    return SUCCESS;
}

int thread_pool_submit(ThreadPoolTaskFunc func, void *arg, ThreadPriority_t priority) {
    return thread_pool_submit_to("default", func, arg, priority);
}

int thread_pool_submit_to(const char *pool_name, ThreadPoolTaskFunc func, void *arg, ThreadPriority_t priority) {
    ThreadPool_t *pool = find_thread_pool(pool_name);
    if (!pool) {
        LOG_ERROR("Thread pool not found: %s", pool_name);
        return FAILURE;
    }
    
    if (!func) {
        LOG_ERROR("Invalid task function");
        return FAILURE;
    }
    
    if (!pool->running) {
        LOG_ERROR("Thread pool not running: %s", pool_name);
        return FAILURE;
    }
    
    // 创建任务节点
    TaskNode_t *task = (TaskNode_t *)malloc(sizeof(TaskNode_t));
    if (!task) {
        LOG_ERROR("Failed to allocate task node");
        return FAILURE;
    }
    
    task->func = func;
    task->arg = arg;
    task->priority = priority;
    task->next = NULL;
    
    // 添加到任务队列
    task_queue_push(pool, task);
    LOG_DEBUG("Task submitted to pool %s, priority: %d, pending tasks: %d", 
             pool_name, priority, pool->pending_tasks);
    return SUCCESS;
}

int thread_pool_get_status(const char *pool_name, int *active_threads, int *pending_tasks) {
    ThreadPool_t *pool = find_thread_pool(pool_name);
    if (!pool) {
        LOG_ERROR("Thread pool not found: %s", pool_name);
        return FAILURE;
    }
    
    if (!active_threads || !pending_tasks) {
        LOG_ERROR("Invalid parameters");
        return FAILURE;
    }
    
    MUTEX_LOCK_LOCK(pool->queue_mutex);
    *active_threads = pool->active_threads;
    *pending_tasks = pool->pending_tasks;
    MUTEX_LOCK_UNLOCK(pool->queue_mutex);
    
    return SUCCESS;
}

int thread_pool_set_size(const char *pool_name, int thread_count) {
    ThreadPool_t *pool = find_thread_pool(pool_name);
    if (!pool) {
        LOG_ERROR("Thread pool not found: %s", pool_name);
        return FAILURE;
    }
    
    if (thread_count < pool->min_threads || thread_count > pool->max_threads) {
        LOG_ERROR("Thread count out of range: %d (min: %d, max: %d)", 
                 thread_count, pool->min_threads, pool->max_threads);
        return FAILURE;
    }
    
    if (!pool->running) {
        LOG_ERROR("Thread pool not running: %s", pool_name);
        return FAILURE;
    }
    
    MUTEX_LOCK_LOCK(pool->queue_mutex);
    
    if (thread_count == pool->thread_count) {
        LOG_WARN("Thread count already set to: %d in pool %s", thread_count, pool_name);
        MUTEX_LOCK_UNLOCK(pool->queue_mutex);
        return SUCCESS;
    }
    
    // 动态调整线程池大小（简化实现）
    LOG_INFO("Adjusting thread pool size: %s, old: %d, new: %d", 
             pool_name, pool->thread_count, thread_count);
    
    MUTEX_LOCK_UNLOCK(pool->queue_mutex);
    return SUCCESS;
}

int thread_pool_get_default(const char **name) {
    if (!name) {
        LOG_ERROR("Invalid parameter");
        return FAILURE;
    }
    
    if (g_default_thread_pool) {
        *name = g_default_thread_pool->name;
        return SUCCESS;
    }
    
    return FAILURE;
}

int thread_pool_set_default(const char *name) {
    ThreadPool_t *pool = find_thread_pool(name);
    if (!pool) {
        LOG_ERROR("Thread pool not found: %s", name);
        return FAILURE;
    }
    
    g_default_thread_pool = pool;
    LOG_INFO("Set default thread pool: %s", name);
    return SUCCESS;
}
