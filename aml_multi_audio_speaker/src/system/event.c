/**
 * @file event.c
 * @brief 应用层事件系统实现
 * @details 实现应用层模块间的事件广播与解耦，提供线程安全的事件通信机制
 * @author AML Audio Team
 * @date 2026-01-15
 */

#include "event.h"
#include "logger.h"
#include <pthread.h>
#include <string.h>

/******************************************************************************************
 * 事件系统内部数据结构
 ******************************************************************************************/

/**
 * @brief 事件结构体
 */
typedef struct {
    int event_type;      // 事件类型
    void *event_data;    // 事件数据
    bool need_free;      // 事件数据是否需要释放
} Event_t;

/**
 * @brief 事件订阅信息结构体
 */
typedef struct {
    EventCallback_t callback;  // 事件回调函数
    void *user_data;          // 用户自定义数据
    bool is_valid;            // 订阅是否有效
    int priority;             // 回调优先级（0-9，数字越小优先级越高）
} EventSubscription_t;

/**
 * @brief 事件队列节点
 */
typedef struct EventQueueNode {
    Event_t event;
    struct EventQueueNode *next;
} EventQueueNode_t;

/******************************************************************************************
 * 事件系统全局变量
 ******************************************************************************************/

// 事件队列（改为链表实现）
static EventQueueNode_t *g_event_queue_head = NULL;
static EventQueueNode_t *g_event_queue_tail = NULL;
static int g_queue_size = 0;         // 当前队列大小
static int g_max_queue_size = MAX_EVENT_QUEUE_SIZE;  // 最大队列大小

// 事件注册中心：[事件类型][订阅者索引] -> 订阅信息
static EventSubscription_t g_event_subscriptions[MAX_EVENT_TYPE][MAX_SUBSCRIBERS_PER_EVENT];
static int g_subscriber_counts[MAX_EVENT_TYPE] = {0};  // 每个事件类型的订阅者数量

// 线程安全机制
static pthread_mutex_t g_event_mutex = PTHREAD_MUTEX_INITIALIZER;  // 保护事件队列和注册中心
static pthread_cond_t g_event_cond = PTHREAD_COND_INITIALIZER;    // 事件条件变量
static pthread_t g_event_thread;                                  // 事件处理线程
static bool g_event_thread_running = false;                       // 事件线程运行标志
static int g_event_thread_priority = 0;                           // 事件线程优先级

/******************************************************************************************
 * 事件系统内部函数
 ******************************************************************************************/

/**
 * @brief 异步回调线程函数
 * @param arg 线程参数
 * @return 线程返回值
 */
static void *async_callback_thread(void *arg) {
    typedef struct {
        Event_t *event;
        EventCallback_t callback;
        void *user_data;
    } AsyncCallbackArgs_t;
    
    AsyncCallbackArgs_t *args = (AsyncCallbackArgs_t *)arg;
    // 调用回调函数
    args->callback(args->event->event_type, 
                  args->event->event_data, 
                  args->user_data);
    // 释放资源
    free(args->event);
    free(args);
    return NULL;
}

/**
 * @brief 事件队列是否为空
 * @return true-为空，false-不为空
 */
static bool event_queue_is_empty(void) {
    return g_queue_size == 0;
}

/**
 * @brief 事件队列是否已满
 * @return true-已满，false-未满
 */
static bool event_queue_is_full(void) {
    return g_queue_size >= g_max_queue_size;
}

/**
 * @brief 将事件加入队列
 * @param event 事件结构体
 * @return SUCCESS/FAILURE
 */
static int event_queue_push(Event_t *event) {
    if (event_queue_is_full()) {
        LOG_WARN("Event queue is full, dropping event: %d", event->event_type);
        // 尝试清理队列头部的事件以腾出空间
        if (g_event_queue_head) {
            EventQueueNode_t *temp = g_event_queue_head;
            g_event_queue_head = g_event_queue_head->next;
            if (!g_event_queue_head) {
                g_event_queue_tail = NULL;
            }
            // 释放事件数据（如果需要）
            if (temp->event.need_free && temp->event.event_data) {
                free(temp->event.event_data);
            }
            free(temp);
            g_queue_size--;
            LOG_WARN("Event queue cleaned, freed 1 event");
        } else {
            return FAILURE;
        }
    }

    // 创建新的队列节点
    EventQueueNode_t *node = (EventQueueNode_t *)malloc(sizeof(EventQueueNode_t));
    if (!node) {
        LOG_ERROR("Failed to allocate event queue node");
        return FAILURE;
    }

    // 复制事件数据
    node->event = *event;
    node->next = NULL;

    // 将节点加入队列尾部
    if (!g_event_queue_head) {
        g_event_queue_head = node;
        g_event_queue_tail = node;
    } else {
        g_event_queue_tail->next = node;
        g_event_queue_tail = node;
    }

    g_queue_size++;
    LOG_DEBUG("Event pushed: %d, queue size: %d", event->event_type, g_queue_size);
    return SUCCESS;
}

