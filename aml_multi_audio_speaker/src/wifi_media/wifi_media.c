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
#include <aml_spotify.h>     // 晶晨Spotify SDK
#include <aml_google_cast.h> // 晶晨Google Cast SDK

#ifdef CONFIG_ENABLE_WIFI_MEDIA

static bool g_wifi_media_init = false;
static bool g_wifi_connected = false;
static bool g_dlna_enabled = true;
static bool g_airplay_enabled = true;
static bool g_spotify_enabled = true;
static bool g_google_cast_enabled = true;
static bool g_dlna_playing = false;
static bool g_airplay_playing = false;
static bool g_spotify_playing = false;
static bool g_google_cast_playing = false;
static bool g_game_mode_enabled = false;
static char g_wifi_name[32] = "Aml_Soundbar";
static char g_current_ssid[32] = {0};
static char g_playing_media_title[128] = {0};
static char g_playing_media_artist[128] = {0};
static int g_current_volume = 80;
static int g_buffer_size = 200;  // 默认缓冲大小（毫秒）
static int g_game_buffer_size = 30;  // 游戏模式缓冲大小（毫秒）

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

/**
 * @brief Spotify状态回调函数
 */
static void spotify_status_callback(int status) {
    switch (status) {
        case SPOTIFY_STATUS_IDLE:
            LOG_INFO("Spotify: Idle");
            g_spotify_playing = false;
            break;
        case SPOTIFY_STATUS_PLAYING:
            LOG_INFO("Spotify: Playing");
            g_spotify_playing = true;
            g_dlna_playing = false;
            g_airplay_playing = false;
            g_google_cast_playing = false;
            
            // 更新LED状态
            led_ctrl_set_state(LED_WIFI, LED_STATE_BLINK_SLOW);
            
            // 更新LCD显示
            lcd_display_text("Spotify Playing", LCD_LINE_1);
            lcd_display_text(g_playing_media_title, LCD_LINE_2);
            
            // 发送Spotify播放事件
            event_notify(EVENT_SPOTIFY_PLAY_START, (void *)g_playing_media_title);
            break;
        case SPOTIFY_STATUS_PAUSED:
            LOG_INFO("Spotify: Paused");
            g_spotify_playing = false;
            
            // 更新LED状态
            led_ctrl_set_state(LED_WIFI, LED_STATE_ON);
            
            // 更新LCD显示
            lcd_display_text("Spotify Paused", LCD_LINE_1);
            lcd_display_text(g_playing_media_title, LCD_LINE_2);
            
            // 发送Spotify暂停事件
            event_notify(EVENT_SPOTIFY_PLAY_PAUSE, NULL);
            break;
        case SPOTIFY_STATUS_STOPPED:
            LOG_INFO("Spotify: Stopped");
            g_spotify_playing = false;
            g_playing_media_title[0] = '\0';
            g_playing_media_artist[0] = '\0';
            
            // 更新LED状态
            led_ctrl_set_state(LED_WIFI, LED_STATE_ON);
            
            // 更新LCD显示
            lcd_display_text("Spotify Stopped", LCD_LINE_1);
            lcd_display_text("", LCD_LINE_2);
            
            // 发送Spotify停止事件
            event_notify(EVENT_SPOTIFY_PLAY_STOP, NULL);
            break;
        default:
            LOG_INFO("Spotify status: %d", status);
            break;
    }
}

/**
 * @brief Spotify媒体信息回调函数
 */
static void spotify_media_info_callback(const char *title, const char *artist, const char *album) {
    if (title) {
        strncpy(g_playing_media_title, title, sizeof(g_playing_media_title) - 1);
    }
    if (artist) {
        strncpy(g_playing_media_artist, artist, sizeof(g_playing_media_artist) - 1);
    }
    
    LOG_INFO("Spotify Media Info: %s - %s", g_playing_media_artist, g_playing_media_title);
}

/**
 * @brief Google Cast状态回调函数
 */
static void google_cast_status_callback(int status) {
    switch (status) {
        case GOOGLE_CAST_STATUS_IDLE:
            LOG_INFO("Google Cast: Idle");
            g_google_cast_playing = false;
            break;
        case GOOGLE_CAST_STATUS_PLAYING:
            LOG_INFO("Google Cast: Playing");
            g_google_cast_playing = true;
            g_dlna_playing = false;
            g_airplay_playing = false;
            g_spotify_playing = false;
            
            // 更新LED状态
            led_ctrl_set_state(LED_WIFI, LED_STATE_BLINK_SLOW);
            
            // 更新LCD显示
            lcd_display_text("Google Cast Playing", LCD_LINE_1);
            lcd_display_text(g_playing_media_title, LCD_LINE_2);
            
            // 发送Google Cast播放事件
            event_notify(EVENT_GOOGLE_CAST_PLAY_START, (void *)g_playing_media_title);
            break;
        case GOOGLE_CAST_STATUS_PAUSED:
            LOG_INFO("Google Cast: Paused");
            g_google_cast_playing = false;
            
            // 更新LED状态
            led_ctrl_set_state(LED_WIFI, LED_STATE_ON);
            
            // 更新LCD显示
            lcd_display_text("Google Cast Paused", LCD_LINE_1);
            lcd_display_text(g_playing_media_title, LCD_LINE_2);
            
            // 发送Google Cast暂停事件
            event_notify(EVENT_GOOGLE_CAST_PLAY_PAUSE, NULL);
            break;
        case GOOGLE_CAST_STATUS_STOPPED:
            LOG_INFO("Google Cast: Stopped");
            g_google_cast_playing = false;
            g_playing_media_title[0] = '\0';
            g_playing_media_artist[0] = '\0';
            
            // 更新LED状态
            led_ctrl_set_state(LED_WIFI, LED_STATE_ON);
            
            // 更新LCD显示
            lcd_display_text("Google Cast Stopped", LCD_LINE_1);
            lcd_display_text("", LCD_LINE_2);
            
            // 发送Google Cast停止事件
            event_notify(EVENT_GOOGLE_CAST_PLAY_STOP, NULL);
            break;
        default:
            LOG_INFO("Google Cast status: %d", status);
            break;
    }
}

