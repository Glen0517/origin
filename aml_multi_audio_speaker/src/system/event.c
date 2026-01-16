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
} Event_t;

/**
 * @brief 事件订阅信息结构体
 */
typedef struct {
    EventCallback_t callback;  // 事件回调函数
    void *user_data;          // 用户自定义数据
    bool is_valid;            // 订阅是否有效
} EventSubscription_t;

/******************************************************************************************
 * 事件系统全局变量
 ******************************************************************************************/

// 事件队列
static Event_t g_event_queue[MAX_EVENT_QUEUE_SIZE];
static int g_queue_head = 0;         // 队列头指针
static int g_queue_tail = 0;         // 队列尾指针
static int g_queue_size = 0;         // 当前队列大小

// 事件注册中心：[事件类型][订阅者索引] -> 订阅信息
static EventSubscription_t g_event_subscriptions[MAX_EVENT_TYPE][MAX_SUBSCRIBERS_PER_EVENT];
static int g_subscriber_counts[MAX_EVENT_TYPE] = {0};  // 每个事件类型的订阅者数量

// 线程安全机制
static pthread_mutex_t g_event_mutex = PTHREAD_MUTEX_INITIALIZER;  // 保护事件队列和注册中心
static pthread_cond_t g_event_cond = PTHREAD_COND_INITIALIZER;    // 事件条件变量
static pthread_t g_event_thread;                                  // 事件处理线程
static bool g_event_thread_running = false;                       // 事件线程运行标志

/******************************************************************************************
 * 事件系统内部函数
 ******************************************************************************************/

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
    return g_queue_size >= MAX_EVENT_QUEUE_SIZE;
}

/**
 * @brief 将事件加入队列
 * @param event 事件结构体
 * @return SUCCESS/FAILURE
 */
static int event_queue_push(Event_t *event) {
    if (event_queue_is_full()) {
        LOG_WARN("Event queue is full, dropping event: %d", event->event_type);
        return FAILURE;
    }

    // 将事件加入队列尾部
    g_event_queue[g_queue_tail] = *event;
    g_queue_tail = (g_queue_tail + 1) % MAX_EVENT_QUEUE_SIZE;
    g_queue_size++;

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

    // 从队列头部取出事件
    *event = g_event_queue[g_queue_head];
    g_queue_head = (g_queue_head + 1) % MAX_EVENT_QUEUE_SIZE;
    g_queue_size--;

    return SUCCESS;
}

/**
 * @brief 事件处理线程函数
 * @param arg 线程参数
 * @return 线程返回值
 */
static void *event_process_thread(void *arg) {
    Event_t event;
    int i, j;

    LOG_INFO("Event processing thread started");

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

            // 加锁保护注册中心
            pthread_mutex_lock(&g_event_mutex);

            // 遍历该事件类型的所有订阅者
            int subscriber_count = g_subscriber_counts[event.event_type];
            for (i = 0; i < subscriber_count; i++) {
                EventSubscription_t *sub = &g_event_subscriptions[event.event_type][i];
                if (sub->is_valid && sub->callback != NULL) {
                    // 解锁，避免回调函数中再次调用event_notify时死锁
                    pthread_mutex_unlock(&g_event_mutex);

                    // 调用回调函数
                    LOG_DEBUG("Calling callback for event: %d, subscriber: %d", event.event_type, i);
                    sub->callback(event.event_type, event.event_data, sub->user_data);

                    // 重新加锁
                    pthread_mutex_lock(&g_event_mutex);
                }
            }
        }

        pthread_mutex_unlock(&g_event_mutex);
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
    memset(g_event_queue, 0, sizeof(g_event_queue));
    g_queue_head = 0;
    g_queue_tail = 0;
    g_queue_size = 0;

    // 2. 初始化事件注册中心
    for (i = 0; i < MAX_EVENT_TYPE; i++) {
        g_subscriber_counts[i] = 0;
        for (j = 0; j < MAX_SUBSCRIBERS_PER_EVENT; j++) {
            g_event_subscriptions[i][j].callback = NULL;
            g_event_subscriptions[i][j].user_data = NULL;
            g_event_subscriptions[i][j].is_valid = false;
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
    memset(g_event_queue, 0, sizeof(g_event_queue));
    g_queue_head = 0;
    g_queue_tail = 0;
    g_queue_size = 0;

    // 4. 清理事件注册中心
    for (int i = 0; i < MAX_EVENT_TYPE; i++) {
        g_subscriber_counts[i] = 0;
        for (int j = 0; j < MAX_SUBSCRIBERS_PER_EVENT; j++) {
            g_event_subscriptions[i][j].callback = NULL;
            g_event_subscriptions[i][j].user_data = NULL;
            g_event_subscriptions[i][j].is_valid = false;
        }
    }

    LOG_INFO("Event system deinitialized successfully");
    return SUCCESS;
}

int event_subscribe(int event_type, EventCallback_t callback, void *user_data) {
    int i;

    if (event_type <= 0 || event_type >= MAX_EVENT_TYPE) {
        LOG_ERROR("Invalid event type: %d", event_type);
        return INVALID_PARAM;
    }

    if (callback == NULL) {
        LOG_ERROR("Invalid callback function");
        return INVALID_PARAM;
    }

    pthread_mutex_lock(&g_event_mutex);

    // 检查是否已经订阅过该事件
    for (i = 0; i < g_subscriber_counts[event_type]; i++) {
        if (g_event_subscriptions[event_type][i].callback == callback) {
            LOG_WARN("Callback already subscribed to event: %d", event_type);
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
    g_subscriber_counts[event_type]++;

    LOG_INFO("Subscribed to event: %d, subscriber count: %d", 
             event_type, g_subscriber_counts[event_type]);

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

    pthread_mutex_lock(&g_event_mutex);

    // 将事件加入队列
    ret = event_queue_push(&event);
    if (ret == SUCCESS) {
        // 通知事件处理线程有新事件
        pthread_cond_signal(&g_event_cond);
    }

    pthread_mutex_unlock(&g_event_mutex);

    LOG_DEBUG("Event notified: %d, data: %p, queue size: %d", 
             event_type, data, g_queue_size);

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
