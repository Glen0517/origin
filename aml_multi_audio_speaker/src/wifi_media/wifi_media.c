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
static bool g_dlna_started = false;
static bool g_airplay_started = false;
static bool g_spotify_started = false;
static bool g_google_cast_started = false;
static bool g_dlna_playing = false;
static bool g_airplay_playing = false;
static bool g_spotify_playing = false;
static bool g_google_cast_playing = false;
static bool g_game_mode_enabled = false;
static bool g_on_demand_start_enabled = true;  // 按需启动使能
static bool g_auto_reconnect_enabled = true;  // 自动重连使能
static char g_wifi_name[32] = "Aml_Soundbar";
static char g_current_ssid[32] = {0};
static char g_playing_media_title[128] = {0};
static char g_playing_media_artist[128] = {0};
static int g_current_volume = 80;
static int g_buffer_size = 200;  // 默认缓冲大小（毫秒）
static int g_min_buffer_size = 50;  // 最小缓冲大小（毫秒）
static int g_max_buffer_size = 500;  // 最大缓冲大小（毫秒）
static int g_game_buffer_size = 30;  // 游戏模式缓冲大小（毫秒）
static int g_network_quality = 5;  // 网络质量（1-10）
static int g_buffer_adjust_interval = 2000;  // 缓冲调整间隔（毫秒）
static int g_last_buffer_adjust_time = 0;  // 上次缓冲调整时间

// 网络连接参数
static int g_reconnect_attempts = 0;  // 重连尝试次数
static int g_max_reconnect_attempts = 5;  // 最大重连尝试次数
static int g_reconnect_interval = 3;  // 重连间隔（秒）
static int g_reconnect_backoff = 1;  // 重连退避系数
static int g_min_reconnect_interval = 1;  // 最小重连间隔（秒）
static int g_max_reconnect_interval = 15;  // 最大重连间隔（秒）
static int g_last_reconnect_time = 0;  // 上次重连时间

// 网络质量检测参数
static int g_ping_count = 5;  //  ping 次数
static int g_ping_timeout = 2000;  // ping 超时（毫秒）
static int g_network_quality_history[10] = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5};  // 网络质量历史
static int g_network_quality_index = 0;  // 网络质量历史索引
static int g_last_quality_check_time = 0;  // 上次质量检查时间
static int g_quality_check_interval = 5000;  // 质量检查间隔（毫秒）

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
// 启动DLNA服务
static void start_dlna_service(void) {
    if (g_dlna_enabled && !g_dlna_started) {
        aml_dlna_start();
        g_dlna_started = true;
        LOG_INFO("DLNA service started on demand");
    }
}

// 停止DLNA服务
static void stop_dlna_service(void) {
    if (g_dlna_enabled && g_dlna_started && !g_dlna_playing) {
        aml_dlna_stop();
        g_dlna_started = false;
        LOG_INFO("DLNA service stopped (idle)");
    }
}

// 启动AirPlay服务
static void start_airplay_service(void) {
    if (g_airplay_enabled && !g_airplay_started) {
        aml_airplay_start();
        g_airplay_started = true;
        LOG_INFO("AirPlay service started on demand");
    }
}

// 停止AirPlay服务
static void stop_airplay_service(void) {
    if (g_airplay_enabled && g_airplay_started && !g_airplay_playing) {
        aml_airplay_stop();
        g_airplay_started = false;
        LOG_INFO("AirPlay service stopped (idle)");
    }
}

// 启动Spotify服务
static void start_spotify_service(void) {
    if (g_spotify_enabled && !g_spotify_started) {
        aml_spotify_start();
        g_spotify_started = true;
        LOG_INFO("Spotify service started on demand");
    }
}

// 停止Spotify服务
static void stop_spotify_service(void) {
    if (g_spotify_enabled && g_spotify_started && !g_spotify_playing) {
        aml_spotify_stop();
        g_spotify_started = false;
        LOG_INFO("Spotify service stopped (idle)");
    }
}

// 启动Google Cast服务
static void start_google_cast_service(void) {
    if (g_google_cast_enabled && !g_google_cast_started) {
        aml_google_cast_start();
        g_google_cast_started = true;
        LOG_INFO("Google Cast service started on demand");
    }
}

// 停止Google Cast服务
static void stop_google_cast_service(void) {
    if (g_google_cast_enabled && g_google_cast_started && !g_google_cast_playing) {
        aml_google_cast_stop();
        g_google_cast_started = false;
        LOG_INFO("Google Cast service stopped (idle)");
    }
}