/**
 * @brief Google Cast媒体信息回调函数
 */
static void google_cast_media_info_callback(const char *title, const char *artist, const char *album) {
    if (title) {
        strncpy(g_playing_media_title, title, sizeof(g_playing_media_title) - 1);
    }
    if (artist) {
        strncpy(g_playing_media_artist, artist, sizeof(g_playing_media_artist) - 1);
    }
    
    LOG_INFO("Google Cast Media Info: %s - %s", g_playing_media_artist, g_playing_media_title);
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
        g_spotify_enabled = cfg->spotify_en;
        g_google_cast_enabled = cfg->google_cast_en;
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
            aml_dlna_set_buffer_size(g_buffer_size);
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
            aml_airplay_set_buffer_size(g_buffer_size);
            aml_airplay_start();
            LOG_INFO("AirPlay module init success");
        }
    }
    
    // 初始化Spotify（如果启用）
    if (g_spotify_enabled) {
        if (aml_spotify_init() != 0) {
            LOG_ERROR("WIFI media init failed: Spotify SDK init error");
        } else {
            aml_spotify_set_status_callback(spotify_status_callback);
            aml_spotify_set_media_info_callback(spotify_media_info_callback);
            aml_spotify_set_device_name(g_wifi_name);
            aml_spotify_set_volume(g_current_volume);
            aml_spotify_set_buffer_size(g_buffer_size);
            aml_spotify_start();
            LOG_INFO("Spotify module init success");
        }
    }
    
    // 初始化Google Cast（如果启用）
    if (g_google_cast_enabled) {
        if (aml_google_cast_init() != 0) {
            LOG_ERROR("WIFI media init failed: Google Cast SDK init error");
        } else {
            aml_google_cast_set_status_callback(google_cast_status_callback);
            aml_google_cast_set_media_info_callback(google_cast_media_info_callback);
            aml_google_cast_set_device_name(g_wifi_name);
            aml_google_cast_set_volume(g_current_volume);
            aml_google_cast_set_buffer_size(g_buffer_size);
            aml_google_cast_start();
            LOG_INFO("Google Cast module init success");
        }
    }
    
    g_wifi_media_init = true;
    g_wifi_connected = false;
    g_dlna_playing = false;
    g_airplay_playing = false;
    g_spotify_playing = false;
    g_google_cast_playing = false;
    g_game_mode_enabled = false;
    g_current_ssid[0] = '\0';
    g_playing_media_title[0] = '\0';
    g_playing_media_artist[0] = '\0';
    
    LOG_INFO("WIFI media module init success (CAST/NET PLAY) [HIGH END ONLY]");
    LOG_INFO("  Device name: %s", g_wifi_name);
    LOG_INFO("  DLNA enabled: %s", g_dlna_enabled ? "YES" : "NO");
    LOG_INFO("  AirPlay enabled: %s", g_airplay_enabled ? "YES" : "NO");
    LOG_INFO("  Spotify enabled: %s", g_spotify_enabled ? "YES" : "NO");
    LOG_INFO("  Google Cast enabled: %s", g_google_cast_enabled ? "YES" : "NO");
    LOG_INFO("  Initial volume: %d", g_current_volume);
    LOG_INFO("  Buffer size: %d ms", g_buffer_size);
    
    return 0;
}

