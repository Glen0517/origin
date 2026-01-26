/**
 * @file network_process.c
 * @brief 网络服务进程实现
 * @details 负责网络服务（DLNA、AirPlay）的处理功能
 * @author AML Audio Team
 * @date 2026-01-22
 */

#include "wifi_media.h"
#include "logger.h"
#include "event.h"
#include "process.h"
#include "common_def.h"
#include <pthread.h>
#include <fcntl.h>

#ifdef _WIN32
// Windows 特定头文件
#include <windows.h>
#include <io.h>  // 用于 _get_osfhandle 函数
#define WNOHANG 1
#else
// Unix 特定头文件
#include <sys/wait.h>
#endif

/******************************************************************************************
 * 网络服务进程内部数据结构
 ******************************************************************************************/

/**
 * @brief 网络服务进程消息类型
 */
typedef enum {
    NETWORK_MSG_INIT = 0,           // 初始化网络服务
    NETWORK_MSG_DEINIT,              // 反初始化网络服务
    NETWORK_MSG_START_DLNA,          // 启动DLNA服务
    NETWORK_MSG_STOP_DLNA,           // 停止DLNA服务
    NETWORK_MSG_START_AIRPLAY,       // 启动AirPlay服务
    NETWORK_MSG_STOP_AIRPLAY,        // 停止AirPlay服务
    NETWORK_MSG_SET_DEVICE_NAME,     // 设置设备名称
    NETWORK_MSG_GET_STATUS,          // 获取服务状态
    NETWORK_MSG_MAX
} NetworkMsgType_t;

/**
 * @brief 网络服务进程消息
 */
typedef struct {
    NetworkMsgType_t type;           // 消息类型
    union {
        struct {
            WifiMediaConfig_t config;  // 网络服务配置
        } init;
        struct {
            char device_name[64];    // 设备名称
        } set_device_name;
    } data;
} NetworkMsg_t;

/******************************************************************************************
 * 网络服务进程全局变量
 ******************************************************************************************/

static bool g_network_process_running = false;
static pid_t g_network_process_pid = -1;
static int g_network_process_pipe[2] = {-1, -1};

// 线程相关变量
static pthread_t g_main_thread;
static pthread_t g_network_thread;
static pthread_t g_service_thread;
static pthread_t g_audio_thread;
static pthread_t g_quality_thread;
static pthread_t g_event_thread;
static pthread_t g_wifi_event_thread;

// 线程同步变量
static pthread_mutex_t g_msg_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_msg_queue_cond = PTHREAD_COND_INITIALIZER;

// 消息队列
static NetworkMsg_t *g_msg_queue = NULL;
static int g_msg_queue_capacity = 100;
static int g_msg_queue_head = 0;
static int g_msg_queue_tail = 0;

// 服务状态
static bool g_dlna_running = false;
static bool g_airplay_running = false;
static bool g_spotify_running = false;
static bool g_google_cast_running = false;

/******************************************************************************************
 * 线程间通信函数
 ******************************************************************************************/

/**
 * @brief 初始化消息队列
 */
static int init_msg_queue(void) {
    g_msg_queue = (NetworkMsg_t *)malloc(sizeof(NetworkMsg_t) * g_msg_queue_capacity);
    if (!g_msg_queue) {
        LOG_ERROR("Failed to allocate memory for message queue");
        return FAILURE;
    }
    g_msg_queue_head = 0;
    g_msg_queue_tail = 0;
    pthread_mutex_init(&g_msg_queue_mutex, NULL);
    pthread_cond_init(&g_msg_queue_cond, NULL);
    return SUCCESS;
}

/**
 * @brief 销毁消息队列
 */
static void destroy_msg_queue(void) {
    if (g_msg_queue) {
        free(g_msg_queue);
        g_msg_queue = NULL;
    }
    pthread_mutex_destroy(&g_msg_queue_mutex);
    pthread_cond_destroy(&g_msg_queue_cond);
}

