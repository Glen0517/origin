/**
 * @file event.h
 * @brief 应用层事件系统接口
 * @details 实现应用层模块间的事件广播与解耦，提供线程安全的事件通信机制
 * @author AML Audio Team
 * @date 2026-01-15
 */

#ifndef __EVENT_H__
#define __EVENT_H__

#include <stdint.h>
#include <stdbool.h>
#include "common_def.h"

/******************************************************************************************
 * 事件系统配置
 ******************************************************************************************/
#define MAX_EVENT_TYPE           1000    // 支持的最大事件类型数
#define MAX_SUBSCRIBERS_PER_EVENT 10      // 每个事件类型支持的最大订阅者数
#define MAX_EVENT_QUEUE_SIZE     50      // 事件队列最大长度

/******************************************************************************************
 * 事件类型定义 - 全局唯一
 ******************************************************************************************/
// 系统事件（1-100）
#define EVENT_SYSTEM_WORKING           1   // 系统进入工作状态
#define EVENT_SYSTEM_OTA_START         2   // OTA升级开始
#define EVENT_SYSTEM_CALIB_START       3   // 校准开始
#define EVENT_SYSTEM_AGE_TEST_START    4   // 老化测试开始
#define EVENT_SYSTEM_ERROR             5   // 系统错误
#define EVENT_SYSTEM_STATUS_UPDATE     6   // 系统状态更新
#define EVENT_SYSTEM_ERROR_LOG_CHECK   7   // 系统错误日志检查
#define EVENT_SYSTEM_STANDBY           8   // 系统进入待机状态

// USB相关事件（101-200）
#define EVENT_USB_MOUNTED              101 // USB挂载成功
#define EVENT_USB_UNMOUNTED            102 // USB卸载

// 音频源事件（201-300）
#define EVENT_SOURCE_HDMI_CONNECTED    201 // HDMI ARC连接
#define EVENT_SOURCE_HDMI_DISCONNECTED 202 // HDMI ARC断开
#define EVENT_SOURCE_SPDIF_CONNECTED   203 // SPDIF连接
#define EVENT_SOURCE_SPDIF_DISCONNECTED 204 // SPDIF断开
#define EVENT_SOURCE_BT_CONNECTED      205 // 蓝牙连接
#define EVENT_SOURCE_BT_DISCONNECTED   206 // 蓝牙断开
#define EVENT_SOURCE_USB_CONNECTED     207 // USB连接
#define EVENT_SOURCE_USB_DISCONNECTED  208 // USB断开
#define EVENT_SOURCE_AUX_CONNECTED     209 // AUX连接
#define EVENT_SOURCE_AUX_DISCONNECTED  210 // AUX断开
#define EVENT_SOURCE_UAC_CONNECTED     211 // UAC连接
#define EVENT_SOURCE_UAC_DISCONNECTED  212 // UAC断开
#define EVENT_SOURCE_WIFI_CONNECTED    213 // WiFi连接
#define EVENT_SOURCE_WIFI_DISCONNECTED 214 // WiFi断开

// 播放控制事件（301-400）
#define EVENT_PLAY_START               301 // 播放开始
#define EVENT_PLAY_PAUSE               302 // 播放暂停
#define EVENT_PLAY_STOP                303 // 播放停止
#define EVENT_PLAY_ERROR               304 // 播放错误
#define EVENT_EQ_CHANGED               305 // EQ模式改变
#define EVENT_SOUND_FIELD_CHANGED      306 // 声场模式改变
#define EVENT_BUFFER_ERROR             307 // 缓冲错误
#define EVENT_BUFFERING                308 // 正在缓冲
#define EVENT_BUFFER_COMPLETE          309 // 缓冲完成

// 音频错误事件（310-320）
#define EVENT_AUDIO_ERROR              310 // 音频错误
#define EVENT_AUDIO_ERROR_RECOVERED    311 // 音频错误恢复

// 错误处理事件（321-330）
#define EVENT_ERROR_OCCURRED           321 // 错误发生

// 蓝牙事件（401-500）
#define EVENT_BT_CONNECTED             401 // 蓝牙连接
#define EVENT_BT_DISCONNECTED          402 // 蓝牙断开
#define EVENT_BT_PLAY_START            403 // 蓝牙播放开始
#define EVENT_BT_PLAY_PAUSE            404 // 蓝牙播放暂停
#define EVENT_BT_PLAY_STOP             405 // 蓝牙播放停止
#define EVENT_BT_ERROR                 406 // 蓝牙错误
#define EVENT_BT_VOLUME_CHANGED        407 // 蓝牙音量改变
#define EVENT_BT_MEDIA_START           408 // 蓝牙媒体开始
#define EVENT_BT_MEDIA_STOP            409 // 蓝牙媒体停止

