#include "audio_source_priv.h"
#include "logger.h"
#include "audio_core.h"
#include "wifi_media.h"
#include "product_type.h"

#include <aml_wifi_cast.h>   // 晶晨WIFI投屏SDK
#include <aml_net_play.h>    // 晶晨网络播放SDK

static bool g_wifi_src_init = false;
static bool g_wifi_cast_enabled = true;
static bool g_net_play_enabled = true;

/**
 * @brief WIFI投屏音频数据接收回调函数
 */
static void wifi_cast_audio_callback(uint8_t *pcm_data, int data_len) {
    if (g_wifi_src_init && g_wifi_cast_enabled && pcm_data && data_len > 0) {
        // 将接收到的音频数据发送到音频核心
        audio_core_play_pcm(pcm_data, data_len);
    }
}

/**
 * @brief 网络播放音频数据接收回调函数
 */
static void net_play_audio_callback(uint8_t *pcm_data, int data_len) {
    if (g_wifi_src_init && g_net_play_enabled && pcm_data && data_len > 0) {
        // 将接收到的音频数据发送到音频核心
        audio_core_play_pcm(pcm_data, data_len);
    }
}

// WIFI仅高端产品支持
#ifdef CONFIG_ENABLE_WIFI_MEDIA
int src_wifi_init(void) {
    if (g_wifi_src_init) {
        LOG_INFO("WIFI source already initialized");
        return 0;
    }
    
    // 初始化WIFI媒体模块
    WifiMediaConfig_t wifi_cfg = {
        .wifi_name = "Aml_Soundbar",
        .dlna_en = true,
        .airplay_en = true
    };
    
    if (wifi_media_init(&wifi_cfg) != 0) {
        LOG_ERROR("WIFI source init failed: WIFI media module init error");
        return -1;
    }
    
    // 初始化Amlogic WIFI投屏SDK
    if (g_wifi_cast_enabled) {
        if (aml_wifi_cast_init() != 0) {
            LOG_ERROR("WIFI source init failed: WIFI cast SDK init error");
        } else {
            aml_wifi_cast_set_audio_callback(wifi_cast_audio_callback);
            LOG_INFO("WIFI cast module init success");
        }
    }
    
    // 初始化Amlogic网络播放SDK
    if (g_net_play_enabled) {
        if (aml_net_play_init() != 0) {
            LOG_ERROR("WIFI source init failed: Network play SDK init error");
        } else {
            aml_net_play_set_audio_callback(net_play_audio_callback);
            LOG_INFO("Network play module init success");
        }
    }
    
    g_wifi_src_init = true;
    
    LOG_INFO("WIFI (cast/network) source init success");
    LOG_INFO("  WIFI cast enabled: %s", g_wifi_cast_enabled ? "YES" : "NO");
    LOG_INFO("  Network play enabled: %s", g_net_play_enabled ? "YES" : "NO");
    
    return 0;
}

void src_wifi_deinit(void) {
    if (g_wifi_src_init) {
        // 反初始化Amlogic网络播放SDK
        if (g_net_play_enabled) {
            aml_net_play_deinit();
        }
        
        // 反初始化Amlogic WIFI投屏SDK
        if (g_wifi_cast_enabled) {
            aml_wifi_cast_deinit();
        }
        
        // 反初始化WIFI媒体模块
        wifi_media_deinit();
        
        g_wifi_src_init = false;
        
        LOG_INFO("WIFI (cast/network) source deinit success");
    }
}
#else
int src_wifi_init(void) { return 0; }
void src_wifi_deinit(void) {}
#endif