/**
 * @brief 向消息队列添加消息
 */
static int add_msg_to_queue(NetworkMsg_t *msg) {
    if (!msg) {
        return FAILURE;
    }
    
    pthread_mutex_lock(&g_msg_queue_mutex);
    
    // 检查队列是否已满
    int next_tail = (g_msg_queue_tail + 1) % g_msg_queue_capacity;
    if (next_tail == g_msg_queue_head) {
        pthread_mutex_unlock(&g_msg_queue_mutex);
        LOG_ERROR("Message queue full");
        return FAILURE;
    }
    
    // 添加消息
    g_msg_queue[g_msg_queue_tail] = *msg;
    g_msg_queue_tail = next_tail;
    
    // 通知等待的线程
    pthread_cond_signal(&g_msg_queue_cond);
    
    pthread_mutex_unlock(&g_msg_queue_mutex);
    return SUCCESS;
}

/**
 * @brief 从消息队列获取消息（阻塞）
 */
static int get_msg_from_queue(NetworkMsg_t *msg) {
    if (!msg) {
        return FAILURE;
    }
    
    pthread_mutex_lock(&g_msg_queue_mutex);
    
    // 等待消息
    while (g_msg_queue_head == g_msg_queue_tail && g_network_process_running) {
        pthread_cond_wait(&g_msg_queue_cond, &g_msg_queue_mutex);
    }
    
    if (!g_network_process_running) {
        pthread_mutex_unlock(&g_msg_queue_mutex);
        return FAILURE;
    }
    
    // 获取消息
    *msg = g_msg_queue[g_msg_queue_head];
    g_msg_queue_head = (g_msg_queue_head + 1) % g_msg_queue_capacity;
    
    pthread_mutex_unlock(&g_msg_queue_mutex);
    return SUCCESS;
}

/******************************************************************************************
 * 线程函数实现
 ******************************************************************************************/

/**
 * @brief 主控制线程：处理进程间通信和消息分发
 */
static void *main_control_thread(void *arg) {
    LOG_INFO("Main control thread started");
    
    while (g_network_process_running) {
        // 读取管道消息
        NetworkMsg_t msg;
        int ret = read(g_network_process_pipe[0], &msg, sizeof(NetworkMsg_t));
        if (ret > 0) {
            // 将消息添加到队列
            add_msg_to_queue(&msg);
        } else if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR("Failed to read from pipe: %d", errno);
            break;
        }
        
        // 短暂休眠，避免CPU占用过高
        usleep(10000); // 10ms
    }
    
    LOG_INFO("Main control thread exited");
    return NULL;
}

/**
 * @brief 网络连接管理线程：处理WiFi连接和重连
 */
static void *network_connection_thread(void *arg) {
    LOG_INFO("Network connection thread started");
    
    int reconnect_attempts = 0;
    const int max_reconnect_attempts = 5;
    int reconnect_delay = 1000; // 初始重连延迟1秒
    
    while (g_network_process_running) {
        // 处理网络连接事件
        LOG_DEBUG("Processing network connection events...");
        
        // 检查WiFi连接状态
        bool connected = wifi_media_get_connect_state();
        
        if (!connected) {
            // WiFi未连接，尝试重连
            reconnect_attempts++;
            LOG_INFO("WiFi not connected, attempting to reconnect... (Attempt %d/%d)", 
                     reconnect_attempts, max_reconnect_attempts);
            
            // 发送重连请求
            // 注意：这里需要调用实际的WiFi重连函数，当前代码中没有实现
            // aml_wifi_reconnect();
            
            // 等待重连结果
            usleep(500000); // 500ms
            
            // 检查是否重连成功
            connected = wifi_media_get_connect_state();
            if (connected) {
                LOG_INFO("WiFi reconnected successfully");
                reconnect_attempts = 0;
                reconnect_delay = 1000; // 重置重连延迟
            } else {
                // 重连失败，增加重连延迟（指数退避）
                if (reconnect_attempts >= max_reconnect_attempts) {
                    LOG_WARN("Max reconnect attempts reached, reducing reconnect frequency");
                    reconnect_delay = 10000; // 10秒
                } else {
                    reconnect_delay = reconnect_delay * 2;
                    if (reconnect_delay > 5000) {
                        reconnect_delay = 5000; // 最大重连延迟5秒
                    }
                }
            }
        } else {
            // WiFi已连接，重置重连计数和延迟
            reconnect_attempts = 0;
            reconnect_delay = 1000;
        }
        
        // 短暂休眠
        usleep(reconnect_delay * 1000); // 转换为微秒
    }
    
    LOG_INFO("Network connection thread exited");
    return NULL;
}

