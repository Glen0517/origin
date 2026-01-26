#include "audio_source_priv.h"
#include "logger.h"
#include "audio_core.h"
#include "bt.h"
#include "peripheral.h"
#include "peripheral_priv.h"
#include "event.h"
#include <pthread.h>

#include <aml_a2dp.h>       // 晶晨A2DP SDK

// 蓝牙事件类型定义
typedef enum {
    BT_EVENT_DEVICE_CONNECT,     // 蓝牙设备连接
    BT_EVENT_DEVICE_DISCONNECT,  // 蓝牙设备断开
    BT_EVENT_PLAY,               // 播放
    BT_EVENT_PAUSE,              // 暂停
    BT_EVENT_STOP,               // 停止
    BT_EVENT_ERROR,              // 错误
    BT_EVENT_VOLUME_CHANGED,     // 音量变化
    BT_EVENT_AUDIO_PARAMS,       // 音频参数变化
    BT_EVENT_DATA_RECEIVED,      // 音频数据接收
    BT_EVENT_STOP_THREAD         // 停止蓝牙线程
} BtEventType_t;

// 蓝牙事件结构体
typedef struct {
    BtEventType_t type;          // 事件类型
    union {
        struct {
            char device_name[64]; // 设备名称
            char device_mac[18];  // 设备MAC地址
        } device_event;
        struct {
            uint8_t *data;        // 音频数据指针
            int data_len;         // 音频数据长度
        } data_event;
        struct {
            int volume;           // 音量值
        } volume_event;
        struct {
            int sample_rate;      // 采样率
            int channels;         // 声道数
            int bit_depth;        // 比特深度
        } audio_params_event;
    } data;
} BtEvent_t;

// 蓝牙事件队列配置
#define BT_EVENT_QUEUE_SIZE 100

// 蓝牙线程相关变量
static pthread_t g_bt_thread = 0;          // 蓝牙处理线程ID
static pthread_mutex_t g_bt_event_mutex = PTHREAD_MUTEX_INITIALIZER;  // 事件队列互斥锁
static pthread_cond_t g_bt_event_cond = PTHREAD_COND_INITIALIZER;     // 事件队列条件变量
static BtEvent_t g_bt_event_queue[BT_EVENT_QUEUE_SIZE];  // 事件队列
static int g_bt_event_head = 0;           // 事件队列头索引
static int g_bt_event_tail = 0;           // 事件队列尾索引
static bool g_bt_thread_running = false;  // 蓝牙线程运行标志

// 蓝牙状态变量
static bool g_bt_src_init = false;
static bool g_bt_connected = false;
static bool g_bt_playing = false;
static int g_bt_volume = 80;
static char g_bt_device_name[64] = {0};
static char g_bt_device_mac[18] = {0}; // XX:XX:XX:XX:XX:XX
static int g_bt_audio_state = 0;
static pthread_mutex_t g_bt_state_mutex = PTHREAD_MUTEX_INITIALIZER;  // 状态互斥锁

/**
 * @brief 将事件添加到蓝牙事件队列
 * @param event 要添加的蓝牙事件
 * @return 添加结果：0表示成功，非0表示失败
 */
static int bt_event_add(const BtEvent_t *event) {
    if (!event) {
        return -1;
    }
    
    pthread_mutex_lock(&g_bt_event_mutex);
    
    // 检查事件队列是否已满
    int next_tail = (g_bt_event_tail + 1) % BT_EVENT_QUEUE_SIZE;
    if (next_tail == g_bt_event_head) {
        pthread_mutex_unlock(&g_bt_event_mutex);
        LOG_ERROR("BT event queue full");
        return -1;
    }
    
    // 添加事件到队列
    g_bt_event_queue[g_bt_event_tail] = *event;
    g_bt_event_tail = next_tail;
    
    // 通知等待的线程
    pthread_cond_signal(&g_bt_event_cond);
    
    pthread_mutex_unlock(&g_bt_event_mutex);
    return 0;
}

/**
 * @brief 从蓝牙事件队列获取事件（阻塞）
 * @param event 用于存储获取到的事件
 * @return 获取结果：0表示成功，非0表示失败
 */
