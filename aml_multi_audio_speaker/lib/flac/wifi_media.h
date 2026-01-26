#ifndef __WIFI_MEDIA_H__
#define __WIFI_MEDIA_H__

#include "common_def.h"
#include "product_type.h"

// 无论是否启用WiFi媒体，都定义配置结构体，因为外部声明需要使用
typedef struct {
    char wifi_name[32];        // WiFi设备名称
    bool dlna_en;              // 是否启用DLNA
    bool airplay_en;           // 是否启用AirPlay
    bool spotify_en;           // 是否启用Spotify
    bool google_cast_en;       // 是否启用Google Cast
    int initial_volume;        // 初始音量
} WifiMediaConfig_t;

#if CONFIG_ENABLE_WIFI_MEDIA

/**
 * @brief 初始化WiFi媒体模块
 * @param cfg WiFi媒体配置结构体指针
 * @return 初始化结果：0表示成功，非0表示失败
 */
int wifi_media_init(WifiMediaConfig_t *cfg);

/**
 * @brief 反初始化WiFi媒体模块
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int wifi_media_deinit(void);

/**
 * @brief 获取WiFi连接状态
 * @return 连接状态：true表示已连接，false表示未连接
 */
bool wifi_media_get_connect_state(void);

/**
 * @brief 获取当前播放状态
 * @return 播放状态：true表示正在播放，false表示未播放
 */
bool wifi_media_is_playing(void);

/**
 * @brief 获取当前播放的媒体标题
 * @return 媒体标题字符串
 */
const char *wifi_media_get_playing_title(void);

/**
 * @brief 设置播放音量
 * @param volume 音量值（0-100）
 * @return 设置结果：0表示成功，非0表示失败
 */
int wifi_media_set_volume(int volume);

/**
 * @brief 获取当前播放音量
 * @return 音量值（0-100）
 */
int wifi_media_get_volume(void);

/**
 * @brief 暂停当前播放
 * @return 暂停结果：0表示成功，非0表示失败
 */
int wifi_media_pause(void);

/**
 * @brief 继续当前播放
 * @return 继续结果：0表示成功，非0表示失败
 */
int wifi_media_resume(void);

/**
 * @brief 停止当前播放
 * @return 停止结果：0表示成功，非0表示失败
 */
int wifi_media_stop(void);

/**
 * @brief 重启WiFi媒体服务
 * @return 重启结果：0表示成功，非0表示失败
 */
int wifi_media_restart(void);

/**
 * @brief 设置WiFi媒体缓冲大小
 * @param buffer_size 缓冲大小（毫秒）
 * @return 设置结果：0表示成功，非0表示失败
 */
int wifi_media_set_buffer_size(int buffer_size);

/**
 * @brief 获取当前WiFi媒体缓冲大小
 * @return 缓冲大小（毫秒）
 */
int wifi_media_get_buffer_size(void);

/**
 * @brief 启用游戏模式
 * @param enable 是否启用游戏模式
 * @return 操作结果：0表示成功，非0表示失败
 */
int wifi_media_enable_game_mode(bool enable);

/**
 * @brief 检查是否启用了游戏模式
 * @return 游戏模式状态：true表示已启用，false表示未启用
 */
bool wifi_media_is_game_mode_enabled(void);

/**
 * @brief 获取当前WiFi SSID
 * @param ssid SSID缓冲区
 * @param len 缓冲区长度
 * @return 获取结果：0表示成功，非0表示失败
 */
int wifi_media_get_current_ssid(char *ssid, int len);

/**
 * @brief 获取Spotify连接状态
 * @return 连接状态：true表示已连接，false表示未连接
 */
bool wifi_media_get_spotify_state(void);

/**
 * @brief 获取Google Cast连接状态
 * @return 连接状态：true表示已连接，false表示未连接
 */
bool wifi_media_get_google_cast_state(void);

/**
 * @brief 获取WiFi媒体状态
 * @return 状态：1表示已连接，0表示未连接
 */
int wifi_media_get_status(void);

#endif // CONFIG_ENABLE_WIFI_MEDIA

// 非WiFi版本的空实现
extern int wifi_media_init(WifiMediaConfig_t *cfg);
extern int wifi_media_deinit(void);
extern bool wifi_media_get_connect_state(void);
extern bool wifi_media_is_playing(void);
extern const char *wifi_media_get_playing_title(void);
extern int wifi_media_set_volume(int volume);
extern int wifi_media_get_volume(void);
extern int wifi_media_pause(void);
extern int wifi_media_resume(void);
extern int wifi_media_stop(void);
extern int wifi_media_restart(void);
extern int wifi_media_set_buffer_size(int buffer_size);
extern int wifi_media_get_buffer_size(void);
extern int wifi_media_enable_game_mode(bool enable);
extern bool wifi_media_is_game_mode_enabled(void);
extern int wifi_media_get_current_ssid(char *ssid, int len);
extern bool wifi_media_get_spotify_state(void);
extern bool wifi_media_get_google_cast_state(void);
extern int wifi_media_get_status(void);

#endif // __WIFI_MEDIA_H__