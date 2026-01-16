#include "wifi_media.h"
#include "logger.h"
#include "product_type.h"
#include "peripheral.h"
#include "peripheral_priv.h"
#include "event.h"
#include "audio_core.h"

#include <aml_wifi.h>        // 晶晨WIFI SDK
#include <aml_dlna.h>        // 晶晨DLNA SDK
#include <aml_airplay.h>     // 晶晨AirPlay SDK

#ifdef CONFIG_ENABLE_WIFI_MEDIA

static bool g_wifi_media_init = false;
static bool g_wifi_connected = false;
static bool g_dlna_enabled = true;
static bool g_airplay_enabled = true;
static bool g_dlna_playing = false;
static bool g_airplay_playing = false;
static char g_wifi_name[32] = "Aml_Soundbar";
static char g_current_ssid[32] = {0};
static char g_playing_media_title[128] = {0};
static char g_playing_media_artist[128] = {0};
static int g_current_volume = 80;

/**
 * @brief WIFI连接状态回调函数
 */
static void wifi_connect_callback(const char *ssid, bool connected) {
    g_wifi_connected = connected;
    
    if (connected) {
        strncpy(g_current_ssid, ssid, sizeof(g_current_ssid) - 1);
        LOG_INFO("WIFI connected: %s", ssid);
        
        // 更新LED状态
        led_ctrl_set_state(LED_WIFI, LED_STATE_ON);
        
        // 更新LCD显示
        lcd_display_text("WIFI Connected", LCD_LINE_1);
        lcd_display_text(ssid, LCD_LINE_2);
        
        // 发送WIFI连接事件
        event_notify(EVENT_WIFI_CONNECTED, (void *)ssid);
    } else {
        LOG_INFO("WIFI disconnected: %s", g_current_ssid);
        g_current_ssid[0] = '\0';
        
        // 更新LED状态
        led_ctrl_set_state(LED_WIFI, LED_STATE_OFF);
        
        // 更新LCD显示
        lcd_display_text("WIFI Disconnected", LCD_LINE_1);
        lcd_display_text("", LCD_LINE_2);
        
        // 发送WIFI断开事件
        event_notify(EVENT_WIFI_DISCONNECTED, NULL);
        
        // 停止当前播放
        if (g_dlna_playing) {
            aml_dlna_stop();
            g_dlna_playing = false;
        }
        if (g_airplay_playing) {
            aml_airplay_stop();
            g_airplay_playing = false;
        }
    }
}

/**
 * @brief DLNA状态回调函数
 */
static void dlna_status_callback(int status) {
    switch (status) {
        case DLNA_STATUS_IDLE:
            LOG_INFO("DLNA: Idle");
            g_dlna_playing = false;
            break;
        case DLNA_STATUS_PLAYING:
            LOG_INFO("DLNA: Playing");
            g_dlna_playing = true;
            g_airplay_playing = false;
            
            // 更新LED状态
            led_ctrl_set_state(LED_WIFI, LED_STATE_BLINK_SLOW);
            
            // 更新LCD显示
            lcd_display_text("DLNA Playing", LCD_LINE_1);
            lcd_display_text(g_playing_media_title, LCD_LINE_2);
            
            // 发送DLNA播放事件
            event_notify(EVENT_DLNA_PLAY_START, (void *)g_playing_media_title);
            break;
        case DLNA_STATUS_PAUSED:
            LOG_INFO("DLNA: Paused");
            g_dlna_playing = false;
            
            // 更新LED状态
            led_ctrl_set_state(LED_WIFI, LED_STATE_ON);
            
            // 更新LCD显示
            lcd_display_text("DLNA Paused", LCD_LINE_1);
            lcd_display_text(g_playing_media_title, LCD_LINE_2);
            
            // 发送DLNA暂停事件
            event_notify(EVENT_DLNA_PLAY_PAUSE, NULL);
            break;
        case DLNA_STATUS_STOPPED:
            LOG_INFO("DLNA: Stopped");
            g_dlna_playing = false;
            g_playing_media_title[0] = '\0';
            g_playing_media_artist[0] = '\0';
            
            // 更新LED状态
            led_ctrl_set_state(LED_WIFI, LED_STATE_ON);
            
            // 更新LCD显示
            lcd_display_text("DLNA Stopped", LCD_LINE_1);
            lcd_display_text("", LCD_LINE_2);
            
            // 发送DLNA停止事件
            event_notify(EVENT_DLNA_PLAY_STOP, NULL);
            break;
        default:
            LOG_INFO("DLNA status: %d", status);
            break;
    }
}