/**
 * @brief 从队列取出事件
 * @param event 用于存储取出的事件
 * @return SUCCESS/FAILURE
 */
static int event_queue_pop(Event_t *event) {
    if (event_queue_is_empty()) {
        return FAILURE;
    }

    // 取出队列头部节点
    EventQueueNode_t *temp = g_event_queue_head;
    *event = temp->event;

    // 更新队列头指针
    g_event_queue_head = g_event_queue_head->next;
    if (!g_event_queue_head) {
        g_event_queue_tail = NULL;
    }

    // 释放节点内存
    free(temp);
    g_queue_size--;

    LOG_DEBUG("Event popped: %d, queue size: %d", event->event_type, g_queue_size);
    return SUCCESS;
}

/**
 * @brief 清理事件队列
 * @return 清理的事件数量
 */
static int event_queue_clear(void) {
    int count = 0;
    EventQueueNode_t *current = g_event_queue_head;
    EventQueueNode_t *next = NULL;

    while (current) {
        next = current->next;
        // 释放事件数据（如果需要）
        if (current->event.need_free && current->event.event_data) {
            free(current->event.event_data);
        }
        free(current);
        current = next;
        count++;
    }

    g_event_queue_head = NULL;
    g_event_queue_tail = NULL;
    g_queue_size = 0;

    LOG_INFO("Event queue cleared, %d events freed", count);
    return count;
}

/**
 * @brief 事件处理线程函数
 * @param arg 线程参数
 * @return 线程返回值
 */
static void *event_process_thread(void *arg) {
    Event_t event;
    int i;

    LOG_INFO("Event processing thread started");

    // 设置线程优先级
    struct sched_param param;
    param.sched_priority = 50;  // 设置中等优先级
    pthread_setschedparam(pthread_self(), SCHED_RR, &param);

    while (g_event_thread_running) {
        pthread_mutex_lock(&g_event_mutex);

        // 等待事件，队列为空时阻塞
        while (event_queue_is_empty() && g_event_thread_running) {
            pthread_cond_wait(&g_event_cond, &g_event_mutex);
        }

        // 如果线程要退出，跳出循环
        if (!g_event_thread_running) {
            pthread_mutex_unlock(&g_event_mutex);
            break;
        }

        // 从队列取出一个事件
        if (event_queue_pop(&event) == SUCCESS) {
            pthread_mutex_unlock(&g_event_mutex);

            // 分发事件给所有订阅者
            LOG_DEBUG("Dispatching event: %d, queue size: %d", event.event_type, g_queue_size);

            // 加锁保护注册中心，获取订阅者信息的副本
            pthread_mutex_lock(&g_event_mutex);
            int subscriber_count = g_subscriber_counts[event.event_type];
            // 创建订阅者信息副本，避免长时间持有锁
            EventSubscription_t temp_subscriptions[MAX_SUBSCRIBERS_PER_EVENT];
            memcpy(temp_subscriptions, g_event_subscriptions[event.event_type], 
                  sizeof(EventSubscription_t) * subscriber_count);
            pthread_mutex_unlock(&g_event_mutex);

            // 按优先级排序订阅者（从高到低）
            for (int j = 0; j < subscriber_count - 1; j++) {
                for (int k = 0; k < subscriber_count - 1 - j; k++) {
                    if (temp_subscriptions[k].priority > temp_subscriptions[k+1].priority) {
                        EventSubscription_t temp = temp_subscriptions[k];
                        temp_subscriptions[k] = temp_subscriptions[k+1];
                        temp_subscriptions[k+1] = temp;
                    }
                }
            }

            // 遍历订阅者，异步执行回调函数
            for (i = 0; i < subscriber_count; i++) {
                if (temp_subscriptions[i].is_valid && temp_subscriptions[i].callback != NULL) {
                    // 创建事件数据副本，用于异步处理
                    Event_t *async_event = (Event_t *)malloc(sizeof(Event_t));
                    if (async_event != NULL) {
                        *async_event = event;
                        // 创建回调参数结构体
                        typedef struct {
                            Event_t *event;
                            EventCallback_t callback;
                            void *user_data;
                        } AsyncCallbackArgs_t;
                        AsyncCallbackArgs_t *args = (AsyncCallbackArgs_t *)malloc(sizeof(AsyncCallbackArgs_t));
                        if (args != NULL) {
                            args->event = async_event;
                            args->callback = temp_subscriptions[i].callback;
                            args->user_data = temp_subscriptions[i].user_data;
                            
                        // 创建线程执行回调函数
                        pthread_t callback_thread;
                        pthread_create(&callback_thread, NULL, 
                                     async_callback_thread, args);
                            pthread_detach(callback_thread); // 分离线程，自动释放资源
                        } else {
                            free(async_event);
                            // 如果内存分配失败，同步执行回调
                            temp_subscriptions[i].callback(event.event_type, event.event_data, 
                                                        temp_subscriptions[i].user_data);
                        }
                    } else {
                        // 如果内存分配失败，同步执行回调
                        temp_subscriptions[i].callback(event.event_type, event.event_data, 
                                                    temp_subscriptions[i].user_data);
                    }
                }
            }

            // 释放原始事件数据（如果需要）
            if (event.need_free && event.event_data) {
                free(event.event_data);
                LOG_DEBUG("Freed event data for event: %d", event.event_type);
            }
        } else {
            pthread_mutex_unlock(&g_event_mutex);
        }
    }

    LOG_INFO("Event processing thread exited");
    return NULL;
}

