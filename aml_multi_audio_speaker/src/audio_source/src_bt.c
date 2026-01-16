#include "audio_source_priv.h"
#include "logger.h"
#include "audio_core.h"
#include "bluetooth.h"
#include "peripheral.h"
#include "peripheral_priv.h"
#include "event.h"

#include <aml_a2dp.h>       // 晶晨A2DP SDK

static bool g_bt_src_init = false;
static bool g_bt_connected = false;
static bool g_bt_playing = false;
static int g_bt_volume = 80;
static char g_bt_device_name[64] = {0};
static char g_bt_device_mac[18] = {0}; // XX:XX:XX:XX:XX:XX
static int g_bt_audio_state = 0;

/**
 * @brief 蓝牙A2DP音频接收回调函数
 */
static void bt_a2dp_audio_callback(uint8_t *pcm_data, int data_len) {
    if (g_bt_src_init && g_bt_connected && pcm_data && data_len > 0) {
        // 检查数据长度是否合理
        if (data_len > 0 && data_len < 8192) { // 限制最大数据长度为8KB
            // 将接收到的音频数据发送到音频核心
            if (audio_core_play_pcm(pcm_data, data_len) != 0) {
                LOG_WARN("Failed to send BT audio data to audio core");
            }
        } else {
            LOG_WARN("Invalid BT audio data length: %d", data_len);
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
    switch (status) {
        case A2DP_STATUS_CONNECTED:
            g_bt_connected = true;
            g_bt_playing = false;
            
            // 获取连接的设备信息
            aml_a2dp_get_device_name(g_bt_device_name, sizeof(g_bt_device_name));
            aml_a2dp_get_device_mac(g_bt_device_mac, sizeof(g_bt_device_mac));
            
            LOG_INFO("Bluetooth source: connected to %s (%s)", g_bt_device_name, g_bt_device_mac);
            
            // 更新LED状态
            led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_ON);
            
            // 更新LCD显示
            lcd_display_text("BT Connected", LCD_LINE_1);
            lcd_display_text(g_bt_device_name, LCD_LINE_2);
            
            // 发送蓝牙连接事件
            event_notify(EVENT_BT_CONNECTED, (void *)g_bt_device_name);
            break;
            
        case A2DP_STATUS_DISCONNECTED:
            g_bt_connected = false;
            g_bt_playing = false;
            
            LOG_INFO("Bluetooth source: disconnected from %s (%s)", g_bt_device_name, g_bt_device_mac);
            
            // 更新LED状态
            led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_OFF);
            
            // 更新LCD显示
            lcd_display_text("BT Disconnected", LCD_LINE_1);
            lcd_display_text("", LCD_LINE_2);
            
            // 发送蓝牙断开事件
            event_notify(EVENT_BT_DISCONNECTED, NULL);
            
            // 清空设备信息
            g_bt_device_name[0] = '\0';
            g_bt_device_mac[0] = '\0';
            break;
            
        case A2DP_STATUS_PLAYING:
            g_bt_playing = true;
            LOG_INFO("Bluetooth source: playing");
            
            // 更新LED状态
            led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_BLINK_SLOW);
            
            // 更新LCD显示
            lcd_display_text("BT Playing", LCD_LINE_1);
            lcd_display_text(g_bt_device_name, LCD_LINE_2);
            
            // 发送蓝牙播放开始事件
            event_notify(EVENT_BT_PLAY_START, NULL);
            break;
            
        case A2DP_STATUS_PAUSED:
            g_bt_playing = false;
            LOG_INFO("Bluetooth source: paused");
            
            // 更新LED状态
            led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_ON);
            
            // 更新LCD显示
            lcd_display_text("BT Paused", LCD_LINE_1);
            lcd_display_text(g_bt_device_name, LCD_LINE_2);
            
            // 发送蓝牙播放暂停事件
            event_notify(EVENT_BT_PLAY_PAUSE, NULL);
            break;
            
        case A2DP_STATUS_STOPPED:
            g_bt_playing = false;
            LOG_INFO("Bluetooth source: stopped");
            
            // 更新LED状态
            led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_ON);
            
            // 更新LCD显示
            lcd_display_text("BT Stopped", LCD_LINE_1);
            lcd_display_text(g_bt_device_name, LCD_LINE_2);
            
            // 发送蓝牙播放停止事件
            event_notify(EVENT_BT_PLAY_STOP, NULL);
            break;
            
        case A2DP_STATUS_ERROR:
            g_bt_playing = false;
            LOG_ERROR("Bluetooth source: error occurred");
            
            // 更新LED状态
            led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_BLINK_FAST);
            
            // 更新LCD显示
            lcd_display_text("BT Error", LCD_LINE_1);
            lcd_display_text(g_bt_device_name, LCD_LINE_2);
            
            // 发送蓝牙错误事件
            event_notify(EVENT_BT_ERROR, NULL);
            break;
            
        default:
            LOG_INFO("Bluetooth source: unknown status %d", status);
            break;
    }
    
    g_bt_audio_state = status;
}

/**
 * @brief 蓝牙A2DP音量回调函数
 */
static void bt_a2dp_volume_callback(int volume) {
    g_bt_volume = volume;
    LOG_INFO("Bluetooth source: volume changed to %d", volume);
    
    // 发送音量变化事件
    event_notify(EVENT_BT_VOLUME_CHANGED, &volume);
}

/**
 * @brief 蓝牙A2DP音频参数回调函数
 */
static void bt_a2dp_audio_params_callback(int sample_rate, int channels, int bit_depth) {
    LOG_INFO("Bluetooth audio params: %d Hz, %d channels, %d bits", sample_rate, channels, bit_depth);
    
    // 可以根据需要调整音频核心参数
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
    
    return 0;
}

void src_bt_deinit(void) {
    if (g_bt_src_init) {
        // 禁用A2DP
        aml_a2dp_enable(false);
        
        // 停止当前播放
        aml_a2dp_stop();
        
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