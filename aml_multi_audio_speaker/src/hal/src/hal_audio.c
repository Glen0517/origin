/**
 * @file hal_audio.c
 * @brief 音频硬件抽象实现
 * @details 实现音频硬件抽象层的接口函数，封装Amlogic音频SDK的功能
 * @author AML Audio Team
 * @date 2026-01-16
 */

#include "hal_audio.h"
#include "log/aml_log.h"
#include <aml_audio.h>

// 定义音频模块日志分类
AML_LOG_DEFINE(audio_log);
// 设置默认日志分类
#define AML_LOG_DEFAULT AML_LOG_GET_CAT(audio_log)

static bool g_audio_init = false;

/**
 * @brief 初始化音频硬件
 * @details 初始化Amlogic音频SDK，准备音频硬件
 * @return 初始化结果：0表示成功，非0表示失败
 */
int hal_audio_init(void) {
    if (g_audio_init) {
        AML_LOGI("HAL audio already initialized");
        return SUCCESS;
    }
    
    if (aml_audio_init() != 0) {
        AML_LOGE("Amlogic audio SDK init failed");
        return FAILURE;
    }
    
    g_audio_init = true;
    AML_LOGI("HAL audio init success");
    return SUCCESS;
}

/**
 * @brief 反初始化音频硬件
 * @details 反初始化Amlogic音频SDK，清理音频硬件资源
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int hal_audio_deinit(void) {
    if (!g_audio_init) {
        AML_LOGI("HAL audio not initialized");
        return SUCCESS;
    }
    
    if (aml_audio_deinit() != 0) {
        AML_LOGE("Amlogic audio SDK deinit failed");
        return FAILURE;
    }
    
    g_audio_init = false;
    AML_LOGI("HAL audio deinit success");
    return SUCCESS;
}

/**
 * @brief 设置音频采样率
 * @details 配置音频硬件的采样率
 * @param sample_rate 采样率值，如44100、48000等
 * @return 配置结果：0表示成功，非0表示失败
 */
