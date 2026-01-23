#include "audio_source_priv.h"
#include "logger.h"
#include "audio_core.h"
#include "product_type.h"
#include "peripheral.h"
#include "peripheral_priv.h"
#include "event.h"

#include <aml_uac.h>         // 晶晨UAC SDK

/**
 * @brief UAC状态和配置结构体
 * @details 封装UAC相关的状态和配置信息
 */
typedef struct {
    bool init;              // 初始化状态
    bool connected;         // 设备连接状态
    char device_path[64];   // 设备路径
    int sample_rate;        // 采样率
    int channels;           // 通道数
    int bit_depth;          // 位深度
    char device_name[128];  // 设备名称
    int buffer_size;        // 缓冲区大小
    int buffer_count;       // 缓冲区数量
} UacState_t;

// UAC状态和配置实例
static UacState_t g_uac_state = {
    .init = false,
    .connected = false,
    .device_path = {0},
    .sample_rate = 48000,
    .channels = 2,
    .bit_depth = 16,
    .device_name = {0},
    .buffer_size = 4096,
    .buffer_count = 4
};

/**
 * @brief UAC设备信息获取函数
 */
static void uac_get_device_info(const char *dev_path) {
    if (!dev_path || strlen(dev_path) == 0) {
        LOG_ERROR("Invalid UAC device path");
        return;
    }
    
    // 获取UAC设备名称
    if (aml_uac_get_device_name(dev_path, g_uac_state.device_name, sizeof(g_uac_state.device_name)) == 0) {
        LOG_INFO("UAC device name: %s", g_uac_state.device_name);
    } else {
        strncpy(g_uac_state.device_name, "Unknown UAC Device", sizeof(g_uac_state.device_name) - 1);
        LOG_WARN("Failed to get UAC device name, using default");
    }
    
    // 获取UAC设备支持的采样率
    int sample_rates[10] = {0};
    int count = aml_uac_get_supported_sample_rates(dev_path, sample_rates, sizeof(sample_rates) / sizeof(int));
    if (count > 0) {
        LOG_INFO("UAC supported sample rates:");
        for (int i = 0; i < count; i++) {
            LOG_INFO("  - %d Hz", sample_rates[i]);
        }
    } else {
        LOG_WARN("Failed to get UAC supported sample rates");
    }
}

/**
 * @brief UAC设备连接回调函数
 */
static void uac_device_callback(const char *dev_path, bool connected) {
    if (g_uac_state.init) {
        if (connected) {
            // 设备连接处理
            strncpy(g_uac_state.device_path, dev_path, sizeof(g_uac_state.device_path) - 1);
            LOG_INFO("UAC device connected: %s", dev_path);
            
            // 获取设备信息
            uac_get_device_info(dev_path);
            
            // 打开UAC设备
            if (aml_uac_open(dev_path) == 0) {
                g_uac_state.connected = true;
                
                // 获取实际的音频参数
                g_uac_state.sample_rate = aml_uac_get_current_sample_rate();
                g_uac_state.channels = aml_uac_get_current_channels();
                g_uac_state.bit_depth = aml_uac_get_current_bit_depth();
                
                LOG_INFO("UAC device opened successfully");
                LOG_INFO("  Current sample rate: %d Hz", g_uac_state.sample_rate);
                LOG_INFO("  Current channels: %d", g_uac_state.channels);
                LOG_INFO("  Current bit depth: %d bits", g_uac_state.bit_depth);
                
                // 更新LED状态
                led_ctrl_set_state(LED_UAC, LED_STATE_ON);
                
                // 更新LCD显示
                lcd_display_text("UAC Connected", LCD_LINE_1);
                lcd_display_text(g_uac_state.device_name, LCD_LINE_2);
                
                // 发送UAC设备连接事件
                event_notify(EVENT_UAC_CONNECTED, (void *)g_uac_state.device_name);
            } else {
                LOG_ERROR("Failed to open UAC device: %s", dev_path);
                g_uac_state.device_path[0] = '\0';
            }
        } else {
            // 设备断开处理
            LOG_INFO("UAC device disconnected: %s", g_uac_state.device_path);
            
            // 关闭UAC设备
            aml_uac_close();
            
            g_uac_state.connected = false;
            g_uac_state.device_path[0] = '\0';
            g_uac_state.device_name[0] = '\0';
            
            // 更新LED状态
            led_ctrl_set_state(LED_UAC, LED_STATE_OFF);
            
            // 更新LCD显示
            lcd_display_text("UAC Disconnected", LCD_LINE_1);
            lcd_display_text("", LCD_LINE_2);
            
            // 发送UAC设备断开事件
            event_notify(EVENT_UAC_DISCONNECTED, NULL);
        }
    }
}

/**
 * @brief UAC音频数据接收回调函数
 */
