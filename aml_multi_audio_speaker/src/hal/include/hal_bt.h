/**
 * @file hal_bt.h
 * @brief 蓝牙硬件抽象接口
 * @details 定义蓝牙硬件抽象层的接口函数，封装Amlogic蓝牙SDK的功能
 * @author AML Audio Team
 * @date 2026-01-16
 */

#ifndef HAL_BT_H
#define HAL_BT_H

#include "common_def.h"

/**
 * @brief 初始化蓝牙硬件
 * @details 初始化Amlogic蓝牙SDK，准备蓝牙硬件
 * @return 初始化结果：0表示成功，非0表示失败
 */
int hal_bt_init(void);

/**
 * @brief 反初始化蓝牙硬件
 * @details 反初始化Amlogic蓝牙SDK，清理蓝牙硬件资源
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int hal_bt_deinit(void);

/**
 * @brief 获取蓝牙连接状态
 * @details 获取当前蓝牙设备的连接状态
 * @return 连接状态：1表示已连接，0表示未连接
 */
int hal_bt_get_connection_status(void);

/**
 * @brief 设置蓝牙设备名称
 * @details 设置蓝牙设备的显示名称
 * @param name 设备名称字符串
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_bt_set_device_name(const char *name);

/**
 * @brief 设置蓝牙配对码
 * @details 设置蓝牙设备的配对码
 * @param pin 配对码字符串
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_bt_set_pin_code(const char *pin);

/**
 * @brief 设置蓝牙可发现模式
 * @details 设置蓝牙设备是否可被其他设备发现
 * @param discoverable 是否可发现
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_bt_set_discoverable(bool discoverable);

/**
 * @brief 设置蓝牙可配对模式
 * @details 设置蓝牙设备是否可被其他设备配对
 * @param pairable 是否可配对
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_bt_set_pairable(bool pairable);

/**
 * @brief 设置蓝牙可配对超时时间
 * @details 设置蓝牙设备可配对模式的超时时间
 * @param timeout 超时时间（秒）
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_bt_set_pairable_timeout(int timeout);

/**
 * @brief 获取已连接蓝牙设备名称
 * @details 获取当前已连接的蓝牙设备名称
 * @param name 设备名称缓冲区
 * @param len 缓冲区长度
 * @return 获取结果：0表示成功，非0表示失败
 */
int hal_bt_get_connected_dev_name(char *name, int len);

/**
 * @brief 获取已连接蓝牙设备地址
 * @details 获取当前已连接的蓝牙设备地址
 * @param addr 设备地址缓冲区
 * @param len 缓冲区长度
 * @return 获取结果：0表示成功，非0表示失败
 */
int hal_bt_get_connected_dev_addr(char *addr, int len);

/**
 * @brief 获取已连接蓝牙设备类型
 * @details 获取当前已连接的蓝牙设备类型
 * @param type 设备类型指针
 * @return 获取结果：0表示成功，非0表示失败
 */
int hal_bt_get_connected_dev_type(int *type);

/**
 * @brief 获取蓝牙断连原因
 * @details 获取蓝牙断开连接的原因
 * @return 断连原因：0表示未知，1表示正常断开，2表示信号丢失，3表示设备电量低，4表示其他
 */
int hal_bt_get_disconnect_reason(void);

/**
 * @brief 连接蓝牙设备
 * @details 连接指定地址的蓝牙设备
 * @param addr 蓝牙设备地址
 * @return 连接结果：0表示成功，非0表示失败
 */
int hal_bt_connect(const char *addr);

/**
 * @brief 断开蓝牙连接
 * @details 断开当前蓝牙设备的连接
 * @return 断开结果：0表示成功，非0表示失败
 */
int hal_bt_disconnect(void);