// 网络质量检测函数
static int detect_network_quality(void) {
    if (!g_wifi_connected) {
        return 1; // 未连接时网络质量最差
    }
    
    // 实际实现中应该使用真实的网络质量检测方法，例如：
    // 1. Ping 测试 - 测试延迟和丢包率
    // 2. 带宽测试 - 测试上传和下载速度
    // 3. 信号强度测试 - 测试 WiFi 信号强度
    
    // 这里使用模拟实现
    static int quality_pattern[] = {8, 7, 9, 8, 7, 6, 8, 9, 7, 8};
    static int pattern_index = 0;
    
    // 循环使用质量模式
    int quality = quality_pattern[pattern_index];
    pattern_index = (pattern_index + 1) % sizeof(quality_pattern) / sizeof(quality_pattern[0]);
    
    // 添加一些随机波动，使模拟更真实
    int variation = rand() % 3 - 1; // -1, 0, 或 1
    quality = quality + variation;
    
    // 确保质量在有效范围内
    if (quality < 1) quality = 1;
    if (quality > 10) quality = 10;
    
    return quality;
}

// 更新网络质量历史
static void update_network_quality_history(int quality) {
    // 更新网络质量历史
    g_network_quality_history[g_network_quality_index] = quality;
    g_network_quality_index = (g_network_quality_index + 1) % 10;
    
    // 计算平均网络质量
    int sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += g_network_quality_history[i];
    }
    g_network_quality = sum / 10;
}

// 定期检查网络质量
static void check_network_quality(void) {
    if (!g_wifi_media_init) return;
    
    // 计算当前时间
    int current_time = hal_get_current_time();
    if (current_time - g_last_quality_check_time < g_quality_check_interval) return;
    
    // 检测网络质量
    int quality = detect_network_quality();
    
    // 更新网络质量历史
    update_network_quality_history(quality);
    
    // 记录网络质量
    LOG_INFO("Network quality check: %d/10 (Average: %d/10)", quality, g_network_quality);
    
    // 基于网络质量调整缓冲大小
    adjust_buffer_based_on_network();
    
    // 发送网络质量事件
    event_notify(EVENT_NETWORK_QUALITY_CHANGED, (void *)&g_network_quality);
    
    g_last_quality_check_time = current_time;
}

// 基于网络质量调整缓冲大小
static void adjust_buffer_based_on_network(void) {
    if (!g_wifi_media_init) return;
    
    // 计算当前时间
    int current_time = hal_get_current_time();
    if (current_time - g_last_buffer_adjust_time < g_buffer_adjust_interval) return;
    
    // 根据网络质量调整缓冲大小
    int new_buffer_size = g_buffer_size;
    
    if (g_network_quality >= 8) {
        // 网络质量好，减少缓冲大小，降低延迟
        new_buffer_size = g_min_buffer_size + (g_buffer_size - g_min_buffer_size) * 0.3;
    } else if (g_network_quality >= 5) {
        // 网络质量中等，使用默认缓冲大小
        new_buffer_size = g_buffer_size;
    } else if (g_network_quality >= 3) {
        // 网络质量较差，增加缓冲大小，提高稳定性
        new_buffer_size = g_buffer_size * 1.5;
        if (new_buffer_size > g_max_buffer_size) {
            new_buffer_size = g_max_buffer_size;
        }
    } else {
        // 网络质量差，使用最大缓冲大小
        new_buffer_size = g_max_buffer_size;
    }
    
    // 如果缓冲大小有变化，更新缓冲设置
    if (abs(new_buffer_size - g_buffer_size) > 10) { // 大于10毫秒的变化才更新
        g_buffer_size = new_buffer_size;
        
        if (g_dlna_enabled && g_dlna_started) {
            aml_dlna_set_buffer_size(new_buffer_size);
        }
        if (g_airplay_enabled && g_airplay_started) {
            aml_airplay_set_buffer_size(new_buffer_size);
        }
        if (g_spotify_enabled && g_spotify_started) {
            aml_spotify_set_buffer_size(new_buffer_size);
        }
        if (g_google_cast_enabled && g_google_cast_started) {
            aml_google_cast_set_buffer_size(new_buffer_size);
        }
        
        LOG_INFO("Adjusting buffer size to %d ms based on network quality (%d/10)", 
                 new_buffer_size, g_network_quality);
    }
    
    g_last_buffer_adjust_time = current_time;
}

