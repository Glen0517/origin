/**
 * @file main_loop.c
 * @brief 主循环模块实现
 * @details 提供业务主循环和事件处理的功能
 * @author AML Audio Team
 * @date 2026-01-28
 */

#include "main_loop.h"

// 外部头文件
#include "power_manager.h"
#include "process_manager.h"
#include "memory_manager.h"

// 事件回调类型定义
typedef void (*EventCallback)(void);
typedef void (*KeyEventCallback)(KeyEvent_e event);

// 事件类型枚举
typedef enum {
    EVENT_TYPE_SYSTEM = 0,
    EVENT_TYPE_BLUETOOTH,
    EVENT_TYPE_AUDIO_SOURCE,
    EVENT_TYPE_PLAY_CTRL,
    EVENT_TYPE_WIFI_MEDIA,
    EVENT_TYPE_BT_MESH,
    EVENT_TYPE_HDMI_ARC,
    EVENT_TYPE_SPDIF,
    EVENT_TYPE_KEY,
    EVENT_TYPE_MAX
} EventType_e;

// 事件优先级枚举
typedef enum {
    EVENT_PRIORITY_LOW = 0,
    EVENT_PRIORITY_MEDIUM,
    EVENT_PRIORITY_HIGH,
    EVENT_PRIORITY_CRITICAL
} EventPriority_e;

// 事件结构体
typedef struct {
    EventType_e type;
    EventPriority_e priority;
    EventCallback callback;
    void *data;
} Event_t;

// 事件队列结构体
typedef struct {
    Event_t *events;
    uint32_t capacity;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} EventQueue_t;

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

// 外部函数声明
extern KeyEvent_e peripheral_get_key_event(void);
extern void peripheral_start_ir_learn(void);
extern void audio_source_switch_next(void);
extern PlayState_e play_ctrl_get_state(void);
extern void play_ctrl_set_state(PlayState_e state);
extern SoundMode_e play_ctrl_get_sound_mode(void);
extern void play_ctrl_next_song(void);
extern void play_ctrl_prev_song(void);
extern int volume_ctrl_master_up(void);
extern int volume_ctrl_master_down(void);
extern int volume_ctrl_bass_up(void);
extern int volume_ctrl_treble_up(void);
extern uint32_t resource_get_system_load(void);

// 全局变量声明
extern int g_sys_running;
// 外部依赖注入容器声明
extern void *g_di_container;

/**
 * @brief 模块事件处理器定义
 * @details 用于管理各个模块的轮询频率和事件处理
 */
typedef void (*EventHandler)(void);

/**
 * @brief 模块轮询配置结构体
 */