static int bt_event_get(BtEvent_t *event) {
    if (!event) {
        return -1;
    }
    
    pthread_mutex_lock(&g_bt_event_mutex);
    
    // 等待事件到来
    while (g_bt_event_head == g_bt_event_tail && g_bt_thread_running) {
        pthread_cond_wait(&g_bt_event_cond, &g_bt_event_mutex);
    }
    
    if (!g_bt_thread_running) {
        pthread_mutex_unlock(&g_bt_event_mutex);
        return -1;
    }
    
    // 获取事件
    *event = g_bt_event_queue[g_bt_event_head];
    g_bt_event_head = (g_bt_event_head + 1) % BT_EVENT_QUEUE_SIZE;
    
    pthread_mutex_unlock(&g_bt_event_mutex);
    return 0;
}

/**
 * @brief 蓝牙处理线程函数
 * @param arg 线程参数
 * @return 线程返回值
 */
static void *bt_process_thread(void *arg) {
    LOG_INFO("Bluetooth process thread started");
    
    BtEvent_t event;
    
    while (g_bt_thread_running) {
        // 从队列获取事件
        if (bt_event_get(&event) != 0) {
            continue;
        }
        
        // 处理不同类型的事件
        switch (event.type) {
            case BT_EVENT_DEVICE_CONNECT: {
                LOG_INFO("Processing BT device connect event: %s (%s)", 
                         event.data.device_event.device_name, 
                         event.data.device_event.device_mac);
                
                // 更新蓝牙连接状态
                pthread_mutex_lock(&g_bt_state_mutex);
                g_bt_connected = true;
                g_bt_playing = false;
                strncpy(g_bt_device_name, event.data.device_event.device_name, sizeof(g_bt_device_name) - 1);
                strncpy(g_bt_device_mac, event.data.device_event.device_mac, sizeof(g_bt_device_mac) - 1);
                pthread_mutex_unlock(&g_bt_state_mutex);
                
                // 更新LED状态
                led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_ON);
                
                // 更新LCD显示
                lcd_display_text("BT Connected", LCD_LINE_1);
                lcd_display_text(event.data.device_event.device_name, LCD_LINE_2);
                
                // 发送蓝牙连接事件
                event_notify(EVENT_BT_CONNECTED, (void *)event.data.device_event.device_name);
                break;
            }
            
            case BT_EVENT_DEVICE_DISCONNECT: {
                LOG_INFO("Processing BT device disconnect event");
                
                // 获取设备信息（用于日志）
                char device_name[64] = {0};
                char device_mac[18] = {0};
                pthread_mutex_lock(&g_bt_state_mutex);
                strncpy(device_name, g_bt_device_name, sizeof(device_name) - 1);
                strncpy(device_mac, g_bt_device_mac, sizeof(device_mac) - 1);
                g_bt_connected = false;
                g_bt_playing = false;
                pthread_mutex_unlock(&g_bt_state_mutex);
                
                LOG_INFO("Bluetooth source: disconnected from %s (%s)", device_name, device_mac);
                
                // 更新LED状态
                led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_OFF);
                
                // 更新LCD显示
                lcd_display_text("BT Disconnected", LCD_LINE_1);
                lcd_display_text("", LCD_LINE_2);
                
                // 发送蓝牙断开事件
                event_notify(EVENT_BT_DISCONNECTED, NULL);
                
                // 清空设备信息
                pthread_mutex_lock(&g_bt_state_mutex);
                g_bt_device_name[0] = '\0';
                g_bt_device_mac[0] = '\0';
                pthread_mutex_unlock(&g_bt_state_mutex);
                break;
            }
            
            case BT_EVENT_PLAY: {
                LOG_INFO("Processing BT play event");
                
                // 更新播放状态
                pthread_mutex_lock(&g_bt_state_mutex);
                g_bt_playing = true;
                pthread_mutex_unlock(&g_bt_state_mutex);
                
                // 更新LED状态
                led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_BLINK_SLOW);
                
                // 更新LCD显示
                char lcd_msg[32] = {0};
                snprintf(lcd_msg, sizeof(lcd_msg), "BT Playing");
                lcd_display_text(lcd_msg, LCD_LINE_1);
                
                // 发送蓝牙播放开始事件
                event_notify(EVENT_BT_PLAY_START, NULL);
                break;
            }
            
            case BT_EVENT_PAUSE: {
                LOG_INFO("Processing BT pause event");
                
                // 更新播放状态
                pthread_mutex_lock(&g_bt_state_mutex);
                g_bt_playing = false;
                pthread_mutex_unlock(&g_bt_state_mutex);
                
                // 更新LED状态
                led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_ON);
                
                // 更新LCD显示
                char lcd_msg[32] = {0};
                snprintf(lcd_msg, sizeof(lcd_msg), "BT Paused");
                lcd_display_text(lcd_msg, LCD_LINE_1);
                
                // 发送蓝牙播放暂停事件
                event_notify(EVENT_BT_PLAY_PAUSE, NULL);
                break;
            }
            
            case BT_EVENT_STOP: {
                LOG_INFO("Processing BT stop event");
                
                // 更新播放状态
                pthread_mutex_lock(&g_bt_state_mutex);
                g_bt_playing = false;
                pthread_mutex_unlock(&g_bt_state_mutex);
                
                // 更新LED状态
                led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_ON);
                
                // 更新LCD显示
                char lcd_msg[32] = {0};
                snprintf(lcd_msg, sizeof(lcd_msg), "BT Stopped");
                lcd_display_text(lcd_msg, LCD_LINE_1);
                
                // 发送蓝牙播放停止事件
                event_notify(EVENT_BT_PLAY_STOP, NULL);
                break;
            }
            
            case BT_EVENT_ERROR: {
                LOG_INFO("Processing BT error event");
                
                // 更新播放状态
                pthread_mutex_lock(&g_bt_state_mutex);
                g_bt_playing = false;
                pthread_mutex_unlock(&g_bt_state_mutex);
                
                // 更新LED状态
                led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_BLINK_FAST);
                
                // 更新LCD显示
                char lcd_msg[32] = {0};
                snprintf(lcd_msg, sizeof(lcd_msg), "BT Error");
                lcd_display_text(lcd_msg, LCD_LINE_1);
                
                // 发送蓝牙错误事件
                event_notify(EVENT_BT_ERROR, NULL);
                break;
            }
            
            case BT_EVENT_VOLUME_CHANGED: {
                LOG_INFO("Processing BT volume changed event: %d", event.data.volume_event.volume);
                
                // 更新音量状态
                pthread_mutex_lock(&g_bt_state_mutex);
                g_bt_volume = event.data.volume_event.volume;
                pthread_mutex_unlock(&g_bt_state_mutex);
                
                // 发送音量变化事件
                event_notify(EVENT_BT_VOLUME_CHANGED, &event.data.volume_event.volume);
                break;
            }
            
            case BT_EVENT_AUDIO_PARAMS: {
                LOG_INFO("Processing BT audio params event: %d Hz, %d channels, %d bits", 
                         event.data.audio_params_event.sample_rate, 
                         event.data.audio_params_event.channels, 
                         event.data.audio_params_event.bit_depth);
                
                // 可以根据需要调整音频核心参数
                break;
            }
            
            case BT_EVENT_DATA_RECEIVED: {
                // 处理蓝牙音频数据
                if (event.data.data_event.data && event.data.data_event.data_len > 0) {
                    // 检查数据长度是否合理
                    if (event.data.data_event.data_len > 0 && event.data.data_event.data_len < 8192) { // 限制最大数据长度为8KB
                        // 将接收到的音频数据发送到音频核心
                        if (audio_core_play_pcm(event.data.data_event.data, event.data.data_event.data_len) != 0) {
                            LOG_WARN("Failed to send BT audio data to audio core");
                        }
                    } else {
                        LOG_WARN("Invalid BT audio data length: %d", event.data.data_event.data_len);
                    }
                }
                break;
            }
            
            case BT_EVENT_STOP_THREAD: {
                LOG_INFO("Stopping BT process thread");
                g_bt_thread_running = false;
                break;
            }
            
            default:
                LOG_ERROR("Unknown BT event type: %d", event.type);
                break;
        }
    }
    
    LOG_INFO("Bluetooth process thread exited");
    return NULL;
}

