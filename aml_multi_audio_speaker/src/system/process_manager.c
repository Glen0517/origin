/**
 * @file process_manager.c
 * @brief 进程管理模块实现
 * @details 统一管理所有进程的创建、销毁和通信
 * @author AML Audio Team
 * @date 2026-01-22
 */

#include "./process_manager.h"
#include "logger.h"
#include "../lib/flac/common_def.h"

#ifdef _WIN32
// Windows 特定头文件
#include <windows.h>
#include <process.h>
#else
// Unix 特定头文件
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#endif

#include <time.h>
#include <string.h>
#include <stdlib.h>

// 包含各个模块的头文件
#include "../lib/flac/storage.h"
#include "../lib/flac/wifi_media.h"
#include "../lib/flac/audio_core.h"
#include "../lib/flac/system.h"
#include "../remote_control/remote_control.h"
#include "../lib/flac/event.h"
#include "../lib/flac/peripheral.h"
#include "../lib/flac/bt.h"  // 蓝牙

/******************************************************************************************
 * 各个进程的main_func实现
 ******************************************************************************************/

/**
 * @brief 存储进程主函数
 * @param arg 进程参数
 */
static void storage_process_main(void *arg) {
    LOG_INFO("Storage process started");
    
    // 初始化存储系统
    LOG_INFO("Initializing storage system...");
    // 这里可以添加实际的存储系统初始化代码
    
    // 主循环
    while (1) {
        // 检查存储设备状态
        LOG_DEBUG("Checking storage devices...");
        // 这里可以添加实际的存储设备检查代码
        
        // 短暂休眠
        usleep(1000000); // 1秒
    }
}

/**
 * @brief 网络进程主函数
 * @param arg 进程参数
 */
static void network_process_main(void *arg) {
    LOG_INFO("Network process started");
    
    // 初始化网络系统
    LOG_INFO("Initializing network system...");
    // 这里可以添加实际的网络系统初始化代码
    
    // 主循环
    while (1) {
        // 检查网络连接状态
        LOG_DEBUG("Checking network connections...");
        // 这里可以添加实际的网络连接检查代码
        
        // 短暂休眠
        usleep(1000000); // 1秒
    }
}

/**
 * @brief 音频进程主函数
 * @param arg 进程参数
 */
static void audio_process_main(void *arg) {
    LOG_INFO("Audio process started");
    
    // 初始化音频系统
    LOG_INFO("Initializing audio system...");
    // 这里可以添加实际的音频系统初始化代码
    
    // 主循环
    while (1) {
        // 处理音频数据
        LOG_DEBUG("Processing audio data...");
        // 这里可以添加实际的音频数据处理代码
        
        // 短暂休眠
        usleep(500000); // 0.5秒
    }
}

/**
 * @brief 系统进程主函数
 * @param arg 进程参数
 */
static void system_process_main(void *arg) {
    LOG_INFO("System process started");
    
    // 初始化系统
    LOG_INFO("Initializing system...");
    // 这里可以添加实际的系统初始化代码
    
    // 主循环
    while (1) {
        // 检查系统状态
        LOG_DEBUG("Checking system status...");
        // 这里可以添加实际的系统状态检查代码
        
        // 短暂休眠
        usleep(2000000); // 2秒
    }
}

/**
 * @brief 远程控制进程主函数
 * @param arg 进程参数
 */
static void remote_control_process_main_impl(void *arg) {
    LOG_INFO("Remote control process started");
    
    // 初始化远程控制系统
    LOG_INFO("Initializing remote control system...");
    // 这里可以添加实际的远程控制系统初始化代码
    
    // 主循环
    while (1) {
        // 处理遥控器事件
        LOG_DEBUG("Processing remote control events...");
        // 这里可以添加实际的遥控器事件处理代码
        
        // 短暂休眠
        usleep(100000); // 0.1秒
    }
}

/******************************************************************************************
 * 事件类型定义
 ******************************************************************************************/

// 按键事件定义
#define EVENT_KEY_PRESSED 901 // 按键按下事件

/******************************************************************************************
 * 蓝牙进程线程间通信相关定义
 ******************************************************************************************/

/**
 * @brief 蓝牙消息类型
 */
typedef enum {
    BT_MSG_TYPE_AUDIO_START,      // 音频开始
    BT_MSG_TYPE_AUDIO_STOP,       // 音频停止
    BT_MSG_TYPE_CONNECTION_CHANGED, // 连接状态改变
    BT_MSG_TYPE_AUTO_RECONNECT,   // 自动重连
    BT_MSG_TYPE_MESH_EVENT,       // MESH事件
    BT_MSG_TYPE_PAIR_REQUEST,     // 配对请求
    BT_MSG_TYPE_PAIR_COMPLETE,    // 配对完成
    BT_MSG_TYPE_PAIR_CANCEL,      // 取消配对
    BT_MSG_TYPE_MAX
} BluetoothMsgType_t;

/**
 * @brief 蓝牙消息结构体
 */
typedef struct {
    BluetoothMsgType_t msg_type;   // 消息类型
    void *data;                    // 消息数据
    size_t data_size;              // 数据大小
} BluetoothMsg_t;

/**
 * @brief 蓝牙消息队列
 */
typedef struct {
    BluetoothMsg_t *messages;      // 消息数组
    int capacity;                  // 队列容量
    int head;                      // 队列头部
    int tail;                      // 队列尾部
    pthread_mutex_t mutex;         // 互斥锁
    pthread_cond_t cond;           // 条件变量
    bool running;                  // 运行标志
} BluetoothMsgQueue_t;

/**
 * @brief 蓝牙线程共享数据
 */
typedef struct {
    bool audio_playing;            // 音频播放状态
    bool connected;                // 连接状态
    char device_name[32];          // 设备名称
    char device_addr[18];          // 设备地址
    int volume;                    // 音量
    int reconnect_attempts;        // 重连尝试次数
    bool mesh_enabled;             // MESH使能状态
    BluetoothMsgQueue_t msg_queue; // 消息队列
    pthread_mutex_t data_mutex;    // 数据互斥锁
} BluetoothSharedData_t;

/**
 * @brief 蓝牙线程共享数据全局变量
 */
static BluetoothSharedData_t g_bluetooth_shared_data = {
    .audio_playing = false,
    .connected = false,
    .device_name = "",
    .device_addr = "",
    .volume = 50,
    .reconnect_attempts = 0,
    .mesh_enabled = false
};

/**
 * @brief 初始化蓝牙消息队列
 * @param queue 消息队列指针
 * @param capacity 队列容量
 * @return SUCCESS/FAILURE
 */
static int bluetooth_msg_queue_init(BluetoothMsgQueue_t *queue, int capacity) {
    if (!queue || capacity <= 0) {
        return FAILURE;
    }
    
    queue->messages = (BluetoothMsg_t *)malloc(sizeof(BluetoothMsg_t) * capacity);
    if (!queue->messages) {
        return FAILURE;
    }
    
    queue->capacity = capacity;
    queue->head = 0;
    queue->tail = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
    queue->running = true;
    
    return SUCCESS;
}

/**
 * @brief 销毁蓝牙消息队列
 * @param queue 消息队列指针
 */
static void bluetooth_msg_queue_destroy(BluetoothMsgQueue_t *queue) {
    if (!queue) {
        return;
    }
    
    queue->running = false;
    pthread_cond_broadcast(&queue->cond);
    
    if (queue->messages) {
        free(queue->messages);
        queue->messages = NULL;
    }
    
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
}

/**
 * @brief 向蓝牙消息队列发送消息
 * @param msg 消息结构体
 * @return SUCCESS/FAILURE
 */
static int bluetooth_send_message(BluetoothMsgType_t msg_type, void *data, size_t data_size) {
    BluetoothMsgQueue_t *queue = &g_bluetooth_shared_data.msg_queue;
    
    pthread_mutex_lock(&queue->mutex);
    
    // 检查队列是否已满
    int next_tail = (queue->tail + 1) % queue->capacity;
    if (next_tail == queue->head) {
        pthread_mutex_unlock(&queue->mutex);
        LOG_WARN("Bluetooth message queue full");
        return FAILURE;
    }
    
    // 添加消息到队列
    queue->messages[queue->tail].msg_type = msg_type;
    queue->messages[queue->tail].data = data;
    queue->messages[queue->tail].data_size = data_size;
    queue->tail = next_tail;
    
    // 通知等待的线程
    pthread_cond_signal(&queue->cond);
    
    pthread_mutex_unlock(&queue->mutex);
    
    LOG_DEBUG("Bluetooth message sent: type=%d", msg_type);
    return SUCCESS;
}