/**
 * @brief 服务管理线程：处理消息队列中的服务请求
 */
static void *service_manager_thread(void *arg) {
    LOG_INFO("Service manager thread started");
    
    while (g_network_process_running) {
        NetworkMsg_t msg;
        if (get_msg_from_queue(&msg) == SUCCESS) {
            // 处理消息
            switch (msg.type) {
                case NETWORK_MSG_INIT:
                    LOG_INFO("Initializing WiFi media services...");
                    wifi_media_deinit();
                    wifi_media_init(&msg.data.init.config);
                    break;
                case NETWORK_MSG_DEINIT:
                    LOG_INFO("Deinitializing WiFi media services...");
                    wifi_media_deinit();
                    break;
                case NETWORK_MSG_START_DLNA:
                    LOG_INFO("Starting DLNA service...");
                    g_dlna_running = true;
                    // wifi_media_start_dlna();
                    break;
                case NETWORK_MSG_STOP_DLNA:
                    LOG_INFO("Stopping DLNA service...");
                    g_dlna_running = false;
                    // wifi_media_stop_dlna();
                    break;
                case NETWORK_MSG_START_AIRPLAY:
                    LOG_INFO("Starting AirPlay service...");
                    g_airplay_running = true;
                    // wifi_media_start_airplay();
                    break;
                case NETWORK_MSG_STOP_AIRPLAY:
                    LOG_INFO("Stopping AirPlay service...");
                    g_airplay_running = false;
                    // wifi_media_stop_airplay();
                    break;
                case NETWORK_MSG_SET_DEVICE_NAME:
                    LOG_INFO("Setting device name: %s", msg.data.set_device_name.device_name);
                    // wifi_media_set_device_name(msg.data.set_device_name.device_name);
                    break;
                case NETWORK_MSG_GET_STATUS:
                    LOG_INFO("Getting service status...");
                    // 发送服务状态
                    break;
                default:
                    LOG_ERROR("Invalid message type: %d", msg.type);
                    break;
            }
        }
        
        // 短暂休眠
        usleep(50000); // 50ms
    }
    
    LOG_INFO("Service manager thread exited");
    return NULL;
}

/**
 * @brief 音频处理线程：处理音频流数据
 */
static void *audio_process_thread(void *arg) {
    LOG_INFO("Audio processing thread started");
    
    // 设置高优先级，确保音频处理实时性
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    struct sched_param param;
    param.sched_priority = 90;
    pthread_attr_setschedparam(&attr, &param);
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    pthread_attr_destroy(&attr);
    
    while (g_network_process_running) {
        // 处理音频流数据
        // 1. 接收音频数据
        // 2. 处理音频格式转换
        // 3. 发送到音频核心
        
        LOG_DEBUG("Processing WiFi audio data...");
        
        // 短暂休眠，模拟音频处理周期
        usleep(10000); // 10ms
    }
    
    LOG_INFO("Audio processing thread exited");
    return NULL;
}

/**
 * @brief 网络质量检测线程：定期检测网络质量
 */