/**
 * @brief 蓝牙A2DP音频接收回调函数
 */
static void bt_a2dp_audio_callback(uint8_t *pcm_data, int data_len) {
    if (g_bt_src_init && g_bt_connected && pcm_data && data_len > 0) {
        // 创建音频数据事件
        BtEvent_t event;
        event.type = BT_EVENT_DATA_RECEIVED;
        event.data.data_event.data = pcm_data;
        event.data.data_event.data_len = data_len;
        
        // 将事件添加到队列
        if (bt_event_add(&event) != 0) {
            LOG_ERROR("Failed to add BT data event to queue");
        }
    } else if (!pcm_data) {
        LOG_ERROR("NULL pointer received in BT audio callback");
    } else if (data_len <= 0) {
        LOG_WARN("Zero or negative data length in BT audio callback: %d", data_len);
    }
}

/**
 * @brief 蓝牙连接状态回调函数
 */
static void bt_a2dp_status_callback(int status) {
    BtEvent_t event;
    
    switch (status) {
        case A2DP_STATUS_CONNECTED:
            // 获取连接的设备信息
            char device_name[64] = {0};
            char device_mac[18] = {0};
            aml_a2dp_get_device_name(device_name, sizeof(device_name));
            aml_a2dp_get_device_mac(device_mac, sizeof(device_mac));
            
            // 创建设备连接事件
            event.type = BT_EVENT_DEVICE_CONNECT;
            strncpy(event.data.device_event.device_name, device_name, sizeof(event.data.device_event.device_name) - 1);
            strncpy(event.data.device_event.device_mac, device_mac, sizeof(event.data.device_event.device_mac) - 1);
            break;
            
        case A2DP_STATUS_DISCONNECTED:
            // 创建设备断开事件
            event.type = BT_EVENT_DEVICE_DISCONNECT;
            break;
            
        case A2DP_STATUS_PLAYING:
            // 创建播放事件
            event.type = BT_EVENT_PLAY;
            break;
            
        case A2DP_STATUS_PAUSED:
            // 创建暂停事件
            event.type = BT_EVENT_PAUSE;
            break;
            
        case A2DP_STATUS_STOPPED:
            // 创建停止事件
            event.type = BT_EVENT_STOP;
            break;
            
        case A2DP_STATUS_ERROR:
            // 创建错误事件
            event.type = BT_EVENT_ERROR;
            break;
            
        default:
            LOG_INFO("Bluetooth source: unknown status %d", status);
            return;
    }
    
    // 将事件添加到队列
    if (bt_event_add(&event) != 0) {
        LOG_ERROR("Failed to add BT status event to queue");
    }
    
    g_bt_audio_state = status;
}

