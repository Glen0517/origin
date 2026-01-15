#include "audio_source_priv.h"
#include "logger.h"
#include "audio_core.h"
#include "bluetooth.h"

#include <aml_a2dp.h>       // 晶晨A2DP SDK

static bool g_bt_src_init = false;
static bool g_bt_connected = false;
static int g_bt_volume = 80;

/**
 * @brief 蓝牙A2DP音频接收回调函数
 */
static void bt_a2dp_audio_callback(uint8_t *pcm_data, int data_len) {
    if (g_bt_src_init && g_bt_connected && pcm_data && data_len > 0) {
        // 将接收到的音频数据发送到音频核心
        audio_core_play_pcm(pcm_data, data_len);
    }
}

/**
 * @brief 蓝牙连接状态回调函数
 */
static void bt_a2dp_status_callback(int status) {
    switch (status) {
        case A2DP_STATUS_CONNECTED:
            g_bt_connected = true;
            LOG_INFO("Bluetooth source: connected");
            break;
        case A2DP_STATUS_DISCONNECTED:
            g_bt_connected = false;
            LOG_INFO("Bluetooth source: disconnected");
            break;
        case A2DP_STATUS_PLAYING:
            LOG_INFO("Bluetooth source: playing");
            break;
        case A2DP_STATUS_PAUSED:
            LOG_INFO("Bluetooth source: paused");
            break;
        default:
            break;
    }
}

/**
 * @brief 蓝牙A2DP音量回调函数
 */
static void bt_a2dp_volume_callback(int volume) {
    g_bt_volume = volume;
    LOG_INFO("Bluetooth source: volume changed to %d", volume);
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
    
    // 设置初始音量
    aml_a2dp_set_volume(g_bt_volume);
    
    // 启用A2DP
    aml_a2dp_enable(true);
    
    g_bt_src_init = true;
    g_bt_connected = false;
    
    LOG_INFO("Bluetooth (A2DP) source init success");
    LOG_INFO("  Initial volume: %d", g_bt_volume);
    
    return 0;
}

void src_bt_deinit(void) {
    if (g_bt_src_init) {
        // 禁用A2DP
        aml_a2dp_enable(false);
        
        // 反初始化Amlogic A2DP
        aml_a2dp_deinit();
        
        g_bt_src_init = false;
        g_bt_connected = false;
        
        LOG_INFO("Bluetooth (A2DP) source deinit success");
    }
}