typedef struct {
    EventHandler handler;     // 事件处理函数
    uint32_t interval_ms;     // 轮询间隔（毫秒）
    uint32_t last_run_ms;     // 上次运行时间（毫秒）
    EventPriority_e priority; // 事件优先级
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
 * @brief 信号处理函数
 * @details 处理系统信号，实现优雅退出
 * @param sig 接收到的信号类型
 * @return 无
 */
void sig_handler(int sig) {
    // 处理中断信号和终止信号
    if (sig == SIGINT || sig == SIGTERM) {
        LOG_INFO("System receive exit signal [%d], start deinit...", sig);
        // 设置系统运行状态为退出
        g_sys_running = 0;
    }
}

/**
 * @brief 初始化事件队列
 * @param capacity 事件队列容量
 * @return 事件队列指针，失败返回NULL
 */
static EventQueue_t *event_queue_init(uint32_t capacity) {
    EventQueue_t *queue = (EventQueue_t *)malloc(sizeof(EventQueue_t));
    if (!queue) {
        LOG_ERROR("Failed to allocate event queue");
        return NULL;
    }
    
    queue->events = (Event_t *)malloc(sizeof(Event_t) * capacity);
    if (!queue->events) {
        LOG_ERROR("Failed to allocate event queue buffer");
        free(queue);
        return NULL;
    }
    
    queue->capacity = capacity;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    
    return queue;
}

/**
 * @brief 销毁事件队列
 * @param queue 事件队列指针
 */
static void event_queue_destroy(EventQueue_t *queue) {
    if (queue) {
        free(queue->events);
        free(queue);
    }
}

/**
 * @brief 向事件队列添加事件
 * @param queue 事件队列指针
 * @param event 事件结构体
 * @return 成功返回true，失败返回false
 */
static bool event_queue_push(EventQueue_t *queue, Event_t event) {
    if (!queue || queue->count >= queue->capacity) {
        return false;
    }
    
    queue->events[queue->tail] = event;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
    
    return true;
}

/**
 * @brief 从事件队列获取事件
 * @param queue 事件队列指针
 * @param event 事件结构体指针
 * @return 成功返回true，失败返回false
 */
static bool event_queue_pop(EventQueue_t *queue, Event_t *event) {
    if (!queue || queue->count == 0) {
        return false;
    }
    
    *event = queue->events[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    
    return true;
}

/**
 * @brief 处理按键事件
 * @param event 按键事件
 */
static void handle_key_event(KeyEvent_e event) {
    LOG_INFO("Key event received: %d", event);
    
    // 根据按键事件执行相应操作
    switch (event) {
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
            LOG_INFO("Unknown key event: %d", event);
            break;
    }
}

/**
 * @brief 业务主循环
 * @details 使用高精度时间函数定期轮询各个模块，处理系统的核心业务逻辑
 * @return 无
 */
void main_business_loop(void) {
    LOG_INFO("Enter business loop...");
    
    // 启动资源监控
    resource_start_monitoring(1000);
    // 启动系统监控
    monitor_start(1000);
    
    // 初始化事件队列
    EventQueue_t *event_queue = event_queue_init(128);
    if (!event_queue) {
        LOG_ERROR("Failed to initialize event queue");
        return;
    }
    
    // 初始化模块轮询配置
    ModulePollConfig poll_configs[] = {
        {bluetooth_event_poll, 50, 0, EVENT_PRIORITY_MEDIUM},        // 蓝牙事件：每50毫秒轮询一次
        {audio_source_event_poll, 50, 0, EVENT_PRIORITY_MEDIUM},     // 音频源事件：每50毫秒轮询一次
        {play_ctrl_event_poll, 10, 0, EVENT_PRIORITY_HIGH},        // 播放控制事件：每10毫秒轮询一次（高实时性）
        {system_event_poll, 2000, 0, EVENT_PRIORITY_LOW},         // 系统事件：每2秒轮询一次
#ifdef CONFIG_ENABLE_BT_MESH
        {subwoofer_comm_event_poll, 100, 0, EVENT_PRIORITY_MEDIUM},  // 低音炮通信事件：每100毫秒轮询一次
#endif
#ifdef CONFIG_ENABLE_HDMI_ARC
        {hdmi_arc_event_poll, 100, 0, EVENT_PRIORITY_MEDIUM},        // HDMI ARC事件：每100毫秒轮询一次
#endif
#ifdef CONFIG_ENABLE_SPDIF
        {spdif_optical_event_poll, 100, 0, EVENT_PRIORITY_MEDIUM},   // SPDIF事件：每100毫秒轮询一次
#endif
#ifdef CONFIG_ENABLE_WIFI_MEDIA
        {wifi_media_event_poll, 100, 0, EVENT_PRIORITY_MEDIUM},      // WiFi媒体事件：每100毫秒轮询一次
#endif
        {NULL, 100, 0, EVENT_PRIORITY_LOW}                        // 结束标记
    };
    
    int epoll_fd = -1;
    int timer_fd = -1;
    
    // 动态轮询频率配置
    uint32_t base_poll_interval = 10;  // 基础轮询间隔（毫秒）
    uint32_t current_poll_interval = base_poll_interval;
    
    // 条件编译：根据平台选择不同的事件驱动方式
#if defined(__linux__) || defined(__linux) || defined(LINUX)
    // Linux平台：使用epoll和timerfd实现事件驱动
    // 创建epoll实例，使用更大的大小
    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        LOG_ERROR("Failed to create epoll: %d", errno);
        event_queue_destroy(event_queue);
        return;
    }
    
    // 创建定时器
    timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer_fd == -1) {
        LOG_ERROR("Failed to create timerfd: %d", errno);
        close(epoll_fd);
        event_queue_destroy(event_queue);
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
        event_queue_destroy(event_queue);
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
        event_queue_destroy(event_queue);
        return;
    }
    
    // 主循环，直到系统运行状态为0时退出
    struct epoll_event events[32]; // 增加事件数组大小
    while (g_sys_running) {
        // 使用epoll阻塞等待事件，设置合理的超时时间
        int ret = epoll_wait(epoll_fd, events, 32, current_poll_interval);
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
        
        // 轮询各个模块的事件
#endif
#ifdef CONFIG_ENABLE_GAME_SPEAKER
                if (CURRENT_PRODUCT_TYPE != PRODUCT_GAME_LOW_END) {
#else
                {
#endif
                    // 处理按键事件
                KeyEvent_e key_event = peripheral_get_key_event();
                if (key_event != KEY_EVENT_NONE) {
                    handle_key_event(key_event);
                }
                
                // 遍历模块轮询配置，自动管理轮询频率
                for (int i = 0; i < (int)(sizeof(poll_configs) / sizeof(poll_configs[0]) - 1); i++) {
                    if (poll_configs[i].handler != NULL &&
                        current_time - poll_configs[i].last_run_ms >= poll_configs[i].interval_ms) {
                        // 根据优先级处理事件
                        if (poll_configs[i].priority == EVENT_PRIORITY_CRITICAL) {
                            //  critical事件立即处理
                            poll_configs[i].handler();
                        } else {
                            // 其他事件放入队列
                            Event_t event = {
                                .type = EVENT_TYPE_SYSTEM,
                                .priority = poll_configs[i].priority,
                                .callback = poll_configs[i].handler,
                                .data = NULL
                            };
                            event_queue_push(event_queue, event);
                        }
                        poll_configs[i].last_run_ms = current_time;
                    }
                }
                
                // 处理事件队列中的事件
                Event_t event;
                while (event_queue_pop(event_queue, &event)) {
                    if (event.callback) {
                        event.callback();
                    }
                }
                
                // 可选模块的事件轮询已集成到统一的轮询配置中
                // 监控内存使用情况
                if (g_di_container) {
                    MemoryManager_t *mem_manager = (MemoryManager_t *)g_di_container;
                    memory_manager_monitor_usage(mem_manager, 80);
                }
                
                // 监控系统进程状态
                process_manager_monitor();
                
#ifdef CONFIG_ENABLE_GAME_SPEAKER
                // 低端游戏音响：仅轮询必要模块
                if (CURRENT_PRODUCT_TYPE == PRODUCT_GAME_LOW_END) {
                    system_event_poll();            // 系统事件：系统状态、资源使用等
                }
#endif
                
            }
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
    
    // 销毁事件队列
    event_queue_destroy(event_queue);
}

/**
 * @brief 触发事件
 * @details 向事件队列添加事件
 * @param type 事件类型
 * @param priority 事件优先级
 * @param callback 事件回调函数
 * @param data 事件数据
 * @return 成功返回true，失败返回false
 */
bool trigger_event(EventType_e type, EventPriority_e priority, EventCallback callback, void *data) {
    // 这里可以实现全局事件触发机制
    // 目前简单实现，实际应用中可以使用全局事件队列
    if (callback) {
        callback();
        return true;
    }
    return false;
}