/**
 * @brief 蓝牙A2DP音量回调函数
 */
static void bt_a2dp_volume_callback(int volume) {
    // 创建音量变化事件
    BtEvent_t event;
    event.type = BT_EVENT_VOLUME_CHANGED;
    event.data.volume_event.volume = volume;
    
    // 将事件添加到队列
    if (bt_event_add(&event) != 0) {
        LOG_ERROR("Failed to add BT volume event to queue");
    }
}

/**
 * @brief 蓝牙A2DP音频参数回调函数
 */
static void bt_a2dp_audio_params_callback(int sample_rate, int channels, int bit_depth) {
    // 创建音频参数变化事件
    BtEvent_t event;
    event.type = BT_EVENT_AUDIO_PARAMS;
    event.data.audio_params_event.sample_rate = sample_rate;
    event.data.audio_params_event.channels = channels;
    event.data.audio_params_event.bit_depth = bit_depth;
    
    // 将事件添加到队列
    if (bt_event_add(&event) != 0) {
        LOG_ERROR("Failed to add BT audio params event to queue");
    }
}

int src_bt_init(void) {
    if (g_bt_src_init) {
        LOG_INFO("Bluetooth source already initialized");
        return 0;
    }
    
    // 初始化Amlogic A2DP
    if (aml_a2dp_init() != 0) {
        LOG_ERROR("Bluetooth source init failed: A2DP init error");
        return -1;
    }
    
    // 设置A2DP回调函数
    aml_a2dp_set_audio_callback(bt_a2dp_audio_callback);
    aml_a2dp_set_status_callback(bt_a2dp_status_callback);
    aml_a2dp_set_volume_callback(bt_a2dp_volume_callback);
    aml_a2dp_set_audio_params_callback(bt_a2dp_audio_params_callback);
    
    // 配置A2DP参数
    aml_a2dp_set_sample_rate(44100);     // 默认采样率44.1kHz
    aml_a2dp_set_buffer_size(4096);      // 设置缓冲区大小4KB
    aml_a2dp_set_buffer_count(4);        // 设置缓冲区数量为4个
    
    // 设置初始音量
    aml_a2dp_set_volume(g_bt_volume);
    
    // 启用A2DP
    aml_a2dp_enable(true);
    
    // 启动蓝牙事件处理线程
    g_bt_thread_running = true;
    if (pthread_create(&g_bt_thread, NULL, bt_process_thread, NULL) != 0) {
        LOG_ERROR("Failed to create Bluetooth process thread");
        aml_a2dp_enable(false);
        aml_a2dp_deinit();
        return -1;
    }
    
    g_bt_src_init = true;
    g_bt_connected = false;
    g_bt_playing = false;
    g_bt_audio_state = A2DP_STATUS_IDLE;
    g_bt_device_name[0] = '\0';
    g_bt_device_mac[0] = '\0';
    
    LOG_INFO("Bluetooth (A2DP) source init success");
    LOG_INFO("  Initial volume: %d", g_bt_volume);
    LOG_INFO("  Default sample rate: 44.1kHz");
    LOG_INFO("  Buffer size: 4KB x 4 buffers");
    LOG_INFO("  Bluetooth process thread started");
    
    return 0;
}

