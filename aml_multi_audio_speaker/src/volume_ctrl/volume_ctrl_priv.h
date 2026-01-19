#ifndef __VOLUME_CTRL_PRIV_H__
#define __VOLUME_CTRL_PRIV_H__

#include "volume_ctrl.h"
#include "product_type.h"

// 音量默认值
#define VOLUME_DEFAULT        DEFAULT_VOLUME_VAL

// 音量轨道类型
#define VOLUME_TRACK_SINGLE    1   // 单轨音量
#define VOLUME_TRACK_DOUBLE    2   // 双轨音量
#define VOLUME_TRACK_TRIPLE    3   // 三轨音量

// 音量控制结构体定义
typedef struct {
    int main_vol;             // 主音量
    int bass_vol;             // 低音音量
    int treble_vol;           // 高音音量
    int mute;                // 静音状态
    int init_ok;              // 初始化状态
    int track;                // 音量轨道类型
    int left;                 // 左声道音量
    int right;                // 右声道音量
    int sub;                  // 低音炮音量
} VolumeCtrl_t;

// 内部函数声明
int main_volume_init(void);
void main_volume_deinit(void);
int channel_volume_init(void);
void channel_volume_deinit(void);

#endif // __VOLUME_CTRL_PRIV_H__