static void uac_audio_data_callback(uint8_t *pcm_data, int data_len) {
    if (g_uac_state.init && g_uac_state.connected && pcm_data && data_len > 0) {
        // 检查数据长度是否合理
        if (data_len > 0 && data_len < 8192) { // 限制最大数据长度为8KB
            // 将接收到的音频数据发送到音频核心
            if (audio_core_play_pcm(pcm_data, data_len) != 0) {
                LOG_WARN("Failed to send UAC audio data to audio core");
            }
        } else {
            LOG_WARN("Invalid UAC audio data length: %d", data_len);
        }
    } else if (!pcm_data) {
        LOG_ERROR("NULL pointer received in UAC audio callback");
    } else if (data_len <= 0) {
        LOG_WARN("Zero or negative data length in UAC audio callback: %d", data_len);
    }
}

/**
 * @brief UAC设备错误回调函数
 * @param error_code 错误代码
 */
static void uac_error_callback(int error_code) {
    LOG_ERROR("UAC device error: %d", error_code);
    
    // 根据错误代码进行相应的处理
    switch (error_code) {
        case UAC_ERROR_DEVICE_BUSY:
            LOG_INFO("UAC device is busy, please try again later");
            // 尝试延迟后重新连接
            LOG_INFO("Will try to reconnect after 2 seconds");
            // 这里可以添加定时器，2秒后尝试重新连接
            break;
        case UAC_ERROR_DATA_OVERRUN:
            LOG_INFO("UAC data overrun detected, check USB connection");
            LOG_INFO("Possible causes: USB cable issue, USB port power problem, or high system load");
            // 尝试调整缓冲区大小
            aml_uac_set_buffer_size(8192); // 增加缓冲区大小
            aml_uac_set_buffer_count(8);   // 增加缓冲区数量
            LOG_INFO("Increased buffer size and count to prevent data overrun");
            break;
        case UAC_ERROR_FORMAT_NOT_SUPPORTED:
            LOG_INFO("UAC audio format not supported");
            LOG_INFO("Trying to use default format: 48kHz, 2 channels, 16 bits");
            // 尝试使用默认格式
            aml_uac_set_sample_rate(48000);
            aml_uac_set_channels(2);
            break;
        case UAC_ERROR_DEVICE_DISCONNECTED:
            LOG_INFO("UAC device disconnected unexpectedly");
            // 重置设备状态
            g_uac_state.connected = false;
            g_uac_state.device_path[0] = '\0';
            g_uac_state.device_name[0] = '\0';
            // 更新LED状态
            led_ctrl_set_state(LED_UAC, LED_STATE_OFF);
            // 更新LCD显示
            lcd_display_text("UAC Disconnected", LCD_LINE_1);
            lcd_display_text("", LCD_LINE_2);
            // 发送UAC设备断开事件
            event_notify(EVENT_UAC_DISCONNECTED, NULL);
            break;
        case UAC_ERROR_DEVICE_NOT_FOUND:
            LOG_INFO("UAC device not found");
            break;
        case UAC_ERROR_INVALID_PARAMETER:
            LOG_INFO("UAC invalid parameter error");
            break;
        case UAC_ERROR_INSUFFICIENT_RESOURCES:
            LOG_INFO("UAC insufficient resources error");
            LOG_INFO("Possible causes: low memory, too many open devices");
            break;
        default:
            LOG_INFO("Unknown UAC device error");
            LOG_INFO("Error code: %d", error_code);
            break;
    }
}

