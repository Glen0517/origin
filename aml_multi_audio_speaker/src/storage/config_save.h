#ifndef __CONFIG_SAVE_H__
#define __CONFIG_SAVE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cJSON.h"
#include "product_type.h"
#include "aml_flash.h"  // AML SOC 原厂FLASH驱动头文件，SDK自带，必包含

/******************************************************************************************
 * 【编译宏控】配置文件路径 - 量产/开发一键切换，核心适配
 * 1. 量产环境：CONFIG_PATH = /etc/aml_soundbar/config/ (只读，根文件系统，防止篡改)
 * 2. 开发环境：CONFIG_PATH = ./config/ (相对路径，方便调试)
 ******************************************************************************************/
#define CONFIG_PROD_ENV        1   // 1=量产环境  0=开发环境
#if CONFIG_PROD_ENV
#define CONFIG_BASE_PATH       "/etc/aml_soundbar/config/"
#else
#define CONFIG_BASE_PATH       "./config/"
#endif

/******************************************************************************************
 * 配置文件全路径宏定义 - 统一管理，避免硬编码
 ******************************************************************************************/
#define CFG_FILE_PRODUCT       CONFIG_BASE_PATH"product_cfg.json"
#define CFG_FILE_AUDIO         CONFIG_BASE_PATH"audio_config.json"
#define CFG_FILE_PLAY          CONFIG_BASE_PATH"play_config.json"
#define CFG_FILE_SOURCE        CONFIG_BASE_PATH"source_config.json"
#define CFG_FILE_KEY           CONFIG_BASE_PATH"key_config.json"
#define CFG_FILE_EQ            CONFIG_BASE_PATH"eq_preset.json"
#define CFG_FILE_HDMI_ARC      CONFIG_BASE_PATH"hdmi_arc_config.json"
#define CFG_FILE_SPDIF         CONFIG_BASE_PATH"spdif_config.json"
#define CFG_FILE_SUBWOOFER     CONFIG_BASE_PATH"subwoofer_config.json"

/******************************************************************************************
 * 核心宏定义 - 兜底默认值、参数范围校验
 ******************************************************************************************/
#define DEFAULT_VOLUME         15
#define MAX_VOLUME             30
#define MIN_VOLUME             0
#define DEFAULT_SAMPLE_RATE    48000
#define KEY_DEBOUNCE_DEF       20
#define LONG_PRESS_DEF         800
#define EQ_GAIN_MIN            -12
#define EQ_GAIN_MAX            12

/******************************************************************************************
 * 配置结构体定义 - 与JSON文件字段一一对应，层级一致，无冗余字段
 * 说明：所有结构体字段均为【全局可访问】，业务模块直接读取，无需二次解析
 ******************************************************************************************/
// 产品能力开关配置(对应product_cfg.json)
typedef struct {
    int enable_hdmi_arc;
    int enable_spdif;
    int enable_bluetooth;
    int enable_bluetooth_mesh;
    int enable_wifi_media;
    int enable_dolby_dts;
    int enable_5_1_sound;
    int enable_3vol_ctrl;
    int enable_2vol_ctrl;
    int enable_1vol_ctrl;
    int enable_ir_learn;
    int enable_mic_mute;
    int enable_dual_ota;
    int enable_usb_ota;
    int enable_audio_calib;
} ProductCapCfg_t;

// 音频核心配置(对应audio_config.json)
typedef struct {
    int sample_rate_list[3];
    int default_sample_rate;
    int channel_num;
    int virtual_5_1_channel;
    int pcm_buffer_size;
    int pcm_period_size;
    int audio_anti_pop;
    int anti_pop_mute_time;
    int mute_threshold;
    int hardware_decode;
    int bass_freq_range[2];
    int bass_amp_gain;
} AudioCfg_t;

// 播放控制配置(对应play_config.json)
typedef struct {
    char sound_field_mode[5][10];
    char default_sound_field[10];
    int enable_bass_boost;
    int bass_boost_gain;
    int enable_voice_boost;
    int voice_boost_gain;
    int power_off_resume;
    int boot_default_play;
    int bt_reconnect_timeout;
} PlayCfg_t;

// 音源优先级配置(对应source_config.json)
typedef struct {
    char source_priority[6][20];
    char default_source[20];
    int source_switch_mute_time;
    int bt_auto_connect;
    int usb_scan_timeout;
} SourceCfg_t;

// 按键映射配置(对应key_config.json)
typedef struct {
    int key_debounce_time_ms;
    int long_press_time_ms;
    int key_code[10];
    char key_func_short[10][30];
    char key_func_long[10][30];
    int key_cnt;
} KeyCfg_t;

// EQ音效配置(对应eq_preset.json)
typedef struct {
    char eq_list[5][10];
    char default_eq[10];
    int gain_range[2];
    int freq_band[9];
    int eq_preset[5][9];
    int eq_band_cnt;
    int eq_mode_cnt;
} EqCfg_t;

// HDMI ARC专用配置(对应hdmi_arc_config.json)
typedef struct {
    int cec_protocol_enable;
    int auto_switch_audio_format;
    int arc_reconnect_enable;
    int reconnect_retry_cnt;
    int hot_plug_mute_time;
} HdmiArcCfg_t;

// SPDIF光纤专用配置(对应spdif_config.json)
typedef struct {
    int sample_rate_support[2];
    int default_sample_rate;
    int spdif_auto_detect;
    int detect_timeout_ms;
    int spdif_err_recover;
} SpdifCfg_t;

// 低音炮专用配置(对应subwoofer_config.json)
typedef struct {
    char bt_pair_name[30];
    char bt_pair_code[10];
    int bt_mesh_enable;
    int bass_volume_sync;
    int volume_sync_delay_ms;
    int sub_bt_reconnect;
    int reconnect_timeout_ms;
    int sub_amp_gain;
} SubwooferCfg_t;