static void *network_quality_thread(void *arg) {
    LOG_INFO("Network quality thread started");
    
    while (g_network_process_running) {
        // 检测网络质量
        // 1. 测量网络延迟
        // 2. 计算丢包率
        // 3. 调整缓冲大小
        
        LOG_DEBUG("Checking network quality...");
        
        // 模拟网络质量检测
        static int quality = 5;
        quality = (quality + rand() % 3 - 1) % 10;
        quality = quality < 1 ? 1 : (quality > 10 ? 10 : quality);
        LOG_INFO("Network quality: %d/10", quality);
        
        // 调整缓冲大小
        // wifi_media_set_buffer_size(quality * 20);
        
        // 每5秒检测一次
        usleep(5000000); // 5000ms
    }
    
    LOG_INFO("Network quality thread exited");
    return NULL;
}

/**
 * @brief 事件轮询线程：轮询各服务状态
 */
static void *event_poll_thread(void *arg) {
    LOG_INFO("Event poll thread started");
    
    while (g_network_process_running) {
        // 轮询各服务状态
        // 1. 检查DLNA状态
        // 2. 检查AirPlay状态
        // 3. 检查Spotify状态
        // 4. 检查Google Cast状态
        
        LOG_DEBUG("Polling service events...");
        
        // 模拟事件轮询
        if (rand() % 200 == 0) {
            LOG_INFO("Service event detected");
        }
        
        // 短暂休眠
        usleep(200000); // 200ms
    }
    
    LOG_INFO("Event poll thread exited");
    return NULL;
}

/**
 * @brief WiFi媒体事件轮询线程：处理WiFi媒体事件
 * @details 负责定期调用wifi_media_event_poll()函数，处理网络质量检测、自动重连等WiFi媒体事件
 */
static void *wifi_event_poll_thread(void *arg) {
    LOG_INFO("WiFi media event poll thread started");
    
    while (g_network_process_running) {
        // 调用WiFi媒体事件轮询函数
        wifi_media_event_poll();
        
        // 短暂休眠
        usleep(50000); // 50ms，与main.c中的调用间隔保持一致
    }
    
    LOG_INFO("WiFi media event poll thread exited");
    return NULL;
}

/******************************************************************************************
 * 网络服务进程主函数
 ******************************************************************************************/

/**
 * @brief 网络服务进程主函数
 * @param arg 进程参数
 * @return 进程返回值
 */
static void *network_process_main(void *arg) {
    LOG_INFO("Network process started, pid: %d", getpid());
    
    // 初始化标志
    g_network_process_running = true;
    
    // 初始化消息队列
    if (init_msg_queue() != SUCCESS) {
        LOG_ERROR("Failed to initialize message queue");
        g_network_process_running = false;
        return NULL;
    }
    
    // 设置管道为非阻塞
    #ifdef _WIN32
    // Windows 平台：使用Windows API设置非阻塞
    DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
    SetNamedPipeHandleState((HANDLE)_get_osfhandle(g_network_process_pipe[0]), &mode, NULL, NULL);
    #else
    // Unix 平台：使用fcntl设置非阻塞
    int flags = fcntl(g_network_process_pipe[0], F_GETFL, 0);
    fcntl(g_network_process_pipe[0], F_SETFL, flags | O_NONBLOCK);
    #endif
    
    // 默认配置
    WifiMediaConfig_t default_config = {
        .wifi_name = "Aml_Soundbar",
        .dlna_en = true,
        .airplay_en = true,
        .spotify_en = true,
        .google_cast_en = true,
        .initial_volume = 80
    };
    
    // 初始化WiFi媒体模块
    if (wifi_media_init(&default_config) != 0) {
        LOG_ERROR("Wifi media init failed");
        destroy_msg_queue();
        g_network_process_running = false;
        return NULL;
    }
    
    // 创建线程属性
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    
    // 设置线程栈大小
    pthread_attr_setstacksize(&thread_attr, 128 * 1024); // 128KB
    
    // 创建线程
    pthread_create(&g_main_thread, &thread_attr, main_control_thread, NULL);
    pthread_create(&g_network_thread, &thread_attr, network_connection_thread, NULL);
    pthread_create(&g_service_thread, &thread_attr, service_manager_thread, NULL);
    pthread_create(&g_audio_thread, NULL, audio_process_thread, NULL); // 音频线程使用单独的属性设置
    pthread_create(&g_quality_thread, &thread_attr, network_quality_thread, NULL);
    pthread_create(&g_event_thread, &thread_attr, event_poll_thread, NULL);
    pthread_create(&g_wifi_event_thread, &thread_attr, wifi_event_poll_thread, NULL);
    
    // 销毁线程属性
    pthread_attr_destroy(&thread_attr);
    
    // 等待所有线程结束
    pthread_join(g_main_thread, NULL);
    pthread_join(g_network_thread, NULL);
    pthread_join(g_service_thread, NULL);
    pthread_join(g_audio_thread, NULL);
    pthread_join(g_quality_thread, NULL);
    pthread_join(g_event_thread, NULL);
    pthread_join(g_wifi_event_thread, NULL);
    
    // 清理资源
    destroy_msg_queue();
    wifi_media_deinit();
    
    g_network_process_running = false;
    LOG_INFO("Network process exited");
    return NULL;
}