void src_bt_deinit(void) {
    if (g_bt_src_init) {
        // 禁用A2DP
        aml_a2dp_enable(false);
        
        // 停止当前播放
        aml_a2dp_stop();
        
        // 停止蓝牙处理线程
        if (g_bt_thread_running) {
            g_bt_thread_running = false;
            
            // 添加停止线程事件
            BtEvent_t event;
            event.type = BT_EVENT_STOP_THREAD;
            bt_event_add(&event);
            
            // 等待线程退出
            if (g_bt_thread != 0) {
                pthread_join(g_bt_thread, NULL);
                g_bt_thread = 0;
            }
        }
        
        // 反初始化Amlogic A2DP
        aml_a2dp_deinit();
        
        // 更新LED状态
        led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_OFF);
        
        g_bt_src_init = false;
        g_bt_connected = false;
        g_bt_playing = false;
        g_bt_audio_state = A2DP_STATUS_IDLE;
        g_bt_device_name[0] = '\0';
        g_bt_device_mac[0] = '\0';
        
        LOG_INFO("Bluetooth (A2DP) source deinit success");
        LOG_INFO("  Bluetooth process thread stopped");
    }
}

/**
 * @brief 获取蓝牙连接状态
 */
bool src_bt_get_connect_state(void) {
    return g_bt_connected;
}

/**
 * @brief 获取当前播放状态
 */
bool src_bt_is_playing(void) {
    return g_bt_playing;
}

/**
 * @brief 设置蓝牙播放音量
 */
int src_bt_set_volume(int volume) {
    if (!g_bt_src_init) {
        return -1;
    }
    
    if (volume >= 0 && volume <= 100) {
        g_bt_volume = volume;
        aml_a2dp_set_volume(volume);
        LOG_INFO("Bluetooth source volume set to %d", volume);
        return 0;
    } else {
        LOG_ERROR("Invalid volume value: %d", volume);
        return -1;
    }
}

/**
 * @brief 获取当前蓝牙音量
 */
int src_bt_get_volume(void) {
    return g_bt_volume;
}

/**
 * @brief 暂停蓝牙播放
 */
int src_bt_pause(void) {
    if (!g_bt_src_init || !g_bt_connected) {
        return -1;
    }
    
    return aml_a2dp_pause();
}

/**
 * @brief 继续蓝牙播放
 */
int src_bt_resume(void) {
    if (!g_bt_src_init || !g_bt_connected) {
        return -1;
    }
    
    return aml_a2dp_play();
}

/**
 * @brief 停止蓝牙播放
 */
int src_bt_stop(void) {
    if (!g_bt_src_init) {
        return -1;
    }
    
    return aml_a2dp_stop();
}

/**
 * @brief 获取当前连接的蓝牙设备名称
 */
const char *src_bt_get_device_name(void) {
    return g_bt_connected ? g_bt_device_name : NULL;
}

/**
 * @brief 获取当前连接的蓝牙设备MAC地址
 */
const char *src_bt_get_device_mac(void) {
    return g_bt_connected ? g_bt_device_mac : NULL;
}

/**
 * @brief 重启蓝牙A2DP服务
 */
int src_bt_restart(void) {
    if (!g_bt_src_init) {
        return -1;
    }
    
    // 先停止服务
    aml_a2dp_enable(false);
    aml_a2dp_stop();
    
    // 重新启用服务
    aml_a2dp_enable(true);
    
    LOG_INFO("Bluetooth A2DP service restarted");
    return 0;
}