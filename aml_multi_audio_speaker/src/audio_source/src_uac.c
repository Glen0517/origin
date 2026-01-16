#include "audio_source_priv.h"
#include "logger.h"
#include "audio_core.h"
#include "product_type.h"
#include "peripheral.h"
#include "peripheral_priv.h"
#include "event.h"

#include <aml_uac.h>         // 晶晨UAC SDK

static bool g_uac_src_init = false;
static bool g_uac_device_connected = false;
static char g_uac_device_path[64] = {0};
static int g_uac_sample_rate = 48000;
static int g_uac_channels = 2;
static int g_uac_bit_depth = 16;
static char g_uac_device_name[128] = {0};

/**
 * @brief UAC设备信息获取函数
 */
static void uac_get_device_info(const char *dev_path) {
    if (!dev_path) {
        return;
    }
    
    // 获取UAC设备名称
    if (aml_uac_get_device_name(dev_path, g_uac_device_name, sizeof(g_uac_device_name)) == 0) {
        LOG_INFO("UAC device name: %s", g_uac_device_name);
    } else {
        strncpy(g_uac_device_name, "Unknown UAC Device", sizeof(g_uac_device_name) - 1);
    }
    
    // 获取UAC设备支持的采样率
    int sample_rates[10] = {0};
    int count = aml_uac_get_supported_sample_rates(dev_path, sample_rates, sizeof(sample_rates) / sizeof(int));
    if (count > 0) {
        LOG_INFO("UAC supported sample rates:");
        for (int i = 0; i < count; i++) {
            LOG_INFO("  - %d Hz", sample_rates[i]);
        }
    }
}

/**
 * @brief UAC设备连接回调函数
 */
static void uac_device_callback(const char *dev_path, bool connected) {
    if (g_uac_src_init) {
        if (connected) {
            // 设备连接处理
            strncpy(g_uac_device_path, dev_path, sizeof(g_uac_device_path) - 1);
            LOG_INFO("UAC device connected: %s", dev_path);
            
            // 获取设备信息
            uac_get_device_info(dev_path);
            
            // 打开UAC设备
            if (aml_uac_open(dev_path) == 0) {
                g_uac_device_connected = true;
                
                // 获取实际的音频参数
                g_uac_sample_rate = aml_uac_get_current_sample_rate();
                g_uac_channels = aml_uac_get_current_channels();
                g_uac_bit_depth = aml_uac_get_current_bit_depth();
                
                LOG_INFO("UAC device opened successfully");
                LOG_INFO("  Current sample rate: %d Hz", g_uac_sample_rate);
                LOG_INFO("  Current channels: %d", g_uac_channels);
                LOG_INFO("  Current bit depth: %d bits", g_uac_bit_depth);
                
                // 更新LED状态
                led_ctrl_set_state(LED_UAC, LED_STATE_ON);
                
                // 更新LCD显示
                lcd_display_text("UAC Connected", LCD_LINE_1);
                lcd_display_text(g_uac_device_name, LCD_LINE_2);
                
                // 发送UAC设备连接事件
                event_notify(EVENT_UAC_CONNECTED, (void *)g_uac_device_name);
            } else {
                LOG_ERROR("Failed to open UAC device: %s", dev_path);
                g_uac_device_path[0] = '\0';
            }
        } else {
            // 设备断开处理
            LOG_INFO("UAC device disconnected: %s", g_uac_device_path);
            
            // 关闭UAC设备
            aml_uac_close();
            
            g_uac_device_connected = false;
            g_uac_device_path[0] = '\0';
            g_uac_device_name[0] = '\0';
            
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
    if (g_uac_src_init && g_uac_device_connected && pcm_data && data_len > 0) {
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
 */
static void uac_error_callback(int error_code) {
    LOG_ERROR("UAC device error: %d", error_code);
    
    // 根据错误代码进行相应的处理
    switch (error_code) {
        case UAC_ERROR_DEVICE_BUSY:
            LOG_INFO("UAC device is busy, please try again later");
            break;
        case UAC_ERROR_DATA_OVERRUN:
            LOG_INFO("UAC data overrun detected, check USB connection");
            break;
        case UAC_ERROR_FORMAT_NOT_SUPPORTED:
            LOG_INFO("UAC audio format not supported");
            break;
        default:
            LOG_INFO("Unknown UAC device error");
            break;
    }
}

// UAC仅高端产品支持
#ifdef CONFIG_ENABLE_UAC
int src_uac_init(void) {
    if (g_uac_src_init) {
        LOG_INFO("UAC source already initialized");
        return 0;
    }
    
    // 初始化Amlogic UAC SDK
    if (aml_uac_init() != 0) {
        LOG_ERROR("UAC source init failed: UAC SDK init error");
        return -1;
    }
    
    // 设置UAC回调函数
    aml_uac_set_device_callback(uac_device_callback);
    aml_uac_set_data_callback(uac_audio_data_callback);
    aml_uac_set_error_callback(uac_error_callback);
    
    // 配置UAC参数
    aml_uac_set_sample_rate(48000);      // 默认采样率48kHz
    aml_uac_set_channels(2);             // 默认立体声
    aml_uac_set_buffer_size(4096);       // 默认缓冲区大小4KB
    aml_uac_set_buffer_count(4);         // 设置缓冲区数量为4个
    
    // 启用UAC设备检测
    aml_uac_enable_detection(true);
    
    g_uac_src_init = true;
    g_uac_device_connected = false;
    g_uac_device_path[0] = '\0';
    g_uac_device_name[0] = '\0';
    
    LOG_INFO("UAC (USB Audio Class) source init success");
    LOG_INFO("  Device detection: ENABLED");
    LOG_INFO("  Default sample rate: 48kHz");
    LOG_INFO("  Default channels: Stereo");
    LOG_INFO("  Buffer size: 4KB x 4 buffers");
    
    return 0;
}

void src_uac_deinit(void) {
    if (g_uac_src_init) {
        // 禁用UAC设备检测
        aml_uac_enable_detection(false);
        
        // 关闭当前打开的UAC设备
        if (g_uac_device_connected) {
            aml_uac_close();
            
            // 更新LED状态
            led_ctrl_set_state(LED_UAC, LED_STATE_OFF);
        }
        
        // 反初始化Amlogic UAC SDK
        aml_uac_deinit();
        
        g_uac_src_init = false;
        g_uac_device_connected = false;
        g_uac_device_path[0] = '\0';
        g_uac_device_name[0] = '\0';
        
        LOG_INFO("UAC (USB Audio Class) source deinit success");
    }
}

/**
 * @brief 获取UAC设备连接状态
 */
bool src_uac_get_connect_state(void) {
    return g_uac_device_connected;
}

/**
 * @brief 获取当前UAC设备名称
 */
const char *src_uac_get_device_name(void) {
    return g_uac_device_connected ? g_uac_device_name : NULL;
}

/**
 * @brief 获取当前UAC音频参数
 */
int src_uac_get_audio_params(int *sample_rate, int *channels, int *bit_depth) {
    if (!g_uac_device_connected) {
        return -1;
    }
    
    if (sample_rate) {
        *sample_rate = g_uac_sample_rate;
    }
    
    if (channels) {
        *channels = g_uac_channels;
    }
    
    if (bit_depth) {
        *bit_depth = g_uac_bit_depth;
    }
    
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
#endif