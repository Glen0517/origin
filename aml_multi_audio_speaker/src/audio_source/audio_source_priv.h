#ifndef __AUDIO_SOURCE_PRIV_H__
#define __AUDIO_SOURCE_PRIV_H__

#include "audio_source.h"
#include "product_type.h"
#include "peripheral_priv.h"
#include "hdmi_arc.h"
#include "spdif_optical.h"
#include "bt.h"
#include "wifi_media.h"

// 音频源常量定义
#define AUDIO_SOURCE_BT         SOURCE_BLUETOOTH
#define AUDIO_SOURCE_USB        SOURCE_USB
#define AUDIO_SOURCE_HDMI       SOURCE_HDMI_ARC
#define AUDIO_SOURCE_SPDIF      SOURCE_SPDIF
#define AUDIO_SOURCE_AUX        SOURCE_AUX
#define AUDIO_SOURCE_UAC        8  // UAC音源
#define AUDIO_SOURCE_WIFI       SOURCE_WIFI

// LED索引定义
#define LED_SOURCE              1     // 音源指示灯

// 音频源结构体定义
typedef struct {
    int cur_source;             // 当前音源
    int src_count;              // 支持的音源数量
    int support_src[8];         // 支持的音源列表
    int source_status[8];       // 各音源状态
    bool auto_switch_en;        // 是否开启自动切换
    int init_ok;                // 初始化状态
} AudioSource_t;

// 内部函数声明
int src_bt_init(void);
void src_bt_deinit(void);
int src_hdmi_init(void);
void src_hdmi_deinit(void);
int src_spdif_init(void);
void src_spdif_deinit(void);
int src_usb_init(void);
void src_usb_deinit(void);
int src_aux_init(void);
void src_aux_deinit(void);
int src_uac_init(void);  // 新增：UAC音源
void src_uac_deinit(void);
int src_wifi_init(void); // 新增：WIFI音源
void src_wifi_deinit(void);

// 状态获取函数声明
int src_usb_get_status(void);
int src_aux_get_status(void);
int src_uac_get_status(void);

// 自动切换函数声明
void audio_source_auto_switch(void);

#endif // __AUDIO_SOURCE_PRIV_H__