/**
 * @brief 分发事件给所有订阅者（同步调用）
 * @param event_type 事件类型
 * @param data 事件数据
 * @return SUCCESS/FAILURE
 */
static int event_dispatch(int event_type, void *data) {
    int i;

    pthread_mutex_lock(&g_event_mutex);

    // 检查事件类型是否有效
    if (event_type <= 0 || event_type >= MAX_EVENT_TYPE) {
        pthread_mutex_unlock(&g_event_mutex);
        LOG_ERROR("Invalid event type: %d", event_type);
        return INVALID_PARAM;
    }

    // 遍历该事件类型的所有订阅者
    int subscriber_count = g_subscriber_counts[event_type];
    for (i = 0; i < subscriber_count; i++) {
        EventSubscription_t *sub = &g_event_subscriptions[event_type][i];
        if (sub->is_valid && sub->callback != NULL) {
            // 解锁，避免回调函数中再次调用event_notify时死锁
            pthread_mutex_unlock(&g_event_mutex);

            // 调用回调函数
            LOG_DEBUG("Calling sync callback for event: %d, subscriber: %d", event_type, i);
            sub->callback(event_type, data, sub->user_data);

            // 重新加锁
            pthread_mutex_lock(&g_event_mutex);
        }
    }

    pthread_mutex_unlock(&g_event_mutex);
    return SUCCESS;
}

/******************************************************************************************
 * 事件系统对外接口实现
 ******************************************************************************************/

int event_system_init(void) {
    int i, j;
    int ret;

    LOG_INFO("Initializing event system...");

    // 1. 初始化事件队列
    g_event_queue_head = NULL;
    g_event_queue_tail = NULL;
    g_queue_size = 0;
    g_max_queue_size = MAX_EVENT_QUEUE_SIZE;

    // 2. 初始化事件注册中心
    for (i = 0; i < MAX_EVENT_TYPE; i++) {
        g_subscriber_counts[i] = 0;
        for (j = 0; j < MAX_SUBSCRIBERS_PER_EVENT; j++) {
            g_event_subscriptions[i][j].callback = NULL;
            g_event_subscriptions[i][j].user_data = NULL;
            g_event_subscriptions[i][j].is_valid = false;
            g_event_subscriptions[i][j].priority = 5;  // 默认优先级
        }
    }

    // 3. 创建事件处理线程
    g_event_thread_running = true;
    ret = pthread_create(&g_event_thread, NULL, event_process_thread, NULL);
    if (ret != 0) {
        LOG_ERROR("Failed to create event thread: %d", ret);
        g_event_thread_running = false;
        return FAILURE;
    }

    LOG_INFO("Event system initialized successfully");
    return SUCCESS;
}