// UAC仅高端产品支持
#ifdef CONFIG_ENABLE_UAC
int src_uac_init(void) {
    if (g_uac_state.init) {
        LOG_INFO("UAC source already initialized");
        return 0;
    }
    
    // 初始化Amlogic UAC SDK
    if (aml_uac_init() != 0) {
        LOG_ERROR("UAC source init failed: UAC SDK init error");
        LOG_ERROR("Possible causes: SDK not installed, insufficient permissions, or hardware issues");
        return -1;
    }
    
    // 设置UAC回调函数
    if (aml_uac_set_device_callback(uac_device_callback) != 0) {
        LOG_ERROR("Failed to set UAC device callback");
        aml_uac_deinit();
        return -1;
    }
    
    if (aml_uac_set_data_callback(uac_audio_data_callback) != 0) {
        LOG_ERROR("Failed to set UAC data callback");
        aml_uac_deinit();
        return -1;
    }
    
    if (aml_uac_set_error_callback(uac_error_callback) != 0) {
        LOG_ERROR("Failed to set UAC error callback");
        aml_uac_deinit();
        return -1;
    }
    
    // 配置UAC参数
    if (aml_uac_set_sample_rate(g_uac_state.sample_rate) != 0) {  // 默认采样率48kHz
        LOG_WARN("Failed to set default sample rate, using SDK default");
    }
    
    if (aml_uac_set_channels(g_uac_state.channels) != 0) {         // 默认立体声
        LOG_WARN("Failed to set default channels, using SDK default");
    }
    
    if (aml_uac_set_buffer_size(g_uac_state.buffer_size) != 0) {   // 默认缓冲区大小4KB
        LOG_WARN("Failed to set default buffer size, using SDK default");
    }
    
    if (aml_uac_set_buffer_count(g_uac_state.buffer_count) != 0) {     // 设置缓冲区数量为4个
        LOG_WARN("Failed to set default buffer count, using SDK default");
    }
    
    // 启用UAC设备检测
    if (aml_uac_enable_detection(true) != 0) {
        LOG_ERROR("Failed to enable UAC device detection");
        aml_uac_deinit();
        return -1;
    }
    
    g_uac_state.init = true;
    g_uac_state.connected = false;
    g_uac_state.device_path[0] = '\0';
    g_uac_state.device_name[0] = '\0';
    
    LOG_INFO("UAC (USB Audio Class) source init success");
    LOG_INFO("  Device detection: ENABLED");
    LOG_INFO("  Default sample rate: %d Hz", g_uac_state.sample_rate);
    LOG_INFO("  Default channels: %d", g_uac_state.channels);
    LOG_INFO("  Buffer size: %d bytes x %d buffers", g_uac_state.buffer_size, g_uac_state.buffer_count);
    
    return 0;
}

void src_uac_deinit(void) {
    if (g_uac_state.init) {
        // 禁用UAC设备检测
        if (aml_uac_enable_detection(false) != 0) {
            LOG_WARN("Failed to disable UAC device detection, continuing deinit");
        }
        
        // 关闭当前打开的UAC设备
        if (g_uac_state.connected) {
            if (aml_uac_close() != 0) {
                LOG_WARN("Failed to close UAC device, continuing deinit");
            } else {
                // 更新LED状态
                led_ctrl_set_state(LED_UAC, LED_STATE_OFF);
                
                // 更新LCD显示
                lcd_display_text("UAC Disconnected", LCD_LINE_1);
                lcd_display_text("", LCD_LINE_2);
            }
        }
        
        // 反初始化Amlogic UAC SDK
        if (aml_uac_deinit() != 0) {
            LOG_WARN("Failed to deinit Amlogic UAC SDK");
        }
        
        g_uac_state.init = false;
        g_uac_state.connected = false;
        g_uac_state.device_path[0] = '\0';
        g_uac_state.device_name[0] = '\0';
        
        LOG_INFO("UAC (USB Audio Class) source deinit success");
    } else {
        LOG_INFO("UAC source not initialized, skipping deinit");
    }
}

/**
 * @brief 获取UAC设备连接状态
 * @return 连接状态：true表示已连接，false表示未连接
 */
bool src_uac_get_connect_state(void) {
    return g_uac_state.connected;
}

/**
 * @brief 获取当前UAC设备名称
 * @return 设备名称：已连接时返回设备名称，未连接时返回NULL
 */
const char *src_uac_get_device_name(void) {
    return g_uac_state.connected ? g_uac_state.device_name : NULL;
}

/**
 * @brief 获取当前UAC音频参数
 * @param sample_rate 采样率指针，用于返回当前采样率
 * @param channels 通道数指针，用于返回当前通道数
 * @param bit_depth 位深度指针，用于返回当前位深度
 * @return 操作结果：0表示成功，-1表示失败
 */
int src_uac_get_audio_params(int *sample_rate, int *channels, int *bit_depth) {
    if (!g_uac_state.connected) {
        LOG_WARN("UAC device not connected, cannot get audio params");
        return -1;
    }
    
    if (sample_rate) {
        *sample_rate = g_uac_state.sample_rate;
    }
    
    if (channels) {
        *channels = g_uac_state.channels;
    }
    
    if (bit_depth) {
        *bit_depth = g_uac_state.bit_depth;
    }
    
    return 0;
}

/**
 * @brief 获取UAC设备状态
 * @details 返回UAC设备的连接状态
 * @return 设备状态：1表示已连接，0表示未连接
 */
int src_uac_get_status(void) {
    return g_uac_state.connected ? 1 : 0;
}

/**
 * @brief 获取UAC设备列表
 * @details 枚举所有可用的UAC设备
 * @param device_list 设备列表指针，用于返回设备路径列表
 * @param max_count 最大设备数量
 * @return 实际设备数量，-1表示失败
 */