int wifi_media_deinit(void) {
    if (g_wifi_media_init) {
        // 停止Google Cast（如果启用）
        if (g_google_cast_enabled) {
            aml_google_cast_stop();
            aml_google_cast_deinit();
        }
        
        // 停止Spotify（如果启用）
        if (g_spotify_enabled) {
            aml_spotify_stop();
            aml_spotify_deinit();
        }
        
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
        g_spotify_playing = false;
        g_google_cast_playing = false;
        g_game_mode_enabled = false;
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
    return g_dlna_playing || g_airplay_playing || g_spotify_playing || g_google_cast_playing;
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
        if (g_spotify_enabled) {
            aml_spotify_set_volume(volume);
        }
        if (g_google_cast_enabled) {
            aml_google_cast_set_volume(volume);
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
    
    if (g_google_cast_playing && g_google_cast_enabled) {
        result = aml_google_cast_pause();
    } else if (g_spotify_playing && g_spotify_enabled) {
        result = aml_spotify_pause();
    } else if (g_dlna_playing && g_dlna_enabled) {
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
    
    if (g_google_cast_enabled) {
        result = aml_google_cast_play();
    } else if (g_spotify_enabled) {
        result = aml_spotify_play();
    } else if (g_dlna_enabled) {
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
    
    if (g_google_cast_enabled) {
        result = aml_google_cast_stop();
        g_google_cast_playing = false;
    }
    if (g_spotify_enabled) {
        result = aml_spotify_stop();
        g_spotify_playing = false;
    }
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
    
    // 重启Google Cast服务
    if (g_google_cast_enabled) {
        aml_google_cast_stop();
        aml_google_cast_start();
    }
    
    // 重启Spotify服务
    if (g_spotify_enabled) {
        aml_spotify_stop();
        aml_spotify_start();
    }
    
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

/**
 * @brief 设置WiFi媒体缓冲大小
 * @param buffer_size 缓冲大小（毫秒）
 * @return 设置结果：0表示成功，非0表示失败
 */
int wifi_media_set_buffer_size(int buffer_size) {
    if (!g_wifi_media_init) {
        return -1;
    }
    
    if (buffer_size >= 0) {
        g_buffer_size = buffer_size;
        
        if (g_dlna_enabled) {
            aml_dlna_set_buffer_size(buffer_size);
        }
        if (g_airplay_enabled) {
            aml_airplay_set_buffer_size(buffer_size);
        }
        if (g_spotify_enabled) {
            aml_spotify_set_buffer_size(buffer_size);
        }
        if (g_google_cast_enabled) {
            aml_google_cast_set_buffer_size(buffer_size);
        }
        
        LOG_INFO("WIFI media buffer size set to %d ms", buffer_size);
        return 0;
    } else {
        LOG_ERROR("Invalid buffer size: %d", buffer_size);
        return -1;
    }
}

/**
 * @brief 获取当前WiFi媒体缓冲大小
 * @return 缓冲大小（毫秒）
 */
int wifi_media_get_buffer_size(void) {
    return g_buffer_size;
}

/**
 * @brief 启用游戏模式
 * @param enable 是否启用游戏模式
 * @return 操作结果：0表示成功，非0表示失败
 */
int wifi_media_enable_game_mode(bool enable) {
    if (!g_wifi_media_init) {
        return -1;
    }
    
    g_game_mode_enabled = enable;
    
    if (enable) {
        // 游戏模式：减小缓冲大小，降低延迟
        wifi_media_set_buffer_size(g_game_buffer_size);
        LOG_INFO("WIFI media game mode enabled (buffer size: %d ms)", g_game_buffer_size);
    } else {
        // 正常模式：恢复默认缓冲大小
        wifi_media_set_buffer_size(200);
        LOG_INFO("WIFI media game mode disabled (buffer size: 200 ms)");
    }
    
    // 发送游戏模式切换事件
    event_notify(enable ? EVENT_GAME_MODE_ENABLED : EVENT_GAME_MODE_DISABLED, NULL);
    
    return 0;
}

/**
 * @brief 检查是否启用了游戏模式
 * @return 游戏模式状态：true表示已启用，false表示未启用
 */
bool wifi_media_is_game_mode_enabled(void) {
    return g_game_mode_enabled;
}

/**
 * @brief 获取当前WiFi SSID
 * @param ssid SSID缓冲区
 * @param len 缓冲区长度
 * @return 获取结果：0表示成功，非0表示失败
 */
int wifi_media_get_current_ssid(char *ssid, int len) {
    if (!g_wifi_media_init) {
        return -1;
    }
    
    if (!ssid || len <= 0) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    strncpy(ssid, g_current_ssid, len - 1);
    return 0;
}

/**
 * @brief 获取Spotify连接状态
 * @return 连接状态：true表示已连接，false表示未连接
 */
bool wifi_media_get_spotify_state(void) {
    return g_spotify_enabled && g_wifi_connected;
}

/**
 * @brief 获取Google Cast连接状态
 * @return 连接状态：true表示已连接，false表示未连接
 */
bool wifi_media_get_google_cast_state(void) {
    return g_google_cast_enabled && g_wifi_connected;
}

#endif

#ifndef CONFIG_ENABLE_WIFI_MEDIA
int wifi_media_init(void *cfg) { return 0; }
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
int wifi_media_set_buffer_size(int buffer_size) { return -1; }
int wifi_media_get_buffer_size(void) { return 200; }
int wifi_media_enable_game_mode(bool enable) { return -1; }
bool wifi_media_is_game_mode_enabled(void) { return false; }
int wifi_media_get_current_ssid(char *ssid, int len) { return -1; }
bool wifi_media_get_spotify_state(void) { return false; }
bool wifi_media_get_google_cast_state(void) { return false; }
#endif