int event_system_deinit(void) {
    int ret;

    LOG_INFO("Deinitializing event system...");

    // 1. 停止事件处理线程
    pthread_mutex_lock(&g_event_mutex);
    g_event_thread_running = false;
    pthread_cond_signal(&g_event_cond);  // 唤醒等待的线程
    pthread_mutex_unlock(&g_event_mutex);

    // 2. 等待线程退出
    ret = pthread_join(g_event_thread, NULL);
    if (ret != 0) {
        LOG_ERROR("Failed to join event thread: %d", ret);
        // 继续清理其他资源
    }

    // 3. 清理事件队列
    event_queue_clear();

    // 4. 清理事件注册中心
    for (int i = 0; i < MAX_EVENT_TYPE; i++) {
        g_subscriber_counts[i] = 0;
        for (int j = 0; j < MAX_SUBSCRIBERS_PER_EVENT; j++) {
            g_event_subscriptions[i][j].callback = NULL;
            g_event_subscriptions[i][j].user_data = NULL;
            g_event_subscriptions[i][j].is_valid = false;
            g_event_subscriptions[i][j].priority = 5;
        }
    }

    LOG_INFO("Event system deinitialized successfully");
    return SUCCESS;
}

int event_subscribe(int event_type, EventCallback_t callback, void *user_data) {
    return event_subscribe_with_priority(event_type, callback, user_data, 5);  // 默认优先级
}

/**
 * @brief 订阅事件（带优先级）
 * @param event_type 事件类型
 * @param callback 事件回调函数
 * @param user_data 用户自定义数据
 * @param priority 回调优先级（0-9，数字越小优先级越高）
 * @return SUCCESS/FAILURE/INVALID_PARAM
 */
int event_subscribe_with_priority(int event_type, EventCallback_t callback, void *user_data, int priority) {
    int i;

    if (event_type <= 0 || event_type >= MAX_EVENT_TYPE) {
        LOG_ERROR("Invalid event type: %d", event_type);
        return INVALID_PARAM;
    }

    if (callback == NULL) {
        LOG_ERROR("Invalid callback function");
        return INVALID_PARAM;
    }

    if (priority < 0 || priority > 9) {
        LOG_WARN("Invalid priority: %d, using default", priority);
        priority = 5;
    }

    pthread_mutex_lock(&g_event_mutex);

    // 检查是否已经订阅过该事件
    for (i = 0; i < g_subscriber_counts[event_type]; i++) {
        if (g_event_subscriptions[event_type][i].callback == callback) {
            LOG_WARN("Callback already subscribed to event: %d", event_type);
            // 更新优先级
            g_event_subscriptions[event_type][i].priority = priority;
            pthread_mutex_unlock(&g_event_mutex);
            return SUCCESS;  // 已经订阅，直接返回成功
        }
    }

    // 检查订阅者数量是否达到上限
    if (g_subscriber_counts[event_type] >= MAX_SUBSCRIBERS_PER_EVENT) {
        LOG_ERROR("Too many subscribers for event: %d, max: %d", 
                 event_type, MAX_SUBSCRIBERS_PER_EVENT);
        pthread_mutex_unlock(&g_event_mutex);
        return FAILURE;
    }

    // 找到一个可用的订阅者位置
    int subscriber_idx = g_subscriber_counts[event_type];
    g_event_subscriptions[event_type][subscriber_idx].callback = callback;
    g_event_subscriptions[event_type][subscriber_idx].user_data = user_data;
    g_event_subscriptions[event_type][subscriber_idx].is_valid = true;
    g_event_subscriptions[event_type][subscriber_idx].priority = priority;
    g_subscriber_counts[event_type]++;

    LOG_INFO("Subscribed to event: %d, subscriber count: %d, priority: %d", 
             event_type, g_subscriber_counts[event_type], priority);

    pthread_mutex_unlock(&g_event_mutex);
    return SUCCESS;
}