/**
 * @brief 从蓝牙消息队列接收消息
 * @param msg 消息结构体指针
 * @return SUCCESS/FAILURE
 */
static int bluetooth_receive_message(BluetoothMsg_t *msg) {
    BluetoothMsgQueue_t *queue = &g_bluetooth_shared_data.msg_queue;
    
    pthread_mutex_lock(&queue->mutex);
    
    // 等待消息
    while (queue->running && queue->head == queue->tail) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    
    if (!queue->running) {
        pthread_mutex_unlock(&queue->mutex);
        return FAILURE;
    }
    
    // 从队列取出消息
    *msg = queue->messages[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    
    pthread_mutex_unlock(&queue->mutex);
    
    LOG_DEBUG("Bluetooth message received: type=%d", msg->msg_type);
    return SUCCESS;
}

/**
 * @brief 蓝牙按键事件处理函数
 * @param event_type 事件类型
 * @param data 事件数据
 * @param user_data 用户数据
 */
static void bluetooth_handle_key_event(int event_type, void *data, void *user_data) {
    if (event_type == EVENT_KEY_PRESSED) {
        KeyEvent_e key_event = *(KeyEvent_e *)data;
        LOG_INFO("Bluetooth received key event: %d", key_event);
        
        // 处理按键事件
        switch (key_event) {
            case KEY_EVENT_PLAY_PAUSE:
                LOG_INFO("Bluetooth handling PLAY_PAUSE key event");
                // 切换音频播放状态
                pthread_mutex_lock(&g_bluetooth_shared_data.data_mutex);
                bool is_playing = g_bluetooth_shared_data.audio_playing;
                g_bluetooth_shared_data.audio_playing = !is_playing;
                pthread_mutex_unlock(&g_bluetooth_shared_data.data_mutex);
                
                // 发送音频状态消息
                if (!is_playing) {
                    bluetooth_send_message(BT_MSG_TYPE_AUDIO_START, NULL, 0);
                } else {
                    bluetooth_send_message(BT_MSG_TYPE_AUDIO_STOP, NULL, 0);
                }
                break;
                
            case KEY_EVENT_VOL_UP:
                LOG_INFO("Bluetooth handling VOL_UP key event");
                // 增加音量
                pthread_mutex_lock(&g_bluetooth_shared_data.data_mutex);
                if (g_bluetooth_shared_data.volume < 100) {
                    g_bluetooth_shared_data.volume += 5;
                    if (g_bluetooth_shared_data.volume > 100) {
                        g_bluetooth_shared_data.volume = 100;
                    }
                    LOG_INFO("Bluetooth volume increased to: %d", g_bluetooth_shared_data.volume);
                }
                pthread_mutex_unlock(&g_bluetooth_shared_data.data_mutex);
                break;
                
            case KEY_EVENT_VOL_DOWN:
                LOG_INFO("Bluetooth handling VOL_DOWN key event");
                // 减少音量
                pthread_mutex_lock(&g_bluetooth_shared_data.data_mutex);
                if (g_bluetooth_shared_data.volume > 0) {
                    g_bluetooth_shared_data.volume -= 5;
                    if (g_bluetooth_shared_data.volume < 0) {
                        g_bluetooth_shared_data.volume = 0;
                    }
                    LOG_INFO("Bluetooth volume decreased to: %d", g_bluetooth_shared_data.volume);
                }
                pthread_mutex_unlock(&g_bluetooth_shared_data.data_mutex);
                break;
                
            case KEY_EVENT_SOURCE_SWITCH:
                LOG_INFO("Bluetooth handling SOURCE_SWITCH key event");
                // 切换到蓝牙音源
                pthread_mutex_lock(&g_bluetooth_shared_data.data_mutex);
                LOG_INFO("Switching to Bluetooth source");
                pthread_mutex_unlock(&g_bluetooth_shared_data.data_mutex);
                break;
                
            case KEY_EVENT_NEXT:
                LOG_INFO("Bluetooth handling NEXT key event");
                // 下一首
                break;
                
            case KEY_EVENT_PREV:
                LOG_INFO("Bluetooth handling PREV key event");
                // 上一首
                break;
                
            case KEY_EVENT_BT_PAIR:
                LOG_INFO("Bluetooth handling BT_PAIR key event");
                // 发送配对请求消息到连接管理线程（中高优先级处理）
                LOG_INFO("Sending pair request message to connection thread");
                bluetooth_send_message(BT_MSG_TYPE_PAIR_REQUEST, NULL, 0);
                break;
                
            default:
                LOG_WARN("Bluetooth received unknown key event: %d", key_event);
                break;
        }
    }
}

/**
 * @brief 蓝牙音频处理线程函数
 * @param arg 线程参数
 */
static void *bluetooth_audio_thread(void *arg) {
    LOG_INFO("Bluetooth audio thread started");
    
    // 初始化音频处理
    LOG_INFO("Initializing Bluetooth audio processing...");
    // 这里可以添加实际的音频处理初始化代码
    
    // 主循环
    while (1) {
        // 检查消息队列
        BluetoothMsg_t msg;
        if (bluetooth_receive_message(&msg) == SUCCESS) {
            // 处理收到的消息
            switch (msg.msg_type) {
                case BT_MSG_TYPE_AUDIO_START:
                    LOG_INFO("Audio start message received");
                    // 处理音频开始
                    pthread_mutex_lock(&g_bluetooth_shared_data.data_mutex);
                    g_bluetooth_shared_data.audio_playing = true;
                    pthread_mutex_unlock(&g_bluetooth_shared_data.data_mutex);
                    
                    // 调用蓝牙音频流开始函数
                    if (bluetooth_start_audio_stream() == SUCCESS) {
                        LOG_INFO("Bluetooth audio stream started successfully");
                    } else {
                        LOG_ERROR("Failed to start Bluetooth audio stream");
                    }
                    break;
                case BT_MSG_TYPE_AUDIO_STOP:
                    LOG_INFO("Audio stop message received");
                    // 处理音频停止
                    pthread_mutex_lock(&g_bluetooth_shared_data.data_mutex);
                    g_bluetooth_shared_data.audio_playing = false;
                    pthread_mutex_unlock(&g_bluetooth_shared_data.data_mutex);
                    
                    // 调用蓝牙音频流停止函数
                    if (bluetooth_stop_audio_stream() == SUCCESS) {
                        LOG_INFO("Bluetooth audio stream stopped successfully");
                    } else {
                        LOG_ERROR("Failed to stop Bluetooth audio stream");
                    }
                    break;
                case BT_MSG_TYPE_CONNECTION_CHANGED:
                    LOG_INFO("Connection changed message received");
                    // 处理连接状态改变
                    pthread_mutex_lock(&g_bluetooth_shared_data.data_mutex);
                    bool connected = g_bluetooth_shared_data.connected;
                    pthread_mutex_unlock(&g_bluetooth_shared_data.data_mutex);
                    
                    if (connected) {
                        // 连接成功，设置音频参数
                        if (bluetooth_set_audio_params(44100, 2, 16) == SUCCESS) {
                            LOG_INFO("Bluetooth audio parameters set successfully");
                        } else {
                            LOG_ERROR("Failed to set Bluetooth audio parameters");
                        }
                    }
                    break;
                default:
                    break;
            }
        }
        
        // 处理A2DP音频流
        LOG_DEBUG("Processing Bluetooth A2DP audio stream...");
        
        // 检查连接状态
        pthread_mutex_lock(&g_bluetooth_shared_data.data_mutex);
        bool connected = g_bluetooth_shared_data.connected;
        bool playing = g_bluetooth_shared_data.audio_playing;
        int volume = g_bluetooth_shared_data.volume;
        pthread_mutex_unlock(&g_bluetooth_shared_data.data_mutex);
        
        if (connected && playing) {
            // 检查并更新音量
            int current_volume = bluetooth_get_audio_volume();
            if (current_volume != volume) {
                if (bluetooth_set_audio_volume(volume) == SUCCESS) {
                    LOG_INFO("Bluetooth volume updated to: %d", volume);
                }
            }
        }
        
        // 短暂休眠
        usleep(10000); // 0.01秒
    }
}

/**
 * @brief 蓝牙连接管理线程函数
 * @param arg 线程参数
 */
static void *bluetooth_connection_thread(void *arg) {
    LOG_INFO("Bluetooth connection thread started");
    
    // 初始化连接管理
    LOG_INFO("Initializing Bluetooth connection management...");
    // 这里可以添加实际的连接管理初始化代码
    
    // 主循环
    while (1) {
        // 检查消息队列
        BluetoothMsg_t msg;
        if (bluetooth_receive_message(&msg) == SUCCESS) {
            // 处理收到的消息
            switch (msg.msg_type) {
                case BT_MSG_TYPE_CONNECTION_CHANGED:
                    LOG_INFO("Connection changed message received");
                    // 处理连接状态改变
                    break;
                case BT_MSG_TYPE_PAIR_REQUEST:
                    LOG_INFO("Pair request message received");
                    // 处理配对请求
                    if (bluetooth_start_pair() == SUCCESS) {
                        LOG_INFO("Bluetooth pairing started successfully");
                    } else {
                        LOG_ERROR("Failed to start Bluetooth pairing");
                    }
                    break;
                case BT_MSG_TYPE_PAIR_CANCEL:
                    LOG_INFO("Pair cancel message received");
                    // 处理取消配对
                    if (bluetooth_stop_pair() == SUCCESS) {
                        LOG_INFO("Bluetooth pairing stopped successfully");
                    } else {
                        LOG_ERROR("Failed to stop Bluetooth pairing");
                    }
                    break;
                default:
                    break;
            }
        }
        
        // 处理连接事件
        LOG_DEBUG("Processing Bluetooth connection events...");
        // 这里可以添加实际的连接事件处理代码
        
        // 模拟连接状态变化
        static bool simulate_connection = false;
        if (rand() % 1000 == 0) { // 模拟连接状态变化
            simulate_connection = !simulate_connection;
            
            pthread_mutex_lock(&g_bluetooth_shared_data.data_mutex);
            g_bluetooth_shared_data.connected = simulate_connection;
            if (simulate_connection) {
                strcpy(g_bluetooth_shared_data.device_name, "Test Device");
                strcpy(g_bluetooth_shared_data.device_addr, "00:11:22:33:44:55");
                g_bluetooth_shared_data.reconnect_attempts = 0;
                LOG_INFO("Attempting to connect to Bluetooth device: %s", g_bluetooth_shared_data.device_addr);
                
                // 调用蓝牙连接函数
                if (bluetooth_connect(g_bluetooth_shared_data.device_addr) == SUCCESS) {
                    LOG_INFO("Bluetooth device connected: %s (%s)",
                             g_bluetooth_shared_data.device_name,
                             g_bluetooth_shared_data.device_addr);
                    
                    // 获取连接设备信息
                    char dev_name[32] = {0};
                    if (bluetooth_get_dev_name(dev_name, sizeof(dev_name)) == SUCCESS) {
                        LOG_INFO("Connected device name: %s", dev_name);
                        strcpy(g_bluetooth_shared_data.device_name, dev_name);
                    }
                    
                    char dev_addr[18] = {0};
                    if (bluetooth_get_dev_addr(dev_addr, sizeof(dev_addr)) == SUCCESS) {
                        LOG_INFO("Connected device address: %s", dev_addr);
                    }
                    
                    int dev_type = bluetooth_get_dev_type();
                    LOG_INFO("Connected device type: %d", dev_type);
                    
                    // 发送连接改变消息
                    bluetooth_send_message(BT_MSG_TYPE_CONNECTION_CHANGED, NULL, 0);
                } else {
                    LOG_ERROR("Failed to connect to Bluetooth device: %s", g_bluetooth_shared_data.device_addr);
                    simulate_connection = false;
                }
            } else {
                LOG_INFO("Bluetooth device disconnected");
                // 调用蓝牙断开函数
                bluetooth_disconnect();
                // 发送连接改变消息
                bluetooth_send_message(BT_MSG_TYPE_CONNECTION_CHANGED, NULL, 0);
            }
            pthread_mutex_unlock(&g_bluetooth_shared_data.data_mutex);
        }
        
        // 短暂休眠
        usleep(50000); // 0.05秒
    }
}

/**
 * @brief 蓝牙事件轮询线程函数
 * @param arg 线程参数
 */
static void *bluetooth_event_thread(void *arg) {
    LOG_INFO("Bluetooth event thread started");
    
    // 初始化事件轮询
    LOG_INFO("Initializing Bluetooth event polling...");
    // 这里可以添加实际的事件轮询初始化代码
    
    // 主循环
    while (1) {
        // 轮询蓝牙状态和事件
        LOG_DEBUG("Polling Bluetooth status and events...");
        
        // 调用蓝牙模块的事件轮询函数，处理实际的蓝牙事件
        bluetooth_event_poll();
        
        // 检查连接状态
        pthread_mutex_lock(&g_bluetooth_shared_data.data_mutex);
        bool connected = g_bluetooth_shared_data.connected;
        pthread_mutex_unlock(&g_bluetooth_shared_data.data_mutex);
        
        // 检查音频流状态
        if (connected) {
            int stream_state = bluetooth_get_audio_stream_state();
            LOG_DEBUG("Current Bluetooth audio stream state: %d", stream_state);
            
            // 根据音频流状态更新播放状态
            pthread_mutex_lock(&g_bluetooth_shared_data.data_mutex);
            g_bluetooth_shared_data.audio_playing = (stream_state == 1);
            pthread_mutex_unlock(&g_bluetooth_shared_data.data_mutex);
        }
        
        // 短暂休眠
        usleep(100000); // 0.1秒
    }
}

/**
 * @brief 蓝牙自动重连线程函数
 * @param arg 线程参数
 */
static void *bluetooth_reconnect_thread(void *arg) {
    LOG_INFO("Bluetooth reconnect thread started");
    
    // 初始化自动重连
    LOG_INFO("Initializing Bluetooth auto-reconnect...");
    // 这里可以添加实际的自动重连初始化代码
    
    // 主循环
    while (1) {
        // 检查连接状态
        pthread_mutex_lock(&g_bluetooth_shared_data.data_mutex);
        bool connected = g_bluetooth_shared_data.connected;
        int reconnect_attempts = g_bluetooth_shared_data.reconnect_attempts;
        char device_addr[18];
        strcpy(device_addr, g_bluetooth_shared_data.device_addr);
        pthread_mutex_unlock(&g_bluetooth_shared_data.data_mutex);
        
        // 处理自动重连逻辑
        if (!connected && strlen(device_addr) > 0) {
            pthread_mutex_lock(&g_bluetooth_shared_data.data_mutex);
            g_bluetooth_shared_data.reconnect_attempts++;
            int attempts = g_bluetooth_shared_data.reconnect_attempts;
            pthread_mutex_unlock(&g_bluetooth_shared_data.data_mutex);
            
            LOG_INFO("Attempting to reconnect to device: %s (Attempt %d)",
                     device_addr, attempts);
            
            // 发送自动重连消息
            bluetooth_send_message(BT_MSG_TYPE_AUTO_RECONNECT, NULL, 0);
            
            // 尝试实际重连
            LOG_INFO("Calling bluetooth_connect() to reconnect to device: %s", device_addr);
            if (bluetooth_connect(device_addr) == SUCCESS) {
                LOG_INFO("Successfully reconnected to device: %s", device_addr);
                
                pthread_mutex_lock(&g_bluetooth_shared_data.data_mutex);
                g_bluetooth_shared_data.connected = true;
                g_bluetooth_shared_data.reconnect_attempts = 0;
                pthread_mutex_unlock(&g_bluetooth_shared_data.data_mutex);
                
                // 发送连接改变消息
                bluetooth_send_message(BT_MSG_TYPE_CONNECTION_CHANGED, NULL, 0);
            } else {
                LOG_ERROR("Failed to reconnect to device: %s", device_addr);
                
                // 增加重连尝试次数
                pthread_mutex_lock(&g_bluetooth_shared_data.data_mutex);
                g_bluetooth_shared_data.reconnect_attempts++;
                pthread_mutex_unlock(&g_bluetooth_shared_data.data_mutex);
            }
        }
        
        // 短暂休眠
        usleep(1000000); // 1秒
    }
}

/**
 * @brief 蓝牙MESH处理线程函数
 * @param arg 线程参数
 */
static void *bluetooth_mesh_thread(void *arg) {
    LOG_INFO("Bluetooth MESH thread started");
    
    // 初始化MESH处理
    LOG_INFO("Initializing Bluetooth MESH processing...");
    // 这里可以添加实际的MESH处理初始化代码
    
    // 主循环
    while (1) {
        // 处理MESH事件
        LOG_DEBUG("Processing Bluetooth MESH events...");
        // 这里可以添加实际的MESH事件处理代码
        
        // 检查MESH状态
        pthread_mutex_lock(&g_bluetooth_shared_data.data_mutex);
        bool mesh_enabled = g_bluetooth_shared_data.mesh_enabled;
        pthread_mutex_unlock(&g_bluetooth_shared_data.data_mutex);
        
        // 模拟MESH事件
        if (mesh_enabled && rand() % 200 == 0) { // 模拟MESH事件
            LOG_INFO("Bluetooth MESH event received");
            // 发送MESH事件消息
            bluetooth_send_message(BT_MSG_TYPE_MESH_EVENT, NULL, 0);
        }
        
        // 短暂休眠
        usleep(100000); // 0.1秒
    }
}

/**
 * @brief 蓝牙线程监控函数
 * @param arg 线程参数
 */
static void *bluetooth_monitor_thread(void *arg) {
    LOG_INFO("Bluetooth monitor thread started");
    
    // 主循环
    while (1) {
        // 监控蓝牙线程状态
        LOG_DEBUG("Monitoring Bluetooth threads...");
        
        // 检查共享数据状态
        pthread_mutex_lock(&g_bluetooth_shared_data.data_mutex);
        bool connected = g_bluetooth_shared_data.connected;
        bool playing = g_bluetooth_shared_data.audio_playing;
        int volume = g_bluetooth_shared_data.volume;
        int reconnect_attempts = g_bluetooth_shared_data.reconnect_attempts;
        bool mesh_enabled = g_bluetooth_shared_data.mesh_enabled;
        char device_name[32] = {0};
        char device_addr[18] = {0};
        strcpy(device_name, g_bluetooth_shared_data.device_name);
        strcpy(device_addr, g_bluetooth_shared_data.device_addr);
        pthread_mutex_unlock(&g_bluetooth_shared_data.data_mutex);
        
        // 打印当前状态
        LOG_DEBUG("Bluetooth status - Connected: %d, Playing: %d, Volume: %d, Reconnect attempts: %d, MESH enabled: %d",
                 connected, playing, volume, reconnect_attempts, mesh_enabled);
        
        if (connected) {
            LOG_DEBUG("Connected device - Name: %s, Address: %s", device_name, device_addr);
            
            // 获取并报告更多蓝牙状态信息
            
            // 获取设备信息
            char dev_name[32] = {0};
            char dev_addr[18] = {0};
            
            if (bluetooth_get_dev_name(dev_name, sizeof(dev_name)) == SUCCESS) {
                LOG_INFO("Monitored - Device Name: %s", dev_name);
            }
            
            if (bluetooth_get_dev_addr(dev_addr, sizeof(dev_addr)) == SUCCESS) {
                LOG_INFO("Monitored - Device Address: %s", dev_addr);
            }
            
            // 获取音频状态
            int stream_state = bluetooth_get_audio_stream_state();
            int audio_volume = bluetooth_get_audio_volume();
            LOG_INFO("Monitored - Stream State: %d, Audio Volume: %d", stream_state, audio_volume);
            
            // 检查自动重连状态
            bool auto_connect = bluetooth_get_auto_connect();
            LOG_INFO("Monitored - Auto-connect: %d", auto_connect);
            
            // 获取重连参数
            int max_attempts = 0;
            int interval = 0;
            if (bluetooth_get_reconnect_params(&max_attempts, &interval) == SUCCESS) {
                LOG_INFO("Monitored - Reconnect Params: Max attempts: %d, Interval: %d sec", max_attempts, interval);
            }
        }
        
        // 检查线程池状态
        int active_threads, pending_tasks;
        if (thread_pool_get_status("bluetooth", &active_threads, &pending_tasks) == SUCCESS) {
            LOG_DEBUG("Bluetooth thread pool - Active threads: %d, Pending tasks: %d",
                     active_threads, pending_tasks);
        }
        
        // 短暂休眠
        usleep(500000); // 0.5秒
    }
}

/**
 * @brief 蓝牙进程主函数
 * @param arg 进程参数
 */
static void bluetooth_process_main(void *arg) {
    LOG_INFO("Bluetooth process started");
    
    // 初始化蓝牙系统
    LOG_INFO("Initializing Bluetooth system...");
    
    // 初始化共享数据
    pthread_mutex_init(&g_bluetooth_shared_data.data_mutex, NULL);
    
    // 初始化消息队列
    LOG_INFO("Initializing Bluetooth message queue...");
    bluetooth_msg_queue_init(&g_bluetooth_shared_data.msg_queue, 100);
    
    // 订阅按键事件
    LOG_INFO("Subscribing to key events...");
    event_subscribe(EVENT_KEY_PRESSED, bluetooth_handle_key_event, NULL);
    
    // 初始化蓝牙模块
    LOG_INFO("Initializing Bluetooth module...");
    if (bluetooth_init(NULL) == SUCCESS) {
        LOG_INFO("Bluetooth module initialized successfully");
    } else {
        LOG_ERROR("Failed to initialize Bluetooth module");
        return;
    }
    
    // 创建蓝牙线程池
    LOG_INFO("Creating Bluetooth thread pool...");
    thread_pool_create("bluetooth", 5, 10, 5);
    
    // 创建各功能线程
    pthread_t audio_thread, connection_thread, event_thread, reconnect_thread, mesh_thread, monitor_thread;
    
    // 创建音频处理线程（高优先级）
    pthread_attr_t audio_attr;
    pthread_attr_init(&audio_attr);
    struct sched_param audio_param;
    audio_param.sched_priority = 90;
    pthread_attr_setschedparam(&audio_attr, &audio_param);
    pthread_create(&audio_thread, &audio_attr, bluetooth_audio_thread, NULL);
    pthread_attr_destroy(&audio_attr);
    
    // 创建连接管理线程（中高优先级）
    pthread_attr_t conn_attr;
    pthread_attr_init(&conn_attr);
    struct sched_param conn_param;
    conn_param.sched_priority = 70;
    pthread_attr_setschedparam(&conn_attr, &conn_param);
    pthread_create(&connection_thread, &conn_attr, bluetooth_connection_thread, NULL);
    pthread_attr_destroy(&conn_attr);
    
    // 创建事件轮询线程（中优先级）
    pthread_attr_t event_attr;
    pthread_attr_init(&event_attr);
    struct sched_param event_param;
    event_param.sched_priority = 50;
    pthread_attr_setschedparam(&event_attr, &event_param);
    pthread_create(&event_thread, &event_attr, bluetooth_event_thread, NULL);
    pthread_attr_destroy(&event_attr);
    
    // 创建自动重连线程（低优先级）
    pthread_attr_t reconnect_attr;
    pthread_attr_init(&reconnect_attr);
    struct sched_param reconnect_param;
    reconnect_param.sched_priority = 30;
    pthread_attr_setschedparam(&reconnect_attr, &reconnect_param);
    pthread_create(&reconnect_thread, &reconnect_attr, bluetooth_reconnect_thread, NULL);
    pthread_attr_destroy(&reconnect_attr);
    
    // 创建MESH处理线程（中优先级）
    pthread_attr_t mesh_attr;
    pthread_attr_init(&mesh_attr);
    struct sched_param mesh_param;
    mesh_param.sched_priority = 50;
    pthread_attr_setschedparam(&mesh_attr, &mesh_param);
    pthread_create(&mesh_thread, &mesh_attr, bluetooth_mesh_thread, NULL);
    pthread_attr_destroy(&mesh_attr);
    
    // 创建监控线程（低优先级）
    pthread_attr_t monitor_attr;
    pthread_attr_init(&monitor_attr);
    struct sched_param monitor_param;
    monitor_param.sched_priority = 20;
    pthread_attr_setschedparam(&monitor_attr, &monitor_param);
    pthread_create(&monitor_thread, &monitor_attr, bluetooth_monitor_thread, NULL);
    pthread_attr_destroy(&monitor_attr);
    
    // 等待所有线程结束
    pthread_join(audio_thread, NULL);
    pthread_join(connection_thread, NULL);
    pthread_join(event_thread, NULL);
    pthread_join(reconnect_thread, NULL);
    pthread_join(mesh_thread, NULL);
    pthread_join(monitor_thread, NULL);
    
    // 清理资源
    bluetooth_msg_queue_destroy(&g_bluetooth_shared_data.msg_queue);
    pthread_mutex_destroy(&g_bluetooth_shared_data.data_mutex);
    
    // 销毁线程池
    thread_pool_destroy("bluetooth");
    LOG_INFO("Bluetooth process exited");
}

/**
 * @brief 各个进程的terminate_func实现
 ******************************************************************************************/

/**
 * @brief 存储进程终止函数
 * @return SUCCESS/FAILURE
 */
static int storage_process_terminate(void) {
    LOG_INFO("Terminating storage process...");
    // 这里可以添加实际的存储进程终止代码
    return SUCCESS;
}

/**
 * @brief 网络进程终止函数
 * @return SUCCESS/FAILURE
 */
static int network_process_terminate(void) {
    LOG_INFO("Terminating network process...");
    // 这里可以添加实际的网络进程终止代码
    return SUCCESS;
}

/**
 * @brief 音频进程终止函数
 * @return SUCCESS/FAILURE
 */
static int audio_process_terminate(void) {
    LOG_INFO("Terminating audio process...");
    // 这里可以添加实际的音频进程终止代码
    return SUCCESS;
}

/**
 * @brief 系统进程终止函数
 * @return SUCCESS/FAILURE
 */
static int system_process_terminate(void) {
    LOG_INFO("Terminating system process...");
    // 这里可以添加实际的系统进程终止代码
    return SUCCESS;
}

/**
 * @brief 远程控制进程终止函数
 * @return SUCCESS/FAILURE
 */
static int remote_control_process_terminate(void) {
    LOG_INFO("Terminating remote control process...");
    // 这里可以添加实际的远程控制进程终止代码
    return SUCCESS;
}

/**
 * @brief 蓝牙进程终止函数
 * @return SUCCESS/FAILURE
 */
static int bluetooth_process_terminate(void) {
    LOG_INFO("Terminating bluetooth process...");
    
    // 销毁蓝牙线程池
    LOG_INFO("Destroying Bluetooth thread pool...");
    thread_pool_destroy("bluetooth");
    
    // 清理蓝牙资源
    LOG_INFO("Cleaning up Bluetooth resources...");
    // 这里可以添加实际的蓝牙资源清理代码
    
    LOG_INFO("Bluetooth process terminated successfully");
    return SUCCESS;
}

/******************************************************************************************
 * 进程管理模块全局变量
 ******************************************************************************************/

static ProcessInfo_t *g_process_list = NULL;
static pthread_mutex_t g_process_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_process_manager_init = false;

/******************************************************************************************
 * 进程管理模块内部函数
 ******************************************************************************************/

/**
 * @brief 查找进程信息
 * @param name 进程名称
 * @return 进程信息指针，未找到返回NULL
 */
static ProcessInfo_t *find_process(const char *name) {
    if (!name) {
        return NULL;
    }
    
    ProcessInfo_t *current = g_process_list;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

/**
 * @brief 添加进程信息
 * @param name 进程名称
 * @param main_func 进程主函数
 * @param main_arg 进程主函数参数
 * @param terminate_func 进程终止函数
 * @return 添加结果：0表示成功，非0表示失败
 */
static int add_process(const char *name, ProcessMainFunc main_func, void *main_arg, int (*terminate_func)(void)) {
    if (!name) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    // 检查进程是否已存在
    if (find_process(name)) {
        LOG_WARN("Process already exists: %s", name);
        return -1;
    }
    
    // 创建进程信息结构体
    ProcessInfo_t *process = (ProcessInfo_t *)malloc(sizeof(ProcessInfo_t));
    if (!process) {
        LOG_ERROR("Failed to allocate memory for process info");
        return -1;
    }
    
    // 初始化进程信息
    memset(process, 0, sizeof(ProcessInfo_t));
    strncpy(process->name, name, sizeof(process->name) - 1);
    process->main_func = main_func;
    process->main_arg = main_arg;
    process->terminate_func = terminate_func;
    process->pid = -1;
    process->status = PROCESS_STATUS_IDLE;
    process->priority = PROCESS_PRIORITY_NORMAL;
    process->start_time = 0;
    process->restart_count = 0;
    process->auto_restart = true;
    process->dependency_count = 0;
    process->msg_pipe[0] = -1;
    process->msg_pipe[1] = -1;
    // 初始化资源限制
    process->limits.max_cpu_usage = 100;  // 默认无限制
    process->limits.max_memory = 0;      // 默认无限制
    process->limits.max_file_descriptors = 1024;  // 默认1024
    process->next = NULL;
    
    // 初始化依赖数组
    for (int i = 0; i < 10; i++) {
        process->dependencies[i] = NULL;
    }
    
    // 添加到进程列表
    if (!g_process_list) {
        g_process_list = process;
    } else {
        ProcessInfo_t *current = g_process_list;
        while (current->next) {
            current = current->next;
        }
        current->next = process;
    }
    
    LOG_INFO("Added process: %s", name);
    return 0;
}

/**
 * @brief 移除进程信息
 * @param name 进程名称
 * @return 移除结果：0表示成功，非0表示失败
 */
static int remove_process(const char *name) {
    if (!name) {
        return -1;
    }
    
    ProcessInfo_t *prev = NULL;
    ProcessInfo_t *current = g_process_list;
    
    while (current) {
        if (strcmp(current->name, name) == 0) {
            // 从列表中移除
            if (prev) {
                prev->next = current->next;
            } else {
                g_process_list = current->next;
            }
            
            // 关闭管道
            if (current->msg_pipe[0] != -1) {
                close(current->msg_pipe[0]);
            }
            if (current->msg_pipe[1] != -1) {
                close(current->msg_pipe[1]);
            }
            
            // 释放依赖名称
            for (int i = 0; i < current->dependency_count; i++) {
                if (current->dependencies[i]) {
                    free(current->dependencies[i]);
                }
            }
            
            // 释放内存
            free(current);
            LOG_INFO("Removed process: %s", name);
            return 0;
        }
        
        prev = current;
        current = current->next;
    }
    
    LOG_WARN("Process not found: %s", name);
    return -1;
}

/**
 * @brief 检查进程是否运行
 * @param pid 进程ID
 * @return 运行状态：true表示运行，false表示未运行
 */
static bool is_process_running(pid_t pid) {
    if (pid == -1) {
        return false;
    }
    
#ifdef _WIN32
    // Windows 平台：使用GetExitCodeProcess检查进程状态
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess == NULL) {
        return false;
    }
    
    DWORD exitCode;
    if (!GetExitCodeProcess(hProcess, &exitCode)) {
        CloseHandle(hProcess);
        return false;
    }
    
    CloseHandle(hProcess);
    return (exitCode == STILL_ACTIVE);
#else
    // Unix 平台：使用kill(0)检查进程是否存在
    return (kill(pid, 0) == 0);
#endif
}

/**
 * @brief 创建进程（跨平台实现）
 * @param process 进程信息
 * @return 进程ID，失败返回-1
 */
static pid_t create_process(ProcessInfo_t *process) {
    if (!process) {
        return -1;
    }
    
    // 创建进程间通信管道
    if (pipe(process->msg_pipe) == -1) {
        LOG_ERROR("Failed to create message pipe");
        return -1;
    }
    
#ifdef _WIN32
    // Windows 平台：使用CreateThread创建线程模拟进程
    unsigned int thread_id;
    HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, (unsigned int (__stdcall *)(void *))process->main_func, process->main_arg, 0, &thread_id);
    if (hThread == NULL) {
        LOG_ERROR("Failed to create process (thread) on Windows");
        close(process->msg_pipe[0]);
        close(process->msg_pipe[1]);
        process->msg_pipe[0] = -1;
        process->msg_pipe[1] = -1;
        return -1;
    }
    
    // 设置线程优先级
    switch (process->priority) {
        case PROCESS_PRIORITY_HIGH:
            SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST);
            break;
        case PROCESS_PRIORITY_LOW:
            SetThreadPriority(hThread, THREAD_PRIORITY_LOWEST);
            break;
        default:
            SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);
            break;
    }
    
    // 不关闭线程句柄，以便后续可以终止线程
    return (pid_t)thread_id;