int src_uac_get_device_list(char **device_list, int max_count) {
    if (!g_uac_state.init) {
        LOG_WARN("UAC source not initialized, cannot get device list");
        return -1;
    }
    
    if (!device_list || max_count <= 0) {
        LOG_ERROR("Invalid parameters for src_uac_get_device_list");
        return -1;
    }
    
    // 调用SDK获取设备列表
    int count = aml_uac_get_device_list(device_list, max_count);
    if (count < 0) {
        LOG_ERROR("Failed to get UAC device list");
        return -1;
    }
    
    LOG_INFO("Found %d UAC devices", count);
    for (int i = 0; i < count; i++) {
        if (device_list[i]) {
            LOG_INFO("  UAC device %d: %s", i+1, device_list[i]);
        }
    }
    
    return count;
}

/**
 * @brief 设置UAC采样率
 * @details 设置UAC设备的采样率
 * @param sample_rate 采样率值
 * @return 操作结果：0表示成功，-1表示失败
 */
int src_uac_set_sample_rate(int sample_rate) {
    if (!g_uac_state.init) {
        LOG_WARN("UAC source not initialized, cannot set sample rate");
        return -1;
    }
    
    if (sample_rate <= 0 || sample_rate > 192000) {
        LOG_ERROR("Invalid sample rate: %d", sample_rate);
        return -1;
    }
    
    if (aml_uac_set_sample_rate(sample_rate) != 0) {
        LOG_ERROR("Failed to set UAC sample rate to %d", sample_rate);
        return -1;
    }
    
    g_uac_state.sample_rate = sample_rate;
    LOG_INFO("Set UAC sample rate to %d Hz", sample_rate);
    return 0;
}

/**
 * @brief 设置UAC通道数
 * @details 设置UAC设备的通道数
 * @param channels 通道数
 * @return 操作结果：0表示成功，-1表示失败
 */
int src_uac_set_channels(int channels) {
    if (!g_uac_state.init) {
        LOG_WARN("UAC source not initialized, cannot set channels");
        return -1;
    }
    
    if (channels <= 0 || channels > 8) {
        LOG_ERROR("Invalid channel count: %d", channels);
        return -1;
    }
    
    if (aml_uac_set_channels(channels) != 0) {
        LOG_ERROR("Failed to set UAC channels to %d", channels);
        return -1;
    }
    
    g_uac_state.channels = channels;
    LOG_INFO("Set UAC channels to %d", channels);
    return 0;
}

/**
 * @brief 设置UAC缓冲区大小
 * @details 设置UAC设备的缓冲区大小
 * @param buffer_size 缓冲区大小（字节）
 * @return 操作结果：0表示成功，-1表示失败
 */
int src_uac_set_buffer_size(int buffer_size) {
    if (!g_uac_state.init) {
        LOG_WARN("UAC source not initialized, cannot set buffer size");
        return -1;
    }
    
    if (buffer_size <= 0 || buffer_size > 65536) {
        LOG_ERROR("Invalid buffer size: %d", buffer_size);
        return -1;
    }
    
    if (aml_uac_set_buffer_size(buffer_size) != 0) {
        LOG_ERROR("Failed to set UAC buffer size to %d", buffer_size);
        return -1;
    }
    
    g_uac_state.buffer_size = buffer_size;
    LOG_INFO("Set UAC buffer size to %d bytes", buffer_size);
    return 0;
}

/**
 * @brief 设置UAC缓冲区数量
 * @details 设置UAC设备的缓冲区数量
 * @param buffer_count 缓冲区数量
 * @return 操作结果：0表示成功，-1表示失败
 */
int src_uac_set_buffer_count(int buffer_count) {
    if (!g_uac_state.init) {
        LOG_WARN("UAC source not initialized, cannot set buffer count");
        return -1;
    }
    
    if (buffer_count <= 0 || buffer_count > 16) {
        LOG_ERROR("Invalid buffer count: %d", buffer_count);
        return -1;
    }
    
    if (aml_uac_set_buffer_count(buffer_count) != 0) {
        LOG_ERROR("Failed to set UAC buffer count to %d", buffer_count);
        return -1;
    }
    
    g_uac_state.buffer_count = buffer_count;
    LOG_INFO("Set UAC buffer count to %d", buffer_count);
    return 0;
}

#else
int src_uac_init(void) { return 0; }
void src_uac_deinit(void) {
    LOG_INFO("UAC source deinit called");
    // 清理UAC相关资源
    // 虽然是空实现，但保持函数接口一致
}
bool src_uac_get_connect_state(void) { return false; }
const char *src_uac_get_device_name(void) { return NULL; }
int src_uac_get_audio_params(int *sample_rate, int *channels, int *bit_depth) { return -1; }
int src_uac_get_status(void) { return 0; }
int src_uac_get_device_list(char **device_list, int max_count) { return -1; }
int src_uac_set_sample_rate(int sample_rate) { return -1; }
int src_uac_set_channels(int channels) { return -1; }
int src_uac_set_buffer_size(int buffer_size) { return -1; }
int src_uac_set_buffer_count(int buffer_count) { return -1; }
#endif