int event_unsubscribe(int event_type, EventCallback_t callback) {
    int i;
    bool found = false;

    if (event_type <= 0 || event_type >= MAX_EVENT_TYPE) {
        LOG_ERROR("Invalid event type: %d", event_type);
        return INVALID_PARAM;
    }

    if (callback == NULL) {
        LOG_ERROR("Invalid callback function");
        return INVALID_PARAM;
    }

    pthread_mutex_lock(&g_event_mutex);

    // 查找要取消订阅的回调函数
    for (i = 0; i < g_subscriber_counts[event_type]; i++) {
        if (g_event_subscriptions[event_type][i].callback == callback) {
            // 标记为无效
            g_event_subscriptions[event_type][i].is_valid = false;
            g_event_subscriptions[event_type][i].callback = NULL;
            g_event_subscriptions[event_type][i].user_data = NULL;
            found = true;

            LOG_INFO("Unsubscribed from event: %d, subscriber index: %d", event_type, i);
            break;
        }
    }

    if (!found) {
        LOG_WARN("Callback not found for event: %d", event_type);
        pthread_mutex_unlock(&g_event_mutex);
        return FAILURE;
    }

    // 如果是最后一个订阅者，直接减少计数
    if (i == g_subscriber_counts[event_type] - 1) {
        g_subscriber_counts[event_type]--;
    } else {
        // 否则，将最后一个订阅者移动到当前位置，保持紧凑
        g_event_subscriptions[event_type][i] = 
            g_event_subscriptions[event_type][g_subscriber_counts[event_type] - 1];
        g_subscriber_counts[event_type]--;
    }

    LOG_INFO("Unsubscribe completed, remaining subscribers: %d", 
             g_subscriber_counts[event_type]);

    pthread_mutex_unlock(&g_event_mutex);
    return SUCCESS;
}

int event_notify(int event_type, void *data) {
    return event_notify_with_free(event_type, data, false);
}

/**
 * @brief 发布事件（可指定是否需要释放数据）
 * @param event_type 事件类型
 * @param data 事件数据
 * @param need_free 事件数据是否需要自动释放
 * @return SUCCESS/FAILURE/INVALID_PARAM
 */
int event_notify_with_free(int event_type, void *data, bool need_free) {
    Event_t event;
    int ret;

    // 检查事件类型是否有效
    if (event_type <= 0 || event_type >= MAX_EVENT_TYPE) {
        LOG_ERROR("Invalid event type: %d", event_type);
        return INVALID_PARAM;
    }

    // 填充事件结构体
    event.event_type = event_type;
    event.event_data = data;
    event.need_free = need_free;

    pthread_mutex_lock(&g_event_mutex);

    // 将事件加入队列
    ret = event_queue_push(&event);
    if (ret == SUCCESS) {
        // 通知事件处理线程有新事件
        pthread_cond_signal(&g_event_cond);
    } else if (need_free && data) {
        // 如果事件入队失败且需要释放数据，手动释放
        free(data);
        LOG_DEBUG("Freed event data after queue failure: %d", event_type);
    }

    pthread_mutex_unlock(&g_event_mutex);

    LOG_DEBUG("Event notified: %d, data: %p, need_free: %d, queue size: %d", 
             event_type, data, need_free, g_queue_size);

    return ret;
}

int event_notify_sync(int event_type, void *data) {
    // 检查事件类型是否有效
    if (event_type <= 0 || event_type >= MAX_EVENT_TYPE) {
        LOG_ERROR("Invalid event type: %d", event_type);
        return INVALID_PARAM;
    }

    LOG_DEBUG("Sync event notified: %d, data: %p", event_type, data);

    // 直接分发事件，同步调用所有订阅者
    return event_dispatch(event_type, data);
}

int event_get_queue_status(int *queue_size) {
    if (queue_size == NULL) {
        LOG_ERROR("Invalid parameter: queue_size is NULL");
        return INVALID_PARAM;
    }

    pthread_mutex_lock(&g_event_mutex);
    *queue_size = g_queue_size;
    pthread_mutex_unlock(&g_event_mutex);

    return SUCCESS;
}

/**
 * @brief 设置事件队列最大大小
 * @details 用于动态调整事件队列大小
 * @param max_size 最大队列大小
 * @return SUCCESS/FAILURE
 */
int event_set_max_queue_size(int max_size) {
    if (max_size <= 0) {
        LOG_ERROR("Invalid max queue size: %d", max_size);
        return FAILURE;
    }

    pthread_mutex_lock(&g_event_mutex);
    g_max_queue_size = max_size;
    pthread_mutex_unlock(&g_event_mutex);

    LOG_INFO("Event queue max size set to: %d", max_size);
    return SUCCESS;
}
