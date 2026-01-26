#include "audio_source_priv.h"
#include "logger.h"
#include "audio_core.h"
#include "storage.h"
#include "hal.h"  // 硬件抽象层
#include <pthread.h>

// 存储进程管理函数声明
int storage_process_mount_device(const char *dev_path);
int storage_process_umount_device(void);

// USB事件类型定义
typedef enum {
    USB_EVENT_DEVICE_CONNECT,     // USB设备连接
    USB_EVENT_DEVICE_DISCONNECT,  // USB设备断开
    USB_EVENT_DATA_RECEIVED,      // USB音频数据接收
    USB_EVENT_STOP_THREAD         // 停止USB线程
} UsbEventType_t;

// USB事件结构体
typedef struct {
    UsbEventType_t type;          // 事件类型
    union {
        struct {
            char dev_path[64];    // USB设备路径
        } device_event;
        struct {
            uint8_t *data;        // 音频数据指针
            int data_len;         // 音频数据长度
        } data_event;
    } data;
} UsbEvent_t;

// USB事件队列配置
#define USB_EVENT_QUEUE_SIZE 100

// USB线程相关变量
static pthread_t g_usb_thread = 0;          // USB处理线程ID
static pthread_mutex_t g_usb_event_mutex = PTHREAD_MUTEX_INITIALIZER;  // 事件队列互斥锁
static pthread_cond_t g_usb_event_cond = PTHREAD_COND_INITIALIZER;     // 事件队列条件变量
static UsbEvent_t g_usb_event_queue[USB_EVENT_QUEUE_SIZE];  // 事件队列
static int g_usb_event_head = 0;           // 事件队列头索引
static int g_usb_event_tail = 0;           // 事件队列尾索引
static bool g_usb_thread_running = false;  // USB线程运行标志

// USB状态变量
static bool g_usb_src_init = false;         // USB音频源初始化标志
static bool g_usb_connected = false;        // USB设备连接状态
static char g_usb_device_path[64] = {0};    // USB设备路径
static pthread_mutex_t g_usb_state_mutex = PTHREAD_MUTEX_INITIALIZER;  // 状态互斥锁

/**
 * @brief 获取USB音频源状态
 * @details 检查USB设备是否连接
 * @return 连接状态：1-已连接，0-未连接
 */
int src_usb_get_status(void) {
    bool connected = false;
    
    // 线程安全地获取USB连接状态
    pthread_mutex_lock(&g_usb_state_mutex);
    connected = g_usb_connected;
    pthread_mutex_unlock(&g_usb_state_mutex);
    
    return g_usb_src_init && connected ? 1 : 0;
}

/**
 * @brief 将事件添加到USB事件队列
 * @param event 要添加的USB事件
 * @return 添加结果：0表示成功，非0表示失败
 */
static int usb_event_add(const UsbEvent_t *event) {
    if (!event) {
        return -1;
    }
    
    pthread_mutex_lock(&g_usb_event_mutex);
    
    // 检查事件队列是否已满
    int next_tail = (g_usb_event_tail + 1) % USB_EVENT_QUEUE_SIZE;
    if (next_tail == g_usb_event_head) {
        pthread_mutex_unlock(&g_usb_event_mutex);
        LOG_ERROR("USB event queue full");
        return -1;
    }
    
    // 添加事件到队列
    g_usb_event_queue[g_usb_event_tail] = *event;
    g_usb_event_tail = next_tail;
    
    // 通知等待的线程
    pthread_cond_signal(&g_usb_event_cond);
    
    pthread_mutex_unlock(&g_usb_event_mutex);
    return 0;
}

/**
 * @brief 从USB事件队列获取事件（阻塞）
 * @param event 用于存储获取到的事件
 * @return 获取结果：0表示成功，非0表示失败
 */
