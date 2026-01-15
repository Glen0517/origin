#ifndef __SUBWOOFER_COMM_H__
#define __SUBWOOFER_COMM_H__

#include "common_def.h"

#if CONFIG_ENABLE_BT_MESH

/**
 * @brief 低音炮配置结构体
 */
typedef struct {
    char bt_name[32];          // 低音炮蓝牙名称
    int bass_gain;             // 低音增益值 (0-100)
    bool vol_sync_en;          // 音量同步使能
    bool auto_connect_en;      // 自动连接使能
} SubwooferConfig_t;

/**
 * @brief 低音炮连接状态枚举
 */
typedef enum {
    SUBWOOFER_STATE_DISCONNECTED = 0,
    SUBWOOFER_STATE_CONNECTING,
    SUBWOOFER_STATE_CONNECTED,
    SUBWOOFER_STATE_PAIRED
} SubwooferState_e;

/**
 * @brief 低音炮音频数据结构体
 */
typedef struct {
    uint8_t *data;             // PCM音频数据
    unsigned int len;          // 数据长度
    int sample_rate;           // 采样率
    int channels;              // 声道数
    int bit_depth;             // 位深度
} SubwooferAudioData_t;

/**
 * @brief 低音炮初始化
 * @param cfg 低音炮配置结构体指针
 * @return SUCCESS/FAILURE
 */
int subwoofer_comm_init(SubwooferConfig_t *cfg);

/**
 * @brief 低音炮反初始化
 * @return SUCCESS/FAILURE
 */
int subwoofer_comm_deinit(void);

/**
 * @brief 获取低音炮连接状态
 * @return 连接状态 (SubwooferState_e)
 */
SubwooferState_e subwoofer_comm_get_state(void);

/**
 * @brief 设置低音增益
 * @param gain 低音增益值 (0-100)
 * @return SUCCESS/FAILURE
 */
int subwoofer_comm_set_bass_gain(int gain);

/**
 * @brief 获取当前低音增益
 * @return 低音增益值 (0-100)
 */
int subwoofer_comm_get_bass_gain(void);

/**
 * @brief 发送音量同步指令
 * @param master_vol 主音量值 (0-30)
 * @param is_mute 是否静音
 * @return SUCCESS/FAILURE
 */
int subwoofer_comm_sync_volume(int master_vol, bool is_mute);

/**
 * @brief 发送开关指令
 * @param power_on 是否开机
 * @return SUCCESS/FAILURE
 */
int subwoofer_comm_set_power(bool power_on);

/**
 * @brief 接收音频数据并发送到功放
 * @param audio_data 音频数据结构体
 * @return SUCCESS/FAILURE
 */
int subwoofer_comm_play_audio(SubwooferAudioData_t *audio_data);

/**
 * @brief 轮询处理低音炮事件
 */
void subwoofer_comm_event_poll(void);

#endif

#endif // __SUBWOOFER_COMM_H__