#else
    // Unix 平台：使用fork()创建进程
    pid_t pid = fork();
    if (pid == -1) {
        LOG_ERROR("Failed to fork process: %d", errno);
        close(process->msg_pipe[0]);
        close(process->msg_pipe[1]);
        process->msg_pipe[0] = -1;
        process->msg_pipe[1] = -1;
        return -1;
    } else if (pid == 0) {
        // 子进程
        close(process->msg_pipe[1]); // 关闭写端
        
        // 设置进程优先级
        if (process->priority == PROCESS_PRIORITY_HIGH) {
            nice(-10); // 提高优先级
        } else if (process->priority == PROCESS_PRIORITY_LOW) {
            nice(10); // 降低优先级
        }
        
        // 执行进程主函数
        if (process->main_func) {
            process->main_func(process->main_arg);
        }
        
        close(process->msg_pipe[0]);
        exit(0);
    } else {
        // 父进程
        close(process->msg_pipe[0]); // 关闭读端
        return pid;
    }
#endif
}

/**
 * @brief 终止进程（跨平台实现）
 * @param process 进程信息
 * @return 终止结果：0表示成功，非0表示失败
 */
static int terminate_process(ProcessInfo_t *process) {
    if (!process || process->pid == -1) {
        return SUCCESS;
    }
    
#ifdef _WIN32
    // Windows 平台：使用TerminateThread终止线程
    HANDLE hThread = OpenThread(THREAD_TERMINATE, FALSE, (DWORD)process->pid);
    if (hThread != NULL) {
        if (!TerminateThread(hThread, 0)) {
            CloseHandle(hThread);
            LOG_ERROR("Failed to terminate process (thread) on Windows");
            return FAILURE;
        }
        CloseHandle(hThread);
    }
#else
    // Unix 平台：使用SIGTERM信号终止进程
    if (kill(process->pid, SIGTERM) == -1) {
        LOG_ERROR("Failed to terminate process: %d", errno);
        return FAILURE;
    }
    
    // 等待进程退出
    waitpid(process->pid, NULL, WNOHANG);
#endif
    
    // 关闭管道
    if (process->msg_pipe[1] != -1) {
        close(process->msg_pipe[1]);
        process->msg_pipe[1] = -1;
    }
    if (process->msg_pipe[0] != -1) {
        close(process->msg_pipe[0]);
        process->msg_pipe[0] = -1;
    }
    
    process->pid = -1;
    process->status = PROCESS_STATUS_IDLE;
    return SUCCESS;
}