/**
 * @brief 创建网络服务进程
 * @return 进程ID，失败返回-1
 */
static pid_t create_network_process(void) {
    // 创建管道
    if (pipe(g_network_process_pipe) == -1) {
        LOG_ERROR("Failed to create pipe: %d", errno);
        return -1;
    }
    
    // 创建进程
    pid_t pid = fork();
    if (pid == -1) {
        LOG_ERROR("Failed to fork network process: %d", errno);
        close(g_network_process_pipe[0]);
        close(g_network_process_pipe[1]);
        g_network_process_pipe[0] = -1;
        g_network_process_pipe[1] = -1;
        return -1;
    } else if (pid == 0) {
        // 子进程
        close(g_network_process_pipe[1]); // 关闭写端
        g_network_process_running = true;
        network_process_main(NULL);
        close(g_network_process_pipe[0]);
        exit(0);
    } else {
        // 父进程
        close(g_network_process_pipe[0]); // 关闭读端
        g_network_process_pid = pid;
        LOG_INFO("Created network process: %d", pid);
    }
    
    return pid;
}

/**
 * @brief 终止网络服务进程
 * @return SUCCESS/FAILURE
 */
static int terminate_network_process(void) {
    if (g_network_process_pid == -1) {
        return SUCCESS;
    }
    
    // 发送终止消息
    NetworkMsg_t msg;
    msg.type = NETWORK_MSG_DEINIT;
    write(g_network_process_pipe[1], &msg, sizeof(NetworkMsg_t));
    
    // 终止进程
    if (kill(g_network_process_pid, SIGTERM) == -1) {
        LOG_ERROR("Failed to terminate network process: %d", errno);
        return FAILURE;
    }
    
    // 等待进程退出
    waitpid(g_network_process_pid, NULL, 0);
    
    // 关闭管道
    close(g_network_process_pipe[1]);
    g_network_process_pipe[0] = -1;
    g_network_process_pipe[1] = -1;
    g_network_process_pid = -1;
    
    LOG_INFO("Terminated network process: %d", g_network_process_pid);
    return SUCCESS;
}

/******************************************************************************************
 * 网络服务进程对外接口
 ******************************************************************************************/

/**
 * @brief 初始化网络服务进程
 * @param config 网络服务配置
 * @return SUCCESS/FAILURE
 */
int network_process_init(WifiMediaConfig_t *config) {
    // 创建网络服务进程
    pid_t pid = create_network_process();
    if (pid == -1) {
        LOG_ERROR("Failed to create network process");
        return FAILURE;
    }
    
    // 发送初始化消息
    if (config) {
        NetworkMsg_t msg;
        msg.type = NETWORK_MSG_INIT;
        msg.data.init.config = *config;
        write(g_network_process_pipe[1], &msg, sizeof(NetworkMsg_t));
    }
    
    LOG_INFO("Network process initialized");
    return SUCCESS;
}