/**
 * @brief AirPlay状态回调函数
 */
static void airplay_status_callback(int status) {
    switch (status) {
        case AIRPLAY_STATUS_IDLE:
            LOG_INFO("AirPlay: Idle");
            g_airplay_playing = false;
            break;
        case AIRPLAY_STATUS_PLAYING:
            LOG_INFO("AirPlay: Playing");
            g_airplay_playing = true;
            g_dlna_playing = false;
            
            // 更新LED状态
            led_ctrl_set_state(LED_WIFI, LED_STATE_BLINK_SLOW);
            
            // 更新LCD显示
            lcd_display_text("AirPlay Playing", LCD_LINE_1);
            lcd_display_text(g_playing_media_title, LCD_LINE_2);
            
            // 发送AirPlay播放事件
            event_notify(EVENT_AIRPLAY_PLAY_START, (void *)g_playing_media_title);
            break;
        case AIRPLAY_STATUS_PAUSED:
            LOG_INFO("AirPlay: Paused");
            g_airplay_playing = false;
            
            // 更新LED状态
            led_ctrl_set_state(LED_WIFI, LED_STATE_ON);
            
            // 更新LCD显示
            lcd_display_text("AirPlay Paused", LCD_LINE_1);
            lcd_display_text(g_playing_media_title, LCD_LINE_2);
            
            // 发送AirPlay暂停事件
            event_notify(EVENT_AIRPLAY_PLAY_PAUSE, NULL);
            break;
        case AIRPLAY_STATUS_STOPPED:
            LOG_INFO("AirPlay: Stopped");
            g_airplay_playing = false;
            g_playing_media_title[0] = '\0';
            g_playing_media_artist[0] = '\0';
            
            // 更新LED状态
            led_ctrl_set_state(LED_WIFI, LED_STATE_ON);
            
            // 更新LCD显示
            lcd_display_text("AirPlay Stopped", LCD_LINE_1);
            lcd_display_text("", LCD_LINE_2);
            
            // 发送AirPlay停止事件
            event_notify(EVENT_AIRPLAY_PLAY_STOP, NULL);
            break;
        default:
            LOG_INFO("AirPlay status: %d", status);
            break;
    }
}

/**
 * @brief DLNA媒体信息回调函数
 */
static void dlna_media_info_callback(const char *title, const char *artist, const char *album) {
    if (title) {
        strncpy(g_playing_media_title, title, sizeof(g_playing_media_title) - 1);
    }
    if (artist) {
        strncpy(g_playing_media_artist, artist, sizeof(g_playing_media_artist) - 1);
    }
    
    LOG_INFO("DLNA Media Info: %s - %s", g_playing_media_artist, g_playing_media_title);
}

/**
 * @brief AirPlay媒体信息回调函数
 */
static void airplay_media_info_callback(const char *title, const char *artist, const char *album) {
    if (title) {
        strncpy(g_playing_media_title, title, sizeof(g_playing_media_title) - 1);
    }
    if (artist) {
        strncpy(g_playing_media_artist, artist, sizeof(g_playing_media_artist) - 1);
    }
    
    LOG_INFO("AirPlay Media Info: %s - %s", g_playing_media_artist, g_playing_media_title);
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
        g_current_volume = cfg->initial_volume;
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
            aml_dlna_set_media_info_callback(dlna_media_info_callback);
            aml_dlna_set_device_name(g_wifi_name);
            aml_dlna_set_volume(g_current_volume);
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
            aml_airplay_set_media_info_callback(airplay_media_info_callback);
            aml_airplay_set_device_name(g_wifi_name);
            aml_airplay_set_volume(g_current_volume);
            aml_airplay_start();
            LOG_INFO("AirPlay module init success");
        }
    }
    
    g_wifi_media_init = true;
    g_wifi_connected = false;
    g_dlna_playing = false;
    g_airplay_playing = false;
    g_current_ssid[0] = '\0';
    g_playing_media_title[0] = '\0';
    g_playing_media_artist[0] = '\0';
    
    LOG_INFO("WIFI media module init success (CAST/NET PLAY) [HIGH END ONLY]");
    LOG_INFO("  Device name: %s", g_wifi_name);
    LOG_INFO("  DLNA enabled: %s", g_dlna_enabled ? "YES" : "NO");
    LOG_INFO("  AirPlay enabled: %s", g_airplay_enabled ? "YES" : "NO");
    LOG_INFO("  Initial volume: %d", g_current_volume);
    
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
        
        // 重置LED状态
        led_ctrl_set_state(LED_WIFI, LED_STATE_OFF);
        
        g_wifi_media_init = false;
        g_wifi_connected = false;
        g_dlna_playing = false;
        g_airplay_playing = false;
        g_current_ssid[0] = '\0';
        g_playing_media_title[0] = '\0';
        g_playing_media_artist[0] = '\0';
        
        LOG_INFO("WIFI media module deinit success");
    }
    
    return 0;
}