// WiFi自动重连函数
static void wifi_auto_reconnect(void) {
    if (!g_wifi_media_init || g_wifi_connected || !g_auto_reconnect_enabled) {
        return;
    }
    
    // 计算当前时间
    int current_time = hal_get_current_time();
    if (current_time - g_last_reconnect_time < g_reconnect_interval * 1000) {
        return;
    }
    
    // 检查重连尝试次数
    if (g_reconnect_attempts >= g_max_reconnect_attempts) {
        LOG_INFO("Max reconnect attempts reached (%d), stopping auto-reconnect", g_max_reconnect_attempts);
        // 重置重连参数
        g_reconnect_attempts = 0;
        g_reconnect_backoff = 1;
        return;
    }
    
    // 计算重连间隔（带退避）
    int current_interval = g_reconnect_interval * g_reconnect_backoff;
    if (current_interval < g_min_reconnect_interval) {
        current_interval = g_min_reconnect_interval;
    } else if (current_interval > g_max_reconnect_interval) {
        current_interval = g_max_reconnect_interval;
    }
    
    // 检查是否达到重连间隔
    if (current_time - g_last_reconnect_time < current_interval * 1000) {
        return;
    }
    
    // 尝试重连
    g_reconnect_attempts++;
    g_reconnect_backoff *= 2; // 指数退避
    g_last_reconnect_time = current_time;
    
    LOG_INFO("Attempting to reconnect to WiFi... (Attempt %d/%d, Interval: %d sec)", 
             g_reconnect_attempts, g_max_reconnect_attempts, current_interval);
    
    // 实际实现中应该调用WiFi重连函数
    // 这里简化处理，模拟重连
    // aml_wifi_reconnect();
    
    // 发送重连事件
    event_notify(EVENT_WIFI_RECONNECT_ATTEMPT, (void *)&g_reconnect_attempts);
}