/**
 * @brief 反初始化网络服务进程
 * @return SUCCESS/FAILURE
 */
int network_process_deinit(void) {
    int ret = terminate_network_process();
    if (ret == SUCCESS) {
        LOG_INFO("Network process deinitialized");
    }
    return ret;
}

/**
 * @brief 启动DLNA服务
 * @return SUCCESS/FAILURE
 */
int network_process_start_dlna(void) {
    if (g_network_process_pid == -1) {
        LOG_ERROR("Network process not initialized");
        return FAILURE;
    }
    
    NetworkMsg_t msg;
    msg.type = NETWORK_MSG_START_DLNA;
    
    if (write(g_network_process_pipe[1], &msg, sizeof(NetworkMsg_t)) != sizeof(NetworkMsg_t)) {
        LOG_ERROR("Failed to send start DLNA message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 停止DLNA服务
 * @return SUCCESS/FAILURE
 */
int network_process_stop_dlna(void) {
    if (g_network_process_pid == -1) {
        LOG_ERROR("Network process not initialized");
        return FAILURE;
    }
    
    NetworkMsg_t msg;
    msg.type = NETWORK_MSG_STOP_DLNA;
    
    if (write(g_network_process_pipe[1], &msg, sizeof(NetworkMsg_t)) != sizeof(NetworkMsg_t)) {
        LOG_ERROR("Failed to send stop DLNA message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 启动AirPlay服务
 * @return SUCCESS/FAILURE
 */
int network_process_start_airplay(void) {
    if (g_network_process_pid == -1) {
        LOG_ERROR("Network process not initialized");
        return FAILURE;
    }
    
    NetworkMsg_t msg;
    msg.type = NETWORK_MSG_START_AIRPLAY;
    
    if (write(g_network_process_pipe[1], &msg, sizeof(NetworkMsg_t)) != sizeof(NetworkMsg_t)) {
        LOG_ERROR("Failed to send start AirPlay message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 停止AirPlay服务
 * @return SUCCESS/FAILURE
 */
int network_process_stop_airplay(void) {
    if (g_network_process_pid == -1) {
        LOG_ERROR("Network process not initialized");
        return FAILURE;
    }
    
    NetworkMsg_t msg;
    msg.type = NETWORK_MSG_STOP_AIRPLAY;
    
    if (write(g_network_process_pipe[1], &msg, sizeof(NetworkMsg_t)) != sizeof(NetworkMsg_t)) {
        LOG_ERROR("Failed to send stop AirPlay message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 设置设备名称
 * @param device_name 设备名称
 * @return SUCCESS/FAILURE
 */
int network_process_set_device_name(const char *device_name) {
    if (g_network_process_pid == -1) {
        LOG_ERROR("Network process not initialized");
        return FAILURE;
    }
    
    if (!device_name) {
        LOG_ERROR("Invalid device name");
        return FAILURE;
    }
    
    NetworkMsg_t msg;
    msg.type = NETWORK_MSG_SET_DEVICE_NAME;
    strncpy(msg.data.set_device_name.device_name, device_name, sizeof(msg.data.set_device_name.device_name) - 1);
    
    if (write(g_network_process_pipe[1], &msg, sizeof(NetworkMsg_t)) != sizeof(NetworkMsg_t)) {
        LOG_ERROR("Failed to send set device name message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 获取网络服务进程ID
 * @return 进程ID，失败返回-1
 */
pid_t network_process_get_pid(void) {
    return g_network_process_pid;
}

/**
 * @brief 检查网络服务进程是否运行
 * @return true表示运行，false表示未运行
 */
bool network_process_is_running(void) {
    if (g_network_process_pid == -1) {
        return false;
    }
    
    int status;
    pid_t result = waitpid(g_network_process_pid, &status, WNOHANG);
    return result == 0;
}
