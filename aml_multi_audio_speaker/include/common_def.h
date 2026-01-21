#ifndef __COMMON_DEF_H__
#define __COMMON_DEF_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "product_type.h"

/******************************************************************************************
 * 【全局通用宏定义】- Linux嵌入式工业级标准，无冗余
 ******************************************************************************************/
#define TRUE                        1
#define FALSE                       0
#define NULL_PTR                    ((void *)0)
#define SUCCESS                     0
#define FAILURE                     -1
#define INVALID_PARAM               -2
#define NOT_SUPPORT                 -3
#define DEVICE_BUSY                 -4
#define READ_WRITE_ERR              -5

#define MAX_VOLUME_VAL              30      // 最大音量值
#define MIN_VOLUME_VAL              0       // 最小音量值
#define DEFAULT_VOLUME_VAL          15      // 默认音量值
#define VOLUME_STEP                 1       // 音量步进值

#define KEY_DEBOUNCE_TIME_MS        20      // 按键防抖时间(工业级最优值)
#define LONG_PRESS_TIME_MS          800     // 长按判定时间
#define BT_RECONNECT_TIMEOUT_S      10      // 蓝牙重连超时时间

#define JSON_CONFIG_PATH            "/etc/aml_soundbar/config/" // 量产配置路径
#define DEV_CONFIG_PATH             "./config/"                 // 开发配置路径
#define FLASH_CALIB_ADDR            0x000E0000                  // FLASH校准分区地址
#define SYSTEM_VERSION              "1.0.0"                    // 系统版本号

/******************************************************************************************
 * 【全局枚举定义】- 所有模块共用，统一规范，无零散枚举
 ******************************************************************************************/
// 音源类型枚举
typedef enum {
    SOURCE_NONE = 0,
    SOURCE_BLUETOOTH,
    SOURCE_USB,
    SOURCE_HDMI_ARC,
    SOURCE_SPDIF,
    SOURCE_AUX,
    SOURCE_WIFI,
    SOURCE_BT_SUBWOOFER  // 低音炮专属蓝牙音源
} AudioSourceType_e;

// 播放状态枚举
typedef enum {
    PLAY_STATE_IDLE = 0,
    PLAY_STATE_PLAYING,
    PLAY_STATE_PAUSE,
    PLAY_STATE_STOP,
    PLAY_STATE_ERROR
} PlayState_e;

// 声场模式枚举
typedef enum {
    SOUND_MODE_NORMAL = 0,
    SOUND_MODE_CINEMA,
    SOUND_MODE_MUSIC,
    SOUND_MODE_GAME,
    SOUND_MODE_NEWS,
    SOUND_MODE_BASS_ONLY // 低音炮专属
} SoundMode_e;

// 系统状态枚举
typedef enum {
    SYS_STATE_IDLE = 0,
    SYS_STATE_WORKING,
    SYS_STATE_OTA,
    SYS_STATE_CALIB,
    SYS_STATE_AGE_TEST,
    SYS_STATE_ERROR,
    SYS_STATE_STANDBY    // 待机状态
} SysState_e;

// LED状态枚举
typedef enum {
    LED_STATE_OFF = 0,
    LED_STATE_ON,
    LED_STATE_FLASH_SLOW,
    LED_STATE_FLASH_FAST,
    LED_STATE_BREATH
} LedState_e;

// LED状态兼容别名
#define LED_STATE_BLINK        LED_STATE_FLASH_SLOW
#define LED_STATE_BLINK_SLOW    LED_STATE_FLASH_SLOW
#define LED_STATE_BLINK_FAST    LED_STATE_FLASH_FAST

// LED索引常量定义
#define LED_BLUETOOTH     0   // 蓝牙LED
#define LED_PLAY         1   // 播放状态LED
#define LED_SYSTEM       2   // 系统LED
#define LED_SOURCE       3   // 音源LED
#define LED_VOLUME       4   // 音量LED

/******************************************************************************************
 * 【全局结构体定义】- 通用结构体，所有模块共用
 ******************************************************************************************/
// 音量结构体
typedef struct {
    int master_volume;  // 主音量
    int bass_volume;    // 低音音量
    int treble_volume;  // 高音音量
    bool is_mute;       // 是否静音
} VolumeInfo_t;

// 系统状态结构体
typedef struct {
    SysState_e sys_state;
    AudioSourceType_e curr_source;
    PlayState_e play_state;
    SoundMode_e sound_mode;
    char bt_dev_name[32]; // 当前蓝牙连接设备名
} SysStatus_t;

/******************************************************************************************
 * 【日志宏定义】- 使用aml_log.h中的方式
 ******************************************************************************************/
#include "log/aml_log.h"

// 声明默认日志分类
AML_LOG_EXTERN(default_log);
#define AML_LOG_DEFAULT AML_LOG_GET_CAT(default_log)

// 兼容原有日志宏，保持向后兼容
#define LOG_DEBUG(fmt, ...) AML_LOGD(fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  AML_LOGI(fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  AML_LOGW(fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) AML_LOGE(fmt, ##__VA_ARGS__)

/******************************************************************************************
 * 【线程锁宏定义】- 通用线程安全锁，所有模块共用
 ******************************************************************************************/
#define MUTEX_LOCK_INIT(mutex)      pthread_mutex_init(&mutex, NULL)
#define MUTEX_LOCK_LOCK(mutex)      pthread_mutex_lock(&mutex)
#define MUTEX_LOCK_UNLOCK(mutex)    pthread_mutex_unlock(&mutex)
#define MUTEX_LOCK_DESTROY(mutex)   pthread_mutex_destroy(&mutex)

#endif // __COMMON_DEF_H__