static void dlna_status_callback(int status) {
    // 按需启动DLNA服务
    if (g_on_demand_start_enabled) {
        start_dlna_service();
    }
    
    switch (status) {
        case DLNA_STATUS_IDLE:
            LOG_INFO("DLNA: Idle");
            g_dlna_playing = false;
            
            // 按需停止DLNA服务
            if (g_on_demand_start_enabled) {
                stop_dlna_service();
            }
            break;
        case DLNA_STATUS_PLAYING:
            LOG_INFO("DLNA: Playing");
            g_dlna_playing = true;
            g_airplay_playing = false;
            g_spotify_playing = false;
            g_google_cast_playing = false;
            
            // 更新LED状态
            led_ctrl_set_state(LED_WIFI, LED_STATE_BLINK_SLOW);
            
            // 更新LCD显示
            lcd_display_text("DLNA Playing", LCD_LINE_1);
            lcd_display_text(g_playing_media_title, LCD_LINE_2);
            
            // 基于网络质量调整缓冲大小
            adjust_buffer_based_on_network();
            
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
            
            // 按需停止DLNA服务
            if (g_on_demand_start_enabled) {
                stop_dlna_service();
            }
            
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
    // 按需启动AirPlay服务
    if (g_on_demand_start_enabled) {
        start_airplay_service();
    }
    
    switch (status) {
        case AIRPLAY_STATUS_IDLE:
            LOG_INFO("AirPlay: Idle");
            g_airplay_playing = false;
            
            // 按需停止AirPlay服务
            if (g_on_demand_start_enabled) {
                stop_airplay_service();
            }
            break;
        case AIRPLAY_STATUS_PLAYING:
            LOG_INFO("AirPlay: Playing");
            g_airplay_playing = true;
            g_dlna_playing = false;
            g_spotify_playing = false;
            g_google_cast_playing = false;
            
            // 更新LED状态
            led_ctrl_set_state(LED_WIFI, LED_STATE_BLINK_SLOW);
            
            // 更新LCD显示
            lcd_display_text("AirPlay Playing", LCD_LINE_1);
            lcd_display_text(g_playing_media_title, LCD_LINE_2);
            
            // 基于网络质量调整缓冲大小
            adjust_buffer_based_on_network();
            
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
            
            // 按需停止AirPlay服务
            if (g_on_demand_start_enabled) {
                stop_airplay_service();
            }
            
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
    // 按需启动Spotify服务
    if (g_on_demand_start_enabled) {
        start_spotify_service();
    }
    
    switch (status) {
        case SPOTIFY_STATUS_IDLE:
            LOG_INFO("Spotify: Idle");
            g_spotify_playing = false;
            
            // 按需停止Spotify服务
            if (g_on_demand_start_enabled) {
                stop_spotify_service();
            }
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
            
            // 基于网络质量调整缓冲大小
            adjust_buffer_based_on_network();
            
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
            
            // 按需停止Spotify服务
            if (g_on_demand_start_enabled) {
                stop_spotify_service();
            }
            
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
    // 按需启动Google Cast服务
    if (g_on_demand_start_enabled) {
        start_google_cast_service();
    }
    
    switch (status) {
        case GOOGLE_CAST_STATUS_IDLE:
            LOG_INFO("Google Cast: Idle");
            g_google_cast_playing = false;
            
            // 按需停止Google Cast服务
            if (g_on_demand_start_enabled) {
                stop_google_cast_service();
            }
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
            
            // 基于网络质量调整缓冲大小
            adjust_buffer_based_on_network();
            
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
            
            // 按需停止Google Cast服务
            if (g_on_demand_start_enabled) {
                stop_google_cast_service();
            }
            
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
            
            // 按需启动
            if (!g_on_demand_start_enabled) {
                aml_dlna_start();
                g_dlna_started = true;
                LOG_INFO("DLNA module started");
            }
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
            
            // 按需启动
            if (!g_on_demand_start_enabled) {
                aml_airplay_start();
                g_airplay_started = true;
                LOG_INFO("AirPlay module started");
            }
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
            
            // 按需启动
            if (!g_on_demand_start_enabled) {
                aml_spotify_start();
                g_spotify_started = true;
                LOG_INFO("Spotify module started");
            }
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
            
            // 按需启动
            if (!g_on_demand_start_enabled) {
                aml_google_cast_start();
                g_google_cast_started = true;
                LOG_INFO("Google Cast module started");
            }
            LOG_INFO("Google Cast module init success");
        }
    }
    
    g_wifi_media_init = true;
    g_wifi_connected = false;
    g_dlna_started = false;
    g_airplay_started = false;
    g_spotify_started = false;
    g_google_cast_started = false;
    g_dlna_playing = false;
    g_airplay_playing = false;
    g_spotify_playing = false;
    g_google_cast_playing = false;
    g_game_mode_enabled = false;
    g_current_ssid[0] = '\0';
    g_playing_media_title[0] = '\0';
    g_playing_media_artist[0] = '\0';
    
    // 初始化网络质量检测和自动重连参数
    memset(g_network_quality_history, 5, sizeof(g_network_quality_history));
    g_network_quality_index = 0;
    g_network_quality = 5;
    g_reconnect_attempts = 0;
    g_reconnect_backoff = 1;
    g_last_reconnect_time = 0;
    g_last_quality_check_time = 0;
    
    LOG_INFO("WIFI media module init success (CAST/NET PLAY) [HIGH END ONLY]");
    LOG_INFO("  On-demand start enabled: %s", g_on_demand_start_enabled ? "YES" : "NO");
    LOG_INFO("  Auto-reconnect enabled: %s", g_auto_reconnect_enabled ? "YES" : "NO");
    LOG_INFO("  Buffer size: %d ms (min: %d, max: %d)", 
             g_buffer_size, g_min_buffer_size, g_max_buffer_size);
    LOG_INFO("  Network quality check interval: %d ms", g_quality_check_interval);
    LOG_INFO("  Reconnect attempts: %d, Max: %d, Interval: %d sec", 
             g_reconnect_attempts, g_max_reconnect_attempts, g_reconnect_interval);
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
    
    // 游戏模式：减小缓冲大小，降低延迟
    int new_buffer_size = enable ? WIFI_GAME_BUFFER_SIZE : WIFI_DEFAULT_BUFFER_SIZE;
    wifi_media_set_buffer_size(new_buffer_size);
    
    LOG_INFO("WIFI media game mode %s (buffer size: %d ms)", enable ? "enabled" : "disabled", new_buffer_size);
    
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

/**
 * @brief 获取WiFi媒体状态
 * @return 状态：1-已连接，0-未连接
 */
int wifi_media_get_status(void) {
    return g_wifi_media_init && g_wifi_connected ? 1 : 0;
}

#endif

// WiFi媒体服务事件轮询函数
void wifi_media_event_poll(void) {
    if (!g_wifi_media_init) return;
    
    // 检查网络质量并调整缓冲
    check_network_quality();
    
    // 处理自动重连
    wifi_auto_reconnect();
}

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
int wifi_media_get_status(void) { return false; }
#endif