// WiFi媒体事件（501-600）
#define EVENT_WIFI_CONNECTED           501 // WiFi连接
#define EVENT_WIFI_DISCONNECTED        502 // WiFi断开
#define EVENT_DLNA_PLAY_START          503 // DLNA播放开始
#define EVENT_DLNA_PLAY_PAUSE          504 // DLNA播放暂停
#define EVENT_DLNA_PLAY_STOP           505 // DLNA播放停止
#define EVENT_AIRPLAY_PLAY_START       506 // AirPlay播放开始
#define EVENT_AIRPLAY_PLAY_PAUSE       507 // AirPlay播放暂停
#define EVENT_AIRPLAY_PLAY_STOP        508 // AirPlay播放停止

// 文件播放事件（601-700）
#define EVENT_FILE_PLAY_START          601 // 文件播放开始
#define EVENT_FILE_PLAY_ERROR          602 // 文件播放错误
#define EVENT_FILE_PLAY_PAUSE          603 // 文件播放暂停
#define EVENT_FILE_PLAY_RESUME         604 // 文件播放恢复
#define EVENT_FILE_PLAY_STOP           605 // 文件播放停止

// 媒体扫描事件（701-800）
#define EVENT_MEDIA_SCAN_ERROR         701 // 媒体扫描错误
#define EVENT_MEDIA_SCAN_COMPLETE      702 // 媒体扫描完成

// UAC事件（801-900）
#define EVENT_UAC_CONNECTED            801 // UAC连接
#define EVENT_UAC_DISCONNECTED         802 // UAC断开

/******************************************************************************************
 * 事件回调函数类型定义
 ******************************************************************************************/
/**
 * @brief 事件回调函数类型
 * @param event_type 事件类型
 * @param data 事件数据，根据事件类型不同而不同
 * @param user_data 用户自定义数据，注册时传入
 */
typedef void (*EventCallback_t)(int event_type, void *data, void *user_data);

/******************************************************************************************
 * 对外暴露接口 - 事件系统所有功能
 ******************************************************************************************/

/**
 * @brief 事件系统初始化
 * @details 初始化事件队列、注册中心和线程安全机制
 * @return SUCCESS/FAILURE
 */
int event_system_init(void);

/**
 * @brief 事件系统反初始化
 * @details 清理事件队列、注册中心和线程资源
 * @return SUCCESS/FAILURE
 */
int event_system_deinit(void);

/**
 * @brief 订阅事件
 * @details 注册事件回调函数，当指定事件发生时，将调用该回调函数
 * @param event_type 事件类型
 * @param callback 事件回调函数
 * @param user_data 用户自定义数据，回调时传入
 * @return SUCCESS/FAILURE/INVALID_PARAM
 */
int event_subscribe(int event_type, EventCallback_t callback, void *user_data);

/**
 * @brief 订阅事件（带优先级）
 * @details 注册事件回调函数，当指定事件发生时，将调用该回调函数
 * @param event_type 事件类型
 * @param callback 事件回调函数
 * @param user_data 用户自定义数据，回调时传入
 * @param priority 回调优先级（0-9，数字越小优先级越高）
 * @return SUCCESS/FAILURE/INVALID_PARAM
 */
int event_subscribe_with_priority(int event_type, EventCallback_t callback, void *user_data, int priority);

/**
 * @brief 取消订阅事件
 * @details 移除已注册的事件回调函数
 * @param event_type 事件类型
 * @param callback 要移除的事件回调函数
 * @return SUCCESS/FAILURE/INVALID_PARAM
 */
int event_unsubscribe(int event_type, EventCallback_t callback);

/**
 * @brief 发布事件
 * @details 发送事件到事件队列，由事件系统异步分发
 * @param event_type 事件类型
 * @param data 事件数据，根据事件类型不同而不同
 * @return SUCCESS/FAILURE/INVALID_PARAM
 */
int event_notify(int event_type, void *data);

/**
 * @brief 发布事件（可指定是否需要释放数据）
 * @details 发送事件到事件队列，由事件系统异步分发
 * @param event_type 事件类型
 * @param data 事件数据，根据事件类型不同而不同
 * @param need_free 事件数据是否需要自动释放
 * @return SUCCESS/FAILURE/INVALID_PARAM
 */
int event_notify_with_free(int event_type, void *data, bool need_free);

/**
 * @brief 发布同步事件
 * @details 立即分发事件，阻塞直到所有订阅者处理完成
 * @param event_type 事件类型
 * @param data 事件数据
 * @return SUCCESS/FAILURE/INVALID_PARAM
 */
int event_notify_sync(int event_type, void *data);

/**
 * @brief 获取事件队列状态
 * @details 用于监控事件系统运行状态
 * @param queue_size [out] 输出当前队列中的事件数量
 * @return SUCCESS/FAILURE/INVALID_PARAM
 */
int event_get_queue_status(int *queue_size);

/**
 * @brief 设置事件队列最大大小
 * @details 用于动态调整事件队列大小
 * @param max_size 最大队列大小
 * @return SUCCESS/FAILURE
 */
int event_set_max_queue_size(int max_size);

#endif // __EVENT_H__
