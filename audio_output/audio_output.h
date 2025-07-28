#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include <stdint.h>
#include <stdbool.h>
#include <linux/types.h>

/**
 * @brief 音频输出格式枚举
 */
enum AudioOutputFormat {
    AUDIO_FORMAT_S16_LE,  // 16位有符号整数，小端
    AUDIO_FORMAT_S32_LE,  // 32位有符号整数，小端
    AUDIO_FORMAT_FLOAT32_LE, // 32位浮点数，小端
    AUDIO_FORMAT_MAX
};

/**
 * @brief 音频输出配置结构体
 */
typedef struct {
    const char* device_name;  // 输出设备名称
    enum AudioOutputFormat format; // 音频格式
    uint32_t sample_rate;     // 采样率 (Hz)
    uint8_t channels;         // 声道数
    uint32_t buffer_size;     // 缓冲区大小 (frames)
    uint32_t period_size;     // 周期大小 (frames)
    bool use_dma;             // 是否使用DMA传输
    int priority;             // 实时优先级 (1-99)
} AudioOutputConfig;

/**
 * @brief 音频输出设备结构体
 * @note 不透明结构体，用户不应直接访问内部成员
 */
typedef struct AudioOutputDevice AudioOutputDevice;

/**
 * @brief 创建音频输出设备
 * @param config 音频输出配置
 * @return 成功返回设备指针，失败返回NULL
 */
AudioOutputDevice* audio_output_create(const AudioOutputConfig* config);

/**
 * @brief 销毁音频输出设备
 * @param device 音频输出设备指针
 */
void audio_output_destroy(AudioOutputDevice* device);

/**
 * @brief 打开音频输出设备
 * @param device 音频输出设备指针
 * @return 成功返回0，失败返回负数错误码
 */
int audio_output_open(AudioOutputDevice* device);

/**
 * @brief 关闭音频输出设备
 * @param device 音频输出设备指针
 */
void audio_output_close(AudioOutputDevice* device);

/**
 * @brief 写入音频数据到输出设备
 * @param device 音频输出设备指针
 * @param data 音频数据缓冲区
 * @param frames 音频帧数
 * @return 成功返回写入的帧数，失败返回负数错误码
 */
int audio_output_write(AudioOutputDevice* device, const void* data, uint32_t frames);

/**
 * @brief 获取音频输出设备的当前状态
 * @param device 音频输出设备指针
 * @return 0表示停止，1表示运行中，负数表示错误状态
 */
int audio_output_get_state(AudioOutputDevice* device);

/**
 * @brief 获取音频输出设备的缓冲信息
 * @param device 音频输出设备指针
 * @param[out] available 可用缓冲帧数
 * @param[out] latency 延迟时间 (微秒)
 * @return 成功返回0，失败返回负数错误码
 */
int audio_output_get_buffer_info(AudioOutputDevice* device, uint32_t* available, uint64_t* latency);

/**
 * @brief 设置音频输出设备的音量
 * @param device 音频输出设备指针
 * @param volume 音量值 (0.0-1.0)
 * @return 成功返回0，失败返回负数错误码
 */
int audio_output_set_volume(AudioOutputDevice* device, float volume);

/**
 * @brief 获取音频输出设备的音量
 * @param device 音频输出设备指针
 * @return 当前音量值 (0.0-1.0)
 */
float audio_output_get_volume(AudioOutputDevice* device);

#endif // AUDIO_OUTPUT_H