/**
 * @brief 检查依赖的进程是否都已启动
 * @param process 进程信息
 * @return 检查结果：true表示所有依赖都已启动，false表示有依赖未启动
 */
static bool check_dependencies(ProcessInfo_t *process) {
    if (!process || process->dependency_count == 0) {
        return true;
    }
    
    for (int i = 0; i < process->dependency_count; i++) {
        if (!process->dependencies[i]) {
            continue;
        }
        
        ProcessInfo_t *dep_process = find_process(process->dependencies[i]);
        if (!dep_process || dep_process->status != PROCESS_STATUS_RUNNING) {
            LOG_WARN("Dependency process %s not running for %s", process->dependencies[i], process->name);
            return false;
        }
    }
    
    return true;
}

/**
 * @brief 按优先级排序进程列表
 * @return 排序后的进程列表
 */
static ProcessInfo_t *sort_processes_by_priority(void) {
    // 简单的冒泡排序
    ProcessInfo_t *sorted = NULL;
    ProcessInfo_t *current = g_process_list;
    
    while (current) {
        ProcessInfo_t *next = current->next;
        
        // 插入到排序后的列表中
        if (!sorted) {
            sorted = current;
            current->next = NULL;
        } else {
            ProcessInfo_t *prev = NULL;
            ProcessInfo_t *sorted_current = sorted;
            
            while (sorted_current && sorted_current->priority >= current->priority) {
                prev = sorted_current;
                sorted_current = sorted_current->next;
            }
            
            if (!prev) {
                current->next = sorted;
                sorted = current;
            } else {
                prev->next = current;
                current->next = sorted_current;
            }
        }
        
        current = next;
    }
    
    return sorted;
}