bool wifi_media_get_connect_state(void) {
    return g_wifi_connected;
}

/**
 * @brief 获取当前播放状态
 */
bool wifi_media_is_playing(void) {
    return g_dlna_playing || g_airplay_playing;
}

/**
 * @brief 获取当前播放的媒体标题
 */
const char *wifi_media_get_playing_title(void) {
    return g_playing_media_title;
}

/**
 * @brief 设置播放音量
 */
int wifi_media_set_volume(int volume) {
    if (!g_wifi_media_init) {
        return -1;
    }
    
    if (volume >= 0 && volume <= 100) {
        g_current_volume = volume;
        
        if (g_dlna_enabled) {
            aml_dlna_set_volume(volume);
        }
        if (g_airplay_enabled) {
            aml_airplay_set_volume(volume);
        }
        
        LOG_INFO("WIFI media volume set to %d", volume);
        return 0;
    } else {
        LOG_ERROR("Invalid volume value: %d", volume);
        return -1;
    }
}

/**
 * @brief 获取当前播放音量
 */
int wifi_media_get_volume(void) {
    return g_current_volume;
}

/**
 * @brief 暂停当前播放
 */
int wifi_media_pause(void) {
    if (!g_wifi_media_init) {
        return -1;
    }
    
    int result = 0;
    
    if (g_dlna_playing && g_dlna_enabled) {
        result = aml_dlna_pause();
    } else if (g_airplay_playing && g_airplay_enabled) {
        result = aml_airplay_pause();
    }
    
    return result;
}

/**
 * @brief 继续当前播放
 */
int wifi_media_resume(void) {
    if (!g_wifi_media_init) {
        return -1;
    }
    
    int result = 0;
    
    if (g_dlna_enabled) {
        result = aml_dlna_play();
    } else if (g_airplay_enabled) {
        result = aml_airplay_play();
    }
    
    return result;
}

/**
 * @brief 停止当前播放
 */
int wifi_media_stop(void) {
    if (!g_wifi_media_init) {
        return -1;
    }
    
    int result = 0;
    
    if (g_dlna_enabled) {
        result = aml_dlna_stop();
        g_dlna_playing = false;
    }
    if (g_airplay_enabled) {
        result = aml_airplay_stop();
        g_airplay_playing = false;
    }
    
    g_playing_media_title[0] = '\0';
    g_playing_media_artist[0] = '\0';
    
    return result;
}

/**
 * @brief 重启WIFI媒体服务
 */
int wifi_media_restart(void) {
    if (!g_wifi_media_init) {
        return -1;
    }
    
    // 先停止所有服务
    wifi_media_stop();
    
    // 重启DLNA服务
    if (g_dlna_enabled) {
        aml_dlna_stop();
        aml_dlna_start();
    }
    
    // 重启AirPlay服务
    if (g_airplay_enabled) {
        aml_airplay_stop();
        aml_airplay_start();
    }
    
    LOG_INFO("WIFI media services restarted");
    return 0;
}

#endif

#ifndef CONFIG_ENABLE_WIFI_MEDIA
int wifi_media_init(WifiMediaConfig_t *cfg) { return 0; }
int wifi_media_deinit(void) { return 0; }
bool wifi_media_get_connect_state(void) { return false; }
bool wifi_media_is_playing(void) { return false; }
const char *wifi_media_get_playing_title(void) { return NULL; }
int wifi_media_set_volume(int volume) { return -1; }
int wifi_media_get_volume(void) { return 80; }
int wifi_media_pause(void) { return -1; }
int wifi_media_resume(void) { return -1; }
int wifi_media_stop(void) { return -1; }
int wifi_media_restart(void) { return -1; }
#endif