static int usb_event_get(UsbEvent_t *event) {
    if (!event) {
        return -1;
    }
    
    pthread_mutex_lock(&g_usb_event_mutex);
    
    // 等待事件到来
    while (g_usb_event_head == g_usb_event_tail && g_usb_thread_running) {
        pthread_cond_wait(&g_usb_event_cond, &g_usb_event_mutex);
    }
    
    if (!g_usb_thread_running) {
        pthread_mutex_unlock(&g_usb_event_mutex);
        return -1;
    }
    
    // 获取事件
    *event = g_usb_event_queue[g_usb_event_head];
    g_usb_event_head = (g_usb_event_head + 1) % USB_EVENT_QUEUE_SIZE;
    
    pthread_mutex_unlock(&g_usb_event_mutex);
    return 0;
}

/**
 * @brief USB处理线程函数
 * @param arg 线程参数
 * @return 线程返回值
 */
static void *usb_process_thread(void *arg) {
    LOG_INFO("USB process thread started");
    
    UsbEvent_t event;
    
    while (g_usb_thread_running) {
        // 从队列获取事件
        if (usb_event_get(&event) != 0) {
            continue;
        }
        
        // 处理不同类型的事件
        switch (event.type) {
            case USB_EVENT_DEVICE_CONNECT: {
                LOG_INFO("Processing USB device connect event: %s", event.data.device_event.dev_path);
                
                // 更新USB连接状态
                pthread_mutex_lock(&g_usb_state_mutex);
                g_usb_connected = true;
                strncpy(g_usb_device_path, event.data.device_event.dev_path, sizeof(g_usb_device_path) - 1);
                pthread_mutex_unlock(&g_usb_state_mutex);
                
                // 打开USB音频设备（通过HAL层）
                if (hal_usb_audio_open(event.data.device_event.dev_path) != 0) {
                    LOG_ERROR("Failed to open USB audio device: %s", event.data.device_event.dev_path);
                }
                
                // 挂载USB存储设备
                if (storage_process_mount_device(event.data.device_event.dev_path) != 0) {
                    LOG_ERROR("Failed to mount USB storage device: %s", event.data.device_event.dev_path);
                }
                
                break;
            }
            
            case USB_EVENT_DEVICE_DISCONNECT: {
                LOG_INFO("Processing USB device disconnect event");
                
                // 获取设备路径（用于日志）
                char device_path[64] = {0};
                pthread_mutex_lock(&g_usb_state_mutex);
                strncpy(device_path, g_usb_device_path, sizeof(device_path) - 1);
                pthread_mutex_unlock(&g_usb_state_mutex);
                
                LOG_INFO("USB audio device disconnected: %s", device_path);
                
                // 关闭USB音频设备（通过HAL层）
                if (hal_usb_audio_close() != 0) {
                    LOG_ERROR("Failed to close USB audio device");
                }
                
                // 卸载USB存储设备
                if (storage_process_umount_device() != 0) {
                    LOG_ERROR("Failed to unmount USB storage device");
                }
                
                // 更新USB连接状态
                pthread_mutex_lock(&g_usb_state_mutex);
                g_usb_connected = false;
                g_usb_device_path[0] = '\0';
                pthread_mutex_unlock(&g_usb_state_mutex);
                
                break;
            }
            
            case USB_EVENT_DATA_RECEIVED: {
                // 处理USB音频数据
                if (event.data.data_event.data && event.data.data_event.data_len > 0) {
                    // 将接收到的音频数据发送到音频核心
                    audio_core_play_pcm(event.data.data_event.data, event.data.data_event.data_len);
                }
                break;
            }
            
            case USB_EVENT_STOP_THREAD: {
                LOG_INFO("Stopping USB process thread");
                g_usb_thread_running = false;
                break;
            }
            
            default:
                LOG_ERROR("Unknown USB event type: %d", event.type);
                break;
        }
    }
    
    LOG_INFO("USB process thread exited");
    return NULL;
}

/**
 * @brief USB音频设备连接状态回调函数
 */
static void usb_audio_device_callback(const char *dev_path, bool connected) {
    if (g_usb_src_init) {
        UsbEvent_t event;
        
        if (connected) {
            // 创建设备连接事件
            event.type = USB_EVENT_DEVICE_CONNECT;
            strncpy(event.data.device_event.dev_path, dev_path, sizeof(event.data.device_event.dev_path) - 1);
        } else {
            // 创建设备断开事件
            event.type = USB_EVENT_DEVICE_DISCONNECT;
        }
        
        // 将事件添加到队列
        if (usb_event_add(&event) != 0) {
            LOG_ERROR("Failed to add USB device event to queue");
        }
    }
}

