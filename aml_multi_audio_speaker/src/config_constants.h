/**
 * @file config_constants.h
 * @brief 配置项路径常量定义
 * @details 集中管理所有配置项路径常量，避免重复定义
 * @author AML Audio Team
 * @date 2026-01-28
 */

#ifndef __CONFIG_CONSTANTS_H__
#define __CONFIG_CONSTANTS_H__

// 音频相关配置
#define CONFIG_AUDIO_SAMPLE_RATE          "audio.sample_rate"
#define CONFIG_AUDIO_CHANNEL_NUM          "audio.channel_num"
#define CONFIG_AUDIO_PCM_BUFFER_SIZE      "audio.pcm_buffer_size"
#define CONFIG_AUDIO_HW_DECODE_EN         "audio.hw_decode_en"
#define CONFIG_AUDIO_DOLBY_DTS_EN         "audio.dolby_dts_en"
#define CONFIG_AUDIO_DEFAULT_SOURCE       "audio.default_source"
#define CONFIG_AUDIO_AUTO_SWITCH_EN       "audio.auto_switch_en"

// HDMI相关配置
#define CONFIG_HDMI_CEC_EN                "hdmi.cec_en"
#define CONFIG_HDMI_AUTO_SWITCH_EN        "hdmi.auto_switch_en"
#define CONFIG_HDMI_SAMPLE_RATE           "hdmi.sample_rate"

// SPDIF相关配置
#define CONFIG_SPDIF_AUTO_SWITCH_EN       "spdif.auto_switch_en"
#define CONFIG_SPDIF_SAMPLE_RATE          "spdif.sample_rate"
#define CONFIG_SPDIF_BITS_PER_SAMPLE      "spdif.bits_per_sample"

// 外设相关配置
#define CONFIG_PERIPHERAL_KEY_DEBOUNCE_MS "peripheral.key_debounce_ms"
#define CONFIG_PERIPHERAL_LONG_PRESS_MS   "peripheral.long_press_ms"
#define CONFIG_PERIPHERAL_IR_LEARN_EN     "peripheral.ir_learn_en"
#define CONFIG_PERIPHERAL_MIC_MUTE_EN     "peripheral.mic_mute_en"

// 蓝牙相关配置
#define CONFIG_BLUETOOTH_BT_NAME          "bluetooth.bt_name"
#define CONFIG_BLUETOOTH_BT_PIN           "bluetooth.bt_pin"
#define CONFIG_BLUETOOTH_BT_AUTO_CONNECT  "bluetooth.bt_auto_connect"

// 音量相关配置
#define CONFIG_VOLUME_MASTER_VOLUME       "volume.master_volume"
#define CONFIG_VOLUME_BASS_VOLUME         "volume.bass_volume"
#define CONFIG_VOLUME_TREBLE_VOLUME       "volume.treble_volume"
#define CONFIG_VOLUME_IS_MUTE             "volume.is_mute"

// 播放相关配置
#define CONFIG_PLAY_POWER_OFF_RESUME_EN   "play.power_off_resume_en"
#define CONFIG_PLAY_BOOT_DEFAULT_PLAY_EN  "play.boot_default_play_en"
#define CONFIG_PLAY_BT_RECONNECT_TIMEOUT  "play.bt_reconnect_timeout"
#define CONFIG_PLAY_DEFAULT_MODE          "play.default_mode"

// 音效相关配置
#define CONFIG_SOUND_DOLBY_EN             "sound.dolby_en"
#define CONFIG_SOUND_VIRTUAL_5_1_EN       "sound.virtual_5_1_en"

// WiFi相关配置
#define CONFIG_WIFI_WIFI_NAME             "wifi.wifi_name"
#define CONFIG_WIFI_DLNA_EN               "wifi.dlna_en"
#define CONFIG_WIFI_AIRPLAY_EN            "wifi.airplay_en"

// 低音炮相关配置
#define CONFIG_SUBWOOFER_BT_NAME          "subwoofer.bt_name"
#define CONFIG_SUBWOOFER_BASS_GAIN        "subwoofer.bass_gain"
#define CONFIG_SUBWOOFER_VOL_SYNC_EN      "subwoofer.vol_sync_en"
#define CONFIG_SUBWOOFER_AUTO_CONNECT_EN  "subwoofer.auto_connect_en"

#endif // __CONFIG_CONSTANTS_H__