/******************************************************************************************
 * 进程管理模块对外接口
 ******************************************************************************************/

/**
 * @brief 初始化进程管理模块
 * @return 初始化结果：0表示成功，非0表示失败
 */
int process_manager_init(void) {
    if (g_process_manager_init) {
        LOG_INFO("Process manager already initialized");
        return SUCCESS;
    }
    
    // 初始化进程列表
    g_process_list = NULL;
    pthread_mutex_init(&g_process_list_mutex, NULL);
    
    // 注册系统进程
    process_manager_register_process("storage", storage_process_main, NULL, storage_process_terminate);
    process_manager_register_process("network", network_process_main, NULL, network_process_terminate);
    process_manager_register_process("audio", audio_process_main, NULL, audio_process_terminate);
    process_manager_register_process("system", system_process_main, NULL, system_process_terminate);
    process_manager_register_process("remote_control", remote_control_process_main_impl, NULL, remote_control_process_terminate);
    process_manager_register_process("bluetooth", bluetooth_process_main, NULL, bluetooth_process_terminate);
    
    g_process_manager_init = true;
    LOG_INFO("Process manager init success");
    
    return SUCCESS;
}

/**
 * @brief 反初始化进程管理模块
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int process_manager_deinit(void) {
    if (!g_process_manager_init) {
        LOG_INFO("Process manager not initialized");
        return SUCCESS;
    }
    
    // 停止所有进程
    process_manager_stop_all_processes();
    
    // 清空进程列表
    ProcessInfo_t *current = g_process_list;
    while (current) {
        ProcessInfo_t *next = current->next;
        
        // 关闭管道
        if (current->msg_pipe[0] != -1) {
            close(current->msg_pipe[0]);
        }
        if (current->msg_pipe[1] != -1) {
            close(current->msg_pipe[1]);
        }
        
        // 释放依赖名称
        for (int i = 0; i < current->dependency_count; i++) {
            if (current->dependencies[i]) {
                free(current->dependencies[i]);
            }
        }
        
        free(current);
        current = next;
    }
    g_process_list = NULL;
    
    // 销毁互斥锁
    pthread_mutex_destroy(&g_process_list_mutex);
    
    g_process_manager_init = false;
    LOG_INFO("Process manager deinitialized");
    
    return SUCCESS;
}

/**
 * @brief 注册进程
 * @param name 进程名称
 * @param main_func 进程主函数
 * @param main_arg 进程主函数参数
 * @param terminate_func 进程终止函数
 * @return 注册结果：0表示成功，非0表示失败
 */
