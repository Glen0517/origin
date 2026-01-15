#include "wifi_media.h"
#include "logger.h"
#include "product_type.h"

#include <aml_wifi.h>        // 晶晨WIFI SDK
#include <aml_dlna.h>        // 晶晨DLNA SDK
#include <aml_airplay.h>     // 晶晨AirPlay SDK

#ifdef CONFIG_ENABLE_WIFI_MEDIA

static bool g_wifi_media_init = false;
static bool g_wifi_connected = false;
static bool g_dlna_enabled = true;
static bool g_airplay_enabled = true;
static char g_wifi_name[32] = "Aml_Soundbar";

/**
 * @brief WIFI连接状态回调函数
 */
static void wifi_connect_callback(const char *ssid, bool connected) {
    g_wifi_connected = connected;
    LOG_INFO("WIFI %s: %s", connected ? "connected" : "disconnected", ssid);
}

/**
 * @brief DLNA状态回调函数
 */
static void dlna_status_callback(int status) {
    LOG_INFO("DLNA status: %d", status);
}

/**
 * @brief AirPlay状态回调函数
 */
static void airplay_status_callback(int status) {
    LOG_INFO("AirPlay status: %d", status);
}

int wifi_media_init(WifiMediaConfig_t *cfg) {
    if (g_wifi_media_init) {
        LOG_INFO("WIFI media module already initialized");
        return 0;
    }
    
    // 初始化配置
    if (cfg != NULL) {
        strncpy(g_wifi_name, cfg->wifi_name, sizeof(g_wifi_name) - 1);
        g_dlna_enabled = cfg->dlna_en;
        g_airplay_enabled = cfg->airplay_en;
    }
    
    // 初始化Amlogic WIFI SDK
    if (aml_wifi_init() != 0) {
        LOG_ERROR("WIFI media init failed: WIFI SDK init error");
        return -1;
    }
    
    // 设置WIFI回调
    aml_wifi_set_connect_callback(wifi_connect_callback);
    
    // 初始化DLNA（如果启用）
    if (g_dlna_enabled) {
        if (aml_dlna_init() != 0) {
            LOG_ERROR("WIFI media init failed: DLNA SDK init error");
        } else {
            aml_dlna_set_status_callback(dlna_status_callback);
            aml_dlna_set_device_name(g_wifi_name);
            aml_dlna_start();
            LOG_INFO("DLNA module init success");
        }
    }
    
    // 初始化AirPlay（如果启用）
    if (g_airplay_enabled) {
        if (aml_airplay_init() != 0) {
            LOG_ERROR("WIFI media init failed: AirPlay SDK init error");
        } else {
            aml_airplay_set_status_callback(airplay_status_callback);
            aml_airplay_set_device_name(g_wifi_name);
            aml_airplay_start();
            LOG_INFO("AirPlay module init success");
        }
    }
    
    g_wifi_media_init = true;
    g_wifi_connected = false;
    
    LOG_INFO("WIFI media module init success (CAST/NET PLAY) [HIGH END ONLY]");
    LOG_INFO("  Device name: %s", g_wifi_name);
    LOG_INFO("  DLNA enabled: %s", g_dlna_enabled ? "YES" : "NO");
    LOG_INFO("  AirPlay enabled: %s", g_airplay_enabled ? "YES" : "NO");
    
    return 0;
}

int wifi_media_deinit(void) {
    if (g_wifi_media_init) {
        // 停止AirPlay（如果启用）
        if (g_airplay_enabled) {
            aml_airplay_stop();
            aml_airplay_deinit();
        }
        
        // 停止DLNA（如果启用）
        if (g_dlna_enabled) {
            aml_dlna_stop();
            aml_dlna_deinit();
        }
        
        // 反初始化WIFI SDK
        aml_wifi_deinit();
        
        g_wifi_media_init = false;
        g_wifi_connected = false;
        
        LOG_INFO("WIFI media module deinit success");
    }
    
    return 0;
}

bool wifi_media_get_connect_state(void) {
    return g_wifi_connected;
}

#else
int wifi_media_init(WifiMediaConfig_t *cfg) { return 0; }
int wifi_media_deinit(void) { return 0; }
bool wifi_media_get_connect_state(void) { return false; }
#endif