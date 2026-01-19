#ifndef __BLUETOOTH_H__
#define __BLUETOOTH_H__

#include "common_def.h"

/******************************************************************************************
 * 蓝牙配置结构体
 ******************************************************************************************/
typedef struct {
    char bt_name[32];      // 蓝牙设备名
    char bt_pin[8];        // 蓝牙配对码
    bool bt_auto_connect;  // 是否自动重连
    bool bt_mesh_en;       // 是否开启蓝牙MESH
} BluetoothConfig_t;

/******************************************************************************************
 * 对外暴露接口 - 蓝牙模块所有功能，宏控裁剪蓝牙MESH
 ******************************************************************************************/
/**
 * @brief  蓝牙模块初始化
 * @param  cfg 蓝牙配置结构体指针
 * @return SUCCESS/FAILURE
 */
int bluetooth_init(BluetoothConfig_t *cfg);

/**
 * @brief  蓝牙模块反初始化
 * @return SUCCESS/FAILURE
 */
int bluetooth_deinit(void);

/**
 * @brief  开启蓝牙配对
 * @return SUCCESS/FAILURE
 */
int bluetooth_start_pair(void);

/**
 * @brief  关闭蓝牙配对
 * @return SUCCESS/FAILURE
 */
int bluetooth_stop_pair(void);

/**
 * @brief  获取蓝牙连接状态
 * @return TRUE-已连接 FALSE-未连接
 */
bool bluetooth_get_connect_state(void);

/**
 * @brief  获取当前连接的蓝牙设备名
 * @param  name 设备名缓冲区
 * @param  len 缓冲区长度
 * @return SUCCESS/FAILURE
 */
int bluetooth_get_dev_name(char *name, int len);

/**
 * @brief  获取当前连接的蓝牙设备地址
 * @param  addr 设备地址缓冲区
 * @param  len 缓冲区长度
 * @return SUCCESS/FAILURE
 */
int bluetooth_get_dev_addr(char *addr, int len);

/**
 * @brief  获取当前连接的蓝牙设备类型
 * @return 设备类型：0表示未知，1表示手机，2表示电脑，3表示其他
 */
int bluetooth_get_dev_type(void);

/**
 * @brief  连接指定的蓝牙设备
 * @param  addr 蓝牙设备地址（格式：XX:XX:XX:XX:XX:XX）
 * @return SUCCESS/FAILURE
 */
int bluetooth_connect(const char *addr);

/**
 * @brief  断开当前连接的蓝牙设备
 * @return SUCCESS/FAILURE
 */
int bluetooth_disconnect(void);

/**
 * @brief  设置蓝牙自动重连功能
 * @param  auto_connect 是否开启自动重连
 * @return SUCCESS/FAILURE
 */
int bluetooth_set_auto_connect(bool auto_connect);

/**
 * @brief  获取蓝牙自动重连功能状态
 * @return TRUE-开启 FALSE-关闭
 */
bool bluetooth_get_auto_connect(void);

/**
 * @brief  开始蓝牙音频流传输
 * @return SUCCESS/FAILURE
 */
int bluetooth_start_audio_stream(void);

/**
 * @brief  停止蓝牙音频流传输
 * @return SUCCESS/FAILURE
 */
int bluetooth_stop_audio_stream(void);

/**
 * @brief  设置蓝牙音频流的音量
 * @param  volume 音量值（0-100）
 * @return SUCCESS/FAILURE
 */
int bluetooth_set_audio_volume(int volume);

/**
 * @brief  获取蓝牙音频流的音量
 * @return 音量值（0-100），失败返回-1
 */
int bluetooth_get_audio_volume(void);

/**
 * @brief  设置蓝牙音频流的参数
 * @param  sample_rate 采样率（如44100、48000等）
 * @param  channels 声道数（1或2）
 * @param  bit_depth 比特深度（8、16、24等）
 * @return SUCCESS/FAILURE
 */
int bluetooth_set_audio_params(int sample_rate, int channels, int bit_depth);

/**
 * @brief  获取蓝牙音频流的状态
 * @return 音频流状态：0表示未启动，1表示正在播放，2表示暂停
 */
int bluetooth_get_audio_stream_state(void);

/**
 * @brief  蓝牙播放控制：播放
 * @return SUCCESS/FAILURE
 */
int bluetooth_play(void);

/**
 * @brief  蓝牙播放控制：暂停
 * @return SUCCESS/FAILURE
 */
int bluetooth_pause(void);

/**
 * @brief  蓝牙播放控制：停止
 * @return SUCCESS/FAILURE
 */
int bluetooth_stop(void);

/**
 * @brief  蓝牙播放控制：下一曲
 * @return SUCCESS/FAILURE
 */
int bluetooth_next(void);

/**
 * @brief  蓝牙播放控制：上一曲
 * @return SUCCESS/FAILURE
 */
int bluetooth_prev(void);

/**
 * @brief  蓝牙播放控制：音量增加
 * @return SUCCESS/FAILURE
 */
int bluetooth_volume_up(void);

/**
 * @brief  蓝牙播放控制：音量减少
 * @return SUCCESS/FAILURE
 */
int bluetooth_volume_down(void);

/**
 * @brief  蓝牙播放控制：静音
 * @return SUCCESS/FAILURE
 */
int bluetooth_mute(void);

/**
 * @brief  蓝牙播放控制：取消静音
 * @return SUCCESS/FAILURE
 */
int bluetooth_unmute(void);

/**
 * @brief  获取蓝牙断连原因
 * @return 断连原因：0表示未知，1表示正常断开，2表示信号丢失，3表示设备电量低，4表示其他
 */
int bluetooth_get_disconnect_reason(void);

/**
 * @brief  重置蓝牙重连尝试次数
 * @return SUCCESS/FAILURE
 */
int bluetooth_reset_reconnect_attempts(void);

/**
 * @brief  设置蓝牙重连参数
 * @param  max_attempts 最大重连尝试次数
 * @param  interval 重连间隔（秒）
 * @return SUCCESS/FAILURE
 */
int bluetooth_set_reconnect_params(int max_attempts, int interval);

/**
 * @brief  获取蓝牙重连参数
 * @param  max_attempts 最大重连尝试次数
 * @param  interval 重连间隔（秒）
 * @return SUCCESS/FAILURE
 */
int bluetooth_get_reconnect_params(int *max_attempts, int *interval);

/**
 * @brief  清理蓝牙连接资源
 * @return SUCCESS/FAILURE
 */
int bluetooth_cleanup_connection(void);

/**
 * @brief  蓝牙事件轮询
 * @return 无
 */
void bluetooth_event_poll(void);

#if CONFIG_ENABLE_BT_MESH
/**
 * @brief  蓝牙MESH组网(高端+低音炮专属)
 * @return SUCCESS/FAILURE
 */
int bluetooth_mesh_create_network(void);

/**
 * @brief  蓝牙MESH配对(高端+低音炮专属)
 * @return SUCCESS/FAILURE
 */
int bluetooth_mesh_pair(void);

/**
 * @brief  蓝牙MESH音量同步(高端+低音炮专属)
 * @param  vol 音量值
 * @return SUCCESS/FAILURE
 */
int bluetooth_mesh_sync_volume(int vol);
#endif

#endif // __BLUETOOTH_H__