/**
 * @brief USB音频数据接收回调函数
 */
static void usb_audio_data_callback(uint8_t *pcm_data, int data_len) {
    bool connected = false;
    
    // 线程安全地获取USB连接状态
    pthread_mutex_lock(&g_usb_state_mutex);
    connected = g_usb_connected;
    pthread_mutex_unlock(&g_usb_state_mutex);
    
    if (g_usb_src_init && connected && pcm_data && data_len > 0) {
        // 创建音频数据事件
        UsbEvent_t event;
        event.type = USB_EVENT_DATA_RECEIVED;
        event.data.data_event.data = pcm_data;
        event.data.data_event.data_len = data_len;
        
        // 将事件添加到队列
        if (usb_event_add(&event) != 0) {
            LOG_ERROR("Failed to add USB data event to queue");
        }
    }
}

int src_usb_init(void) {
    if (g_usb_src_init) {
        return 0;
    }
    
    // 初始化USB硬件（通过HAL层）
    if (hal_usb_init() != 0) {
        LOG_ERROR("USB source init failed: HAL USB init error");
        return FAILURE;
    }
    
    // 设置USB音频设备连接状态回调（通过HAL层）
    if (hal_usb_audio_set_device_callback(usb_audio_device_callback) != 0) {
        LOG_ERROR("Failed to set USB device callback");
        hal_usb_deinit();
        return FAILURE;
    }
    
    // 设置USB音频数据接收回调（通过HAL层）
    if (hal_usb_audio_set_data_callback(usb_audio_data_callback) != 0) {
        LOG_ERROR("Failed to set USB data callback");
        hal_usb_deinit();
        return FAILURE;
    }
    
    // 启用USB音频设备检测（通过HAL层）
    if (hal_usb_audio_enable_detection(true) != 0) {
        LOG_ERROR("Failed to enable USB detection");
        hal_usb_deinit();
        return FAILURE;
    }
    
    // 创建并启动USB处理线程
    g_usb_thread_running = true;
    if (pthread_create(&g_usb_thread, NULL, usb_process_thread, NULL) != 0) {
        LOG_ERROR("Failed to create USB process thread");
        hal_usb_deinit();
        return FAILURE;
    }
    
    g_usb_src_init = true;
    g_usb_connected = false;
    g_usb_device_path[0] = '\0';
    
    LOG_INFO("USB local source init success");
    LOG_INFO("  Device detection: ENABLED");
    LOG_INFO("  Process thread: RUNNING");
    
    return 0;
}

void src_usb_deinit(void) {
    if (g_usb_src_init) {
        // 禁用USB音频设备检测（通过HAL层）
        if (hal_usb_audio_enable_detection(false) != 0) {
            LOG_ERROR("Failed to disable USB detection");
        }
        
        // 停止USB处理线程
        if (g_usb_thread_running) {
            LOG_INFO("Stopping USB process thread...");
            
            // 发送停止线程事件
            UsbEvent_t event;
            event.type = USB_EVENT_STOP_THREAD;
            usb_event_add(&event);
            
            // 等待线程退出
            pthread_join(g_usb_thread, NULL);
            g_usb_thread_running = false;
        }
        
        // 关闭当前打开的USB音频设备（通过HAL层）
        bool connected = false;
        pthread_mutex_lock(&g_usb_state_mutex);
        connected = g_usb_connected;
        pthread_mutex_unlock(&g_usb_state_mutex);
        
        if (connected) {
            if (hal_usb_audio_close() != 0) {
                LOG_ERROR("Failed to close USB audio device");
            }
            // 确保设备被卸载
            if (storage_process_umount_device() != 0) {
                LOG_ERROR("Failed to unmount USB storage device during deinit");
            }
        }
        
        // 反初始化USB硬件（通过HAL层）
        if (hal_usb_deinit() != 0) {
            LOG_ERROR("Failed to deinit USB hardware");
        }
        
        g_usb_src_init = false;
        g_usb_connected = false;
        g_usb_device_path[0] = '\0';
        
        LOG_INFO("USB local source deinit success");
    }
}