int hal_audio_set_sample_rate(int sample_rate) {
    if (!g_audio_init) {
        AML_LOGE("HAL audio not initialized");
        return FAILURE;
    }
    
    if (sample_rate <= 0) {
        AML_LOGE("Invalid sample rate: %d", sample_rate);
        return INVALID_PARAM;
    }
    
    int ret = aml_audio_set_sample_rate(sample_rate);
    if (ret != 0) {
        AML_LOGE("Set sample rate failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("Sample rate set to: %d", sample_rate);
    return SUCCESS;
}

/**
 * @brief 设置音频通道数
 * @details 配置音频硬件的通道数
 * @param channels 通道数，如1（单声道）、2（立体声）等
 * @return 配置结果：0表示成功，非0表示失败
 */
int hal_audio_set_channels(int channels) {
    if (!g_audio_init) {
        AML_LOGE("HAL audio not initialized");
        return FAILURE;
    }
    
    if (channels <= 0) {
        AML_LOGE("Invalid channels: %d", channels);
        return INVALID_PARAM;
    }
    
    int ret = aml_audio_set_channels(channels);
    if (ret != 0) {
        AML_LOGE("Set channels failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("Channels set to: %d", channels);
    return SUCCESS;
}

/**
 * @brief 播放PCM音频数据
 * @details 向音频硬件写入PCM格式的音频数据并播放
 * @param data 音频数据指针，指向PCM数据缓冲区
 * @param len 数据长度，单位为字节
 * @return 播放结果：0表示成功，非0表示失败
 */
int hal_audio_play_pcm(uint8_t *data, int len) {
    if (!g_audio_init) {
        AML_LOGE("HAL audio not initialized");
        return FAILURE;
    }
    
    if (!data || len <= 0) {
        AML_LOGE("Invalid audio data or length");
        return INVALID_PARAM;
    }
    
    int ret = aml_audio_play_pcm(data, len);
    if (ret != 0) {
        AML_LOGE("Play PCM failed: %d", ret);
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 停止音频播放
 * @details 停止当前的音频播放
 * @return 停止结果：0表示成功，非0表示失败
 */
int hal_audio_stop(void) {
    if (!g_audio_init) {
        AML_LOGE("HAL audio not initialized");
        return FAILURE;
    }
    
    int ret = aml_audio_stop();
    if (ret != 0) {
        AML_LOGE("Stop audio failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("Audio stopped");
    return SUCCESS;
}

/**
 * @brief 获取音频播放状态
 * @details 获取当前音频播放的状态
 * @return 播放状态：1表示正在播放，0表示停止
 */
int hal_audio_get_status(void) {
    if (!g_audio_init) {
        AML_LOGE("HAL audio not initialized");
        return 0;
    }
    
    return aml_audio_get_status();
}

/**
 * @brief 设置音频硬件解码
 * @details 启用或禁用音频硬件解码功能
 * @param enable 是否启用硬件解码：true表示启用，false表示禁用
 * @return 配置结果：0表示成功，非0表示失败
 */
int hal_audio_set_hw_decode(bool enable) {
    if (!g_audio_init) {
        AML_LOGE("HAL audio not initialized");
        return FAILURE;
    }
    
    int ret = aml_audio_set_hw_decode(enable);
    if (ret != 0) {
        AML_LOGE("Set hardware decode failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("Hardware decode %s", enable ? "enabled" : "disabled");
    return SUCCESS;
}

/**
 * @brief 设置杜比DTS功能
 * @details 启用或禁用杜比DTS音效功能
 * @param enable 是否启用杜比DTS：true表示启用，false表示禁用
 * @return 配置结果：0表示成功，非0表示失败
 */
int hal_audio_set_dolby_dts(bool enable) {
    if (!g_audio_init) {
        AML_LOGE("HAL audio not initialized");
        return FAILURE;
    }
    
    int ret = aml_audio_set_dolby_dts(enable);
    if (ret != 0) {
        AML_LOGE("Set Dolby DTS failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("Dolby DTS %s", enable ? "enabled" : "disabled");
    return SUCCESS;
}

/**
 * @brief 播放测试音频
 * @details 播放用于测试的音频信号
 * @return 播放结果：0表示成功，非0表示失败
 */
int hal_audio_play_test_audio(void) {
    if (!g_audio_init) {
        AML_LOGE("HAL audio not initialized");
        return FAILURE;
    }
    
    int ret = aml_audio_play_test_audio();
    if (ret != 0) {
        AML_LOGE("Play test audio failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("Test audio playing");
    return SUCCESS;
}

/**
 * @brief 停止测试音频播放
 * @details 停止当前播放的测试音频
 * @return 停止结果：0表示成功，非0表示失败
 */
int hal_audio_stop_test_audio(void) {
    if (!g_audio_init) {
        AML_LOGE("HAL audio not initialized");
        return FAILURE;
    }
    
    int ret = aml_audio_stop_test_audio();
    if (ret != 0) {
        AML_LOGE("Stop test audio failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("Test audio stopped");
    return SUCCESS;
}

/**
 * @brief 暂停音频播放
 * @details 暂停当前的音频播放
 * @return 暂停结果：0表示成功，非0表示失败
 */
int hal_audio_pause(void) {
    if (!g_audio_init) {
        AML_LOGE("HAL audio not initialized");
        return FAILURE;
    }
    
    int ret = aml_audio_pause();
    if (ret != 0) {
        AML_LOGE("Pause audio failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("Audio paused");
    return SUCCESS;
}

/**
 * @brief 恢复音频播放
 * @details 恢复暂停的音频播放
 * @return 恢复结果：0表示成功，非0表示失败
 */
int hal_audio_resume(void) {
    if (!g_audio_init) {
        AML_LOGE("HAL audio not initialized");
        return FAILURE;
    }
    
    int ret = aml_audio_resume();
    if (ret != 0) {
        AML_LOGE("Resume audio failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("Audio resumed");
    return SUCCESS;
}

/**
 * @brief 设置音量
 * @details 设置音频播放的音量
 * @param volume 音量值，范围根据硬件而定
 * @return 设置结果：0表示成功，非0表示失败
 */
int hal_audio_set_volume(int volume) {
    if (!g_audio_init) {
        AML_LOGE("HAL audio not initialized");
        return FAILURE;
    }
    
    if (volume < MIN_VOLUME_VAL || volume > MAX_VOLUME_VAL) {
        AML_LOGE("Invalid volume: %d, range: %d-%d", volume, MIN_VOLUME_VAL, MAX_VOLUME_VAL);
        return INVALID_PARAM;
    }
    
    int ret = aml_audio_set_volume(volume);
    if (ret != 0) {
        AML_LOGE("Set volume failed: %d", ret);
        return FAILURE;
    }
    
    AML_LOGI("Volume set to: %d", volume);
    return SUCCESS;
}