/**
 * @brief 初始化蓝牙A2DP功能
 * @details 初始化蓝牙A2DP协议相关的功能
 * @return 初始化结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_init(void);

/**
 * @brief 反初始化蓝牙A2DP功能
 * @details 反初始化蓝牙A2DP协议相关的功能
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_deinit(void);

/**
 * @brief 开始蓝牙A2DP音频流
 * @details 开始蓝牙A2DP音频流的传输
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_start_stream(void);

/**
 * @brief 停止蓝牙A2DP音频流
 * @details 停止蓝牙A2DP音频流的传输
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_stop_stream(void);

/**
 * @brief 设置蓝牙A2DP音量
 * @details 设置蓝牙A2DP音频流的音量
 * @param volume 音量值（0-100）
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_set_volume(int volume);

/**
 * @brief 获取蓝牙A2DP音量
 * @details 获取蓝牙A2DP音频流的当前音量
 * @return 音量值（0-100），失败返回-1
 */
int hal_bt_a2dp_get_volume(void);

/**
 * @brief 设置蓝牙A2DP音频参数
 * @details 设置蓝牙A2DP音频流的参数
 * @param sample_rate 采样率
 * @param channels 声道数
 * @param bit_depth 比特深度
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_set_audio_params(int sample_rate, int channels, int bit_depth);

/**
 * @brief 获取蓝牙A2DP媒体状态
 * @details 获取蓝牙A2DP音频流的播放状态
 * @return 媒体状态：1表示正在播放，0表示停止
 */
int hal_bt_a2dp_get_media_status(void);

/**
 * @brief 蓝牙A2DP播放控制：播放
 * @details 控制蓝牙A2DP音频流开始播放
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_play(void);

/**
 * @brief 蓝牙A2DP播放控制：暂停
 * @details 控制蓝牙A2DP音频流暂停播放
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_pause(void);

/**
 * @brief 蓝牙A2DP播放控制：停止
 * @details 控制蓝牙A2DP音频流停止播放
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_stop(void);

/**
 * @brief 蓝牙A2DP播放控制：下一曲
 * @details 控制蓝牙A2DP音频流播放下一曲
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_next(void);

/**
 * @brief 蓝牙A2DP播放控制：上一曲
 * @details 控制蓝牙A2DP音频流播放上一曲
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_prev(void);

/**
 * @brief 蓝牙A2DP音量控制：增加音量
 * @details 控制蓝牙A2DP音频流增加音量
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_volume_up(void);

/**
 * @brief 蓝牙A2DP音量控制：减少音量
 * @details 控制蓝牙A2DP音频流减少音量
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_volume_down(void);

/**
 * @brief 蓝牙A2DP音量控制：静音
 * @details 控制蓝牙A2DP音频流静音
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_mute(void);

/**
 * @brief 蓝牙A2DP音量控制：取消静音
 * @details 控制蓝牙A2DP音频流取消静音
 * @return 操作结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_unmute(void);

/**
 * @brief 轮询蓝牙事件
 * @details 处理蓝牙相关的事件，包括连接、断开、媒体流等
 * @return 轮询结果：0表示成功，非0表示失败
 */
int hal_bt_event_poll(void);

/**
 * @brief 轮询蓝牙A2DP事件
 * @details 处理蓝牙A2DP相关的事件
 * @return 轮询结果：0表示成功，非0表示失败
 */
int hal_bt_a2dp_event_poll(void);

/**
 * @brief 轮询蓝牙HFP事件
 * @details 处理蓝牙HFP（免提电话）相关的事件
 * @return 轮询结果：0表示成功，非0表示失败
 */
int hal_bt_hfp_event_poll(void);

/**
 * @brief 轮询蓝牙MESH事件
 * @details 处理蓝牙MESH网络相关的事件
 * @return 轮询结果：0表示成功，非0表示失败
 */
int hal_bt_mesh_event_poll(void);

/**
 * @brief 创建蓝牙MESH网络
 * @details 创建蓝牙MESH网络
 * @return 创建结果：0表示成功，非0表示失败
 */
int hal_bt_mesh_create_network(void);

/**
 * @brief 蓝牙MESH配对
 * @details 进行蓝牙MESH设备配对
 * @return 配对结果：0表示成功，非0表示失败
 */
int hal_bt_mesh_pair(void);

/**
 * @brief 蓝牙MESH音量同步
 * @details 同步蓝牙MESH网络中所有设备的音量
 * @param vol 音量值
 * @return 同步结果：0表示成功，非0表示失败
 */
int hal_bt_mesh_sync_volume(int vol);

#endif /* HAL_BT_H */