int process_manager_register_process(const char *name, ProcessMainFunc main_func, void *main_arg, int (*terminate_func)(void)) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    int ret = add_process(name, main_func, main_arg, terminate_func);
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    
    return ret;
}

/**
 * @brief 设置进程优先级
 * @param name 进程名称
 * @param priority 进程优先级
 * @return 设置结果：0表示成功，非0表示失败
 */
int process_manager_set_process_priority(const char *name, ProcessPriority_t priority) {
    if (!g_process_manager_init || priority >= PROCESS_PRIORITY_MAX) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    if (!process) {
        LOG_ERROR("Process not found: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    process->priority = priority;
    LOG_INFO("Set process %s priority to %d", name, priority);
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    return 0;
}

/**
 * @brief 设置进程自动重启
 * @param name 进程名称
 * @param auto_restart 是否自动重启
 * @return 设置结果：0表示成功，非0表示失败
 */
int process_manager_set_process_auto_restart(const char *name, bool auto_restart) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    if (!process) {
        LOG_ERROR("Process not found: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    process->auto_restart = auto_restart;
    LOG_INFO("Set process %s auto_restart to %s", name, auto_restart ? "true" : "false");
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    return 0;
}

/**
 * @brief 添加进程依赖
 * @param name 进程名称
 * @param dependency 依赖的进程名称
 * @return 添加结果：0表示成功，非0表示失败
 */
int process_manager_add_process_dependency(const char *name, const char *dependency) {
    if (!g_process_manager_init || !name || !dependency) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    if (!process) {
        LOG_ERROR("Process not found: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    if (process->dependency_count >= 10) {
        LOG_ERROR("Too many dependencies for process: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    // 检查依赖的进程是否存在
    if (!find_process(dependency)) {
        LOG_ERROR("Dependency process not found: %s", dependency);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    // 添加依赖
    process->dependencies[process->dependency_count] = strdup(dependency);
    if (!process->dependencies[process->dependency_count]) {
        LOG_ERROR("Failed to allocate memory for dependency");
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    process->dependency_count++;
    LOG_INFO("Added dependency %s to process %s", dependency, name);
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    return 0;
}

/**
 * @brief 启动进程
 * @param name 进程名称
 * @return 启动结果：0表示成功，非0表示失败
 */
int process_manager_start_process(const char *name) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    if (!process) {
        LOG_ERROR("Process not found: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    if (process->status == PROCESS_STATUS_RUNNING) {
        LOG_WARN("Process already running: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return 0;
    }
    
    // 检查依赖的进程是否都已启动
    if (!check_dependencies(process)) {
        LOG_ERROR("Dependencies not met for process: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    process->status = PROCESS_STATUS_STARTING;
    
    // 使用统一的进程创建接口
    int ret = 0;
    if (process->main_func) {
        // 使用统一的进程创建接口
        pid_t pid = create_process(process);
        if (pid == -1) {
            LOG_ERROR("Failed to create process: %s", name);
            process->status = PROCESS_STATUS_IDLE;
            MUTEX_LOCK_UNLOCK(g_process_list_mutex);
            return -1;
        }
        
        process->pid = pid;
        process->status = PROCESS_STATUS_RUNNING;
        process->start_time = time(NULL);
        process->restart_count++;
        ret = SUCCESS;
        LOG_INFO("Started process: %s, pid: %d", name, pid);
    } else {
        LOG_ERROR("Process has no main function: %s", name);
        process->status = PROCESS_STATUS_IDLE;
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    if (ret != SUCCESS) {
        LOG_ERROR("Failed to start process: %s", name);
        process->status = PROCESS_STATUS_IDLE;
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    // 如果是使用模块接口启动的进程，设置虚拟进程ID
    if (process->pid == -1) {
        process->pid = 1; // 虚拟进程ID
    }
    
    process->status = PROCESS_STATUS_RUNNING;
    process->start_time = time(NULL);
    process->restart_count++;
    LOG_INFO("Started process: %s, pid: %d", name, process->pid);
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    return 0;
}

/**
 * @brief 停止进程
 * @param name 进程名称
 * @return 停止结果：0表示成功，非0表示失败
 */
int process_manager_stop_process(const char *name) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    if (!process) {
        LOG_ERROR("Process not found: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    if (process->status == PROCESS_STATUS_IDLE) {
        LOG_WARN("Process not running: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return 0;
    }
    
    process->status = PROCESS_STATUS_STOPPING;
    
    // 使用统一的进程终止接口
    int ret = 0;
    if (process->terminate_func) {
        ret = process->terminate_func();
    } else {
        // 使用统一的进程终止接口
        ret = terminate_process(process);
    }
    
    if (ret != SUCCESS) {
        LOG_ERROR("Failed to stop process: %s", name);
        process->status = PROCESS_STATUS_CRASHED;
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    // 重置进程信息
    process->pid = -1;
    process->status = PROCESS_STATUS_IDLE;
    LOG_INFO("Stopped process: %s", name);
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    return 0;
}

/**
 * @brief 启动所有进程
 * @return 启动结果：0表示成功，非0表示失败
 */
int process_manager_start_all_processes(void) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    // 按优先级排序进程列表
    ProcessInfo_t *sorted_list = sort_processes_by_priority();
    
    ProcessInfo_t *current = sorted_list;
    int failed = 0;
    
    while (current) {
        if (current->status == PROCESS_STATUS_IDLE) {
            // 检查依赖的进程是否都已启动
            if (!check_dependencies(current)) {
                LOG_WARN("Dependencies not met for process: %s, skipping", current->name);
                current = current->next;
                continue;
            }
            
            current->status = PROCESS_STATUS_STARTING;
            
            // 使用统一的进程创建接口
            int ret = 0;
            if (current->main_func) {
                // 使用统一的进程创建接口
                pid_t pid = create_process(current);
                if (pid == -1) {
                    LOG_ERROR("Failed to create process: %s", current->name);
                    current->status = PROCESS_STATUS_IDLE;
                    failed++;
                    current = current->next;
                    continue;
                }
                
                current->pid = pid;
                current->status = PROCESS_STATUS_RUNNING;
                current->start_time = time(NULL);
                current->restart_count++;
                ret = SUCCESS;
                LOG_INFO("Started process: %s, pid: %d", current->name, pid);
            } else {
                LOG_ERROR("Process has no main function: %s", current->name);
                current->status = PROCESS_STATUS_IDLE;
                failed++;
                current = current->next;
                continue;
            }
            
            if (ret != SUCCESS) {
                LOG_ERROR("Failed to start process: %s", current->name);
                current->status = PROCESS_STATUS_IDLE;
                failed++;
            } else {
                // 如果是使用模块接口启动的进程，设置虚拟进程ID
                if (current->pid == -1) {
                    current->pid = 1; // 虚拟进程ID
                }
                
                current->status = PROCESS_STATUS_RUNNING;
                current->start_time = time(NULL);
                current->restart_count++;
                LOG_INFO("Started process: %s, pid: %d", current->name, current->pid);
            }
        }
        current = current->next;
    }
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    
    if (failed > 0) {
        LOG_ERROR("Failed to start %d processes", failed);
        return -1;
    }
    
    LOG_INFO("Started all processes");
    return 0;
}

/**
 * @brief 停止所有进程
 * @return 停止结果：0表示成功，非0表示失败
 */
int process_manager_stop_all_processes(void) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *current = g_process_list;
    int failed = 0;
    
    while (current) {
        if (current->status == PROCESS_STATUS_RUNNING) {
            current->status = PROCESS_STATUS_STOPPING;
            
            // 使用统一的进程终止接口
            int ret = 0;
            if (current->terminate_func) {
                ret = current->terminate_func();
            } else {
                // 使用统一的进程终止接口
                ret = terminate_process(current);
            }
            
            if (ret != SUCCESS) {
                LOG_ERROR("Failed to stop process: %s", current->name);
                current->status = PROCESS_STATUS_CRASHED;
                failed++;
            } else {
                current->pid = -1;
                current->status = PROCESS_STATUS_IDLE;
                LOG_INFO("Stopped process: %s", current->name);
            }
        }
        current = current->next;
    }
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    
    if (failed > 0) {
        LOG_ERROR("Failed to stop %d processes", failed);
        return -1;
    }
    
    LOG_INFO("Stopped all processes");
    return 0;
}

/**
 * @brief 获取进程ID
 * @param name 进程名称
 * @return 进程ID，失败返回-1
 */
pid_t process_manager_get_process_pid(const char *name) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    pid_t pid = process ? process->pid : -1;
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    
    return pid;
}

/**
 * @brief 获取进程状态
 * @param name 进程名称
 * @return 进程状态，失败返回PROCESS_STATUS_IDLE
 */
ProcessStatus_t process_manager_get_process_status(const char *name) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return PROCESS_STATUS_IDLE;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    ProcessStatus_t status = PROCESS_STATUS_IDLE;
    
    if (process) {
        status = process->status;
    }
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    
    return status;
}

/**
 * @brief 检查进程是否运行
 * @param name 进程名称
 * @return 运行状态：true表示运行，false表示未运行
 */
bool process_manager_is_process_running(const char *name) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return false;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    bool running = false;
    
    if (process) {
        if (process->status == PROCESS_STATUS_RUNNING) {
            // 验证进程是否真的在运行
            running = is_process_running(process->pid);
            if (!running) {
                // 更新状态
                process->status = PROCESS_STATUS_CRASHED;
                LOG_WARN("Process %s has crashed, updated status", name);
            }
        }
    }
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    
    return running;
}

/**
 * @brief 监控进程状态
 * @return 监控结果：0表示成功，非0表示失败
 */
int process_manager_monitor_processes(void) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *current = g_process_list;
    int restarted = 0;
    
    while (current) {
        if (current->status == PROCESS_STATUS_RUNNING) {
            // 验证进程是否真的在运行
            if (!is_process_running(current->pid)) {
                // 更新状态为崩溃
                current->status = PROCESS_STATUS_CRASHED;
                LOG_WARN("Process %s has crashed", current->name);
                
                // 如果设置了自动重启，尝试重启进程
                if (current->auto_restart) {
                    LOG_INFO("Attempting to restart crashed process: %s", current->name);
                    
                    // 重置进程信息
                    current->pid = -1;
                    current->status = PROCESS_STATUS_STARTING;
                    
                    // 使用统一的进程创建接口
                    int ret = 0;
                    if (current->main_func) {
                        // 使用统一的进程创建接口
                        pid_t pid = create_process(current);
                        if (pid == -1) {
                            LOG_ERROR("Failed to restart process: %s", current->name);
                            current->status = PROCESS_STATUS_CRASHED;
                            current = current->next;
                            continue;
                        }
                        
                        current->pid = pid;
                        current->status = PROCESS_STATUS_RUNNING;
                        current->start_time = time(NULL);
                        current->restart_count++;
                        ret = SUCCESS;
                        LOG_INFO("Restarted process: %s, pid: %d", current->name, pid);
                    } else {
                        LOG_ERROR("Process has no main function: %s", current->name);
                        current->status = PROCESS_STATUS_CRASHED;
                        current = current->next;
                        continue;
                    }
                    
                    if (ret != SUCCESS) {
                        LOG_ERROR("Failed to restart process: %s", current->name);
                        current->status = PROCESS_STATUS_CRASHED;
                    } else {
                        LOG_INFO("Restarted process: %s, pid: %d", current->name, current->pid);
                        restarted++;
                    }
                }
            } else {
                LOG_DEBUG("Monitoring process: %s (pid: %d, status: running)", current->name, current->pid);
            }
        }
        current = current->next;
    }
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    
    if (restarted > 0) {
        LOG_INFO("Restarted %d crashed processes", restarted);
    }
    
    return 0;
}

/**
 * @brief 发送消息到进程
 * @param name 进程名称
 * @param msg_type 消息类型
 * @param msg_data 消息数据
 * @param msg_size 消息大小
 * @return 发送结果：0表示成功，非0表示失败
 */
int process_manager_send_message(const char *name, uint32_t msg_type, void *msg_data, size_t msg_size) {
    if (!g_process_manager_init || !name) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    if (!process || process->status != PROCESS_STATUS_RUNNING || process->msg_pipe[1] == -1) {
        LOG_ERROR("Process not running or no message pipe: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    // 构建消息
    ProcessMsg_t msg;
    msg.msg_type = msg_type;
    msg.msg_size = msg_size;
    msg.msg_data = msg_data;
    
    // 发送消息
    if (write(process->msg_pipe[1], &msg, sizeof(ProcessMsg_t)) != sizeof(ProcessMsg_t)) {
        LOG_ERROR("Failed to send message to process: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    // 如果有消息数据，发送数据
    if (msg_data && msg_size > 0) {
        if (write(process->msg_pipe[1], msg_data, msg_size) != (ssize_t)msg_size) {
            LOG_ERROR("Failed to send message data to process: %s", name);
            MUTEX_LOCK_UNLOCK(g_process_list_mutex);
            return -1;
        }
    }
    
    LOG_DEBUG("Sent message type %d to process: %s", msg_type, name);
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    return 0;
}

/**
 * @brief 广播消息到所有进程
 * @param msg_type 消息类型
 * @param msg_data 消息数据
 * @param msg_size 消息大小
 * @return 广播结果：0表示成功，非0表示失败
 */
int process_manager_broadcast_message(uint32_t msg_type, void *msg_data, size_t msg_size) {
    if (!g_process_manager_init) {
        LOG_ERROR("Process manager not initialized");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *current = g_process_list;
    int failed = 0;
    
    while (current) {
        if (current->status == PROCESS_STATUS_RUNNING && current->msg_pipe[1] != -1) {
            // 构建消息
            ProcessMsg_t msg;
            msg.msg_type = msg_type;
            msg.msg_size = msg_size;
            msg.msg_data = msg_data;
            
            // 发送消息
            if (write(current->msg_pipe[1], &msg, sizeof(ProcessMsg_t)) != sizeof(ProcessMsg_t)) {
                LOG_ERROR("Failed to send broadcast message to process: %s", current->name);
                failed++;
                current = current->next;
                continue;
            }
            
            // 如果有消息数据，发送数据
            if (msg_data && msg_size > 0) {
                if (write(current->msg_pipe[1], msg_data, msg_size) != (ssize_t)msg_size) {
                    LOG_ERROR("Failed to send broadcast message data to process: %s", current->name);
                    failed++;
                    current = current->next;
                    continue;
                }
            }
            
            LOG_DEBUG("Sent broadcast message type %d to process: %s", msg_type, current->name);
        }
        current = current->next;
    }
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    
    if (failed > 0) {
        LOG_ERROR("Failed to send broadcast message to %d processes", failed);
        return -1;
    }
    
    return 0;
}

/**
 * @brief 设置进程资源限制
 * @param name 进程名称
 * @param limits 资源限制
 * @return 设置结果：0表示成功，非0表示失败
 */
int process_manager_set_process_resource_limits(const char *name, ProcessResourceLimits_t *limits) {
    if (!g_process_manager_init || !name || !limits) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    if (!process) {
        LOG_ERROR("Process not found: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    // 设置资源限制
    process->limits = *limits;
    LOG_INFO("Set resource limits for process %s:", name);
    LOG_INFO("  Max CPU usage: %d%%", limits->max_cpu_usage);
    LOG_INFO("  Max memory: %d MB", limits->max_memory);
    LOG_INFO("  Max file descriptors: %d", limits->max_file_descriptors);
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    return 0;
}

/**
 * @brief 获取进程资源使用情况
 * @param name 进程名称
 * @param cpu_usage CPU使用率（输出）
 * @param memory_usage 内存使用量（输出，MB）
 * @return 获取结果：0表示成功，非0表示失败
 */
int process_manager_get_process_resource_usage(const char *name, int *cpu_usage, int *memory_usage) {
    if (!g_process_manager_init || !name) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    if (!process || process->status != PROCESS_STATUS_RUNNING) {
        LOG_ERROR("Process not found or not running: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
    // 这里只是一个示例实现，实际应该根据操作系统获取真实的资源使用情况
    if (cpu_usage) {
        *cpu_usage = 0; // 示例值
    }
    if (memory_usage) {
        *memory_usage = 0; // 示例值
    }
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    return 0;
}

/**
 * @brief 终止进程（发送信号）
 * @param name 进程名称
 * @param signal 信号值
 * @return 终止结果：0表示成功，非0表示失败
 */
int process_manager_kill_process(const char *name, int signal) {
    if (!g_process_manager_init || !name) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_process_list_mutex);
    
    ProcessInfo_t *process = find_process(name);
    if (!process || process->status != PROCESS_STATUS_RUNNING) {
        LOG_ERROR("Process not found or not running: %s", name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
    
#ifdef _WIN32
    // Windows平台：使用TerminateThread终止线程
    HANDLE hThread = OpenThread(THREAD_TERMINATE, FALSE, (DWORD)process->pid);
    if (hThread != NULL) {
        if (!TerminateThread(hThread, signal)) {
            CloseHandle(hThread);
            LOG_ERROR("Failed to terminate process (thread) on Windows");
            MUTEX_LOCK_UNLOCK(g_process_list_mutex);
            return -1;
        }
        CloseHandle(hThread);
    }
#else
    // Unix平台：使用kill发送信号
    if (kill(process->pid, signal) == -1) {
        LOG_ERROR("Failed to send signal %d to process: %s", signal, name);
        MUTEX_LOCK_UNLOCK(g_process_list_mutex);
        return -1;
    }
#endif
    
    LOG_INFO("Sent signal %d to process: %s", signal, name);
    
    MUTEX_LOCK_UNLOCK(g_process_list_mutex);
    return 0;
}