/******************************************************************************************
 * 全局配置总结构体 - 所有配置聚合，统一管理，全局唯一
 ******************************************************************************************/
typedef struct {
    ProductCapCfg_t  product_cap;
    AudioCfg_t       audio;
    PlayCfg_t        play;
    SourceCfg_t      source;
    KeyCfg_t         key;
    EqCfg_t          eq;
    HdmiArcCfg_t     hdmi_arc;
    SpdifCfg_t       spdif;
    SubwooferCfg_t   subwoofer;
} GlobalConfig_t;

/******************************************************************************************
 * 对外暴露的核心接口 - 所有业务模块仅需调用这3个接口，解耦极致
 ******************************************************************************************/
// 初始化配置：加载默认值 -> 加载JSON配置 -> 加载FLASH校准参数 【程序启动时调用一次即可】
int config_init(void);

// 获取全局配置句柄：业务模块通过该接口获取配置结构体指针，读取配置项
GlobalConfig_t *config_get_global(void);

// 销毁配置：释放内存，程序退出时调用
void config_deinit(void);

/******************************************************************************************
 * ======================== FLASH 校准参数 专属配置 (产线量产核心，START) ========================
 * 适配 AML SOC 原厂 FLASH 驱动，通用所有晶晨芯片，无需修改底层驱动
 * 核心规则：FLASH校准参数 优先级 > JSON配置 > 代码默认值
 ******************************************************************************************/
// 1. FLASH 分区地址与大小配置 (AML SOC 通用分区规划，量产推荐独立分区，避开系统区，防止刷机丢失)
// ！！！重要：该地址为FLASH用户校准区起始地址，需与你的SDK分区表一致，建议分配 16KB (0x4000) 足够使用
#define FLASH_CALIB_START_ADDR     0x000E0000  // 校准区起始地址(示例)，根据你的实际分区表修改
#define FLASH_CALIB_SECTOR_SIZE    4096        // AML FLASH 最小擦除单位：4K字节(必选，不可改)
#define FLASH_CALIB_TOTAL_SIZE     0x4000      // 校准区总大小 16KB，足够存储所有校准参数

// 2. 校准参数范围校验宏 (与JSON配置一致，统一标准)
#define CALIB_EQ_GAIN_MIN          EQ_GAIN_MIN
#define CALIB_EQ_GAIN_MAX          EQ_GAIN_MAX
#define CALIB_BASS_GAIN_MIN        0
#define CALIB_BASS_GAIN_MAX        30
#define CALIB_TREBLE_GAIN_MIN      0
#define CALIB_TREBLE_GAIN_MAX      20
#define CALIB_VOLUME_GAIN_MIN      0
#define CALIB_VOLUME_GAIN_MAX      10
#define CALIB_CHANNEL_BALANCE_MIN  -5
#define CALIB_CHANNEL_BALANCE_MAX  5

// 3. 【核心】FLASH校准参数结构体 (工业级规范，所有产线校准参数结构化存储，对齐4字节，防止FLASH读写错位)
// 适配4类产品：高端Soundbar(完整EQ+全参数)、中端(简化EQ)、低端(仅高低音)、低音炮(仅低音增益)
// 大小：256字节 < 4K扇区，完美适配，无空间浪费
typedef struct __attribute__((packed, aligned(4)))
{
    int eq_gain[9];                // EQ频点增益(9个频点，对应eq_preset.json的freq_band)，行业标准
    int bass_gain;                 // 低音增益(核心校准项，低音炮优先级最高)
    int treble_gain;               // 高音增益
    int master_volume_gain;        // 主音量增益补偿
    int channel_balance;           // 声道平衡(-5~5，左负右正)
    int product_type_match;        // 产品类型匹配码，防止跨产品刷写参数
    unsigned int crc32_checksum;   // CRC32校验和，校验参数完整性，工业级必加
} FlashCalibParam_t;

// 4. 产品类型匹配码 (与product_type.h宏定义一一对应，防止参数刷错机型)
#define CALIB_MATCH_HIGH_END        0x01
#define CALIB_MATCH_MID_END         0x02
#define CALIB_MATCH_LOW_END         0x03
#define CALIB_MATCH_SUBWOOFER       0x04

// 5. 对外暴露的【产线专用核心接口】- 产线上位机/工具直接调用，所有接口均为int返回值，0=成功，-1=失败
/**
 * @brief  FLASH校准参数读取接口 - 程序启动时自动调用，加载校准参数覆盖JSON配置
 * @return 0:成功  -1:无有效参数  -2:校验失败
 */
int flash_calib_param_read(FlashCalibParam_t *p_param);

/**
 * @brief  FLASH校准参数写入接口 - 产线专用，写入前自动擦除扇区+校验参数合法性+计算CRC32
 * @param  p_param 待写入的校准参数结构体指针
 * @return 0:成功  -1:参数非法  -2:FLASH擦除失败  -3:FLASH写入失败
 */
int flash_calib_param_write(FlashCalibParam_t *p_param);

/**
 * @brief  FLASH校准参数恢复默认值 - 产线返修专用，清除校准参数，恢复JSON配置
 * @return 0:成功  -1:擦除失败
 */
int flash_calib_param_erase(void);

/**
 * @brief  加载FLASH校准参数到全局配置 - 对接原有逻辑，在config_init中调用，优先级最高
 * @return 0:加载成功  -1:无有效参数
 */
int config_load_flash_calib_param(void);

/******************************************************************************************
 * ======================== FLASH 校准参数 专属配置 (产线量产核心，END) ========================
 ******************************************************************************************/

#endif // __CONFIG_SAVE_H__