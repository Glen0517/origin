#ifndef __USB_AUDIO_H__
#define __USB_AUDIO_H__

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <alsa/asoundlib.h>
#include <pipewire/pipewire.h>

/**
 * USB音频服务状态枚举
 */
typedef enum {
    USB_AUDIO_STATE_DISABLED,    // 禁用状态
    USB_AUDIO_STATE_ENABLED,     // 启用状态
    USB_AUDIO_STATE_ACTIVE,      // 活动状态（有音频输入）
    USB_AUDIO_STATE_ERROR        // 错误状态
} UsbAudioState;

/**
 * USB音频格式结构
 */
typedef struct {
    snd_pcm_format_t format;  // 音频格式
    uint32_t channels;        // 声道数
    uint32_t rate;            // 采样率
} UsbAudioFormat;

/**
 * USB音频配置结构
 */
typedef struct {
    char device_name[64];     // 设备名称
    char alsa_device[64];     // ALSA设备名称
    uint32_t sample_rate;     // 采样率
    uint32_t channels;        // 声道数
    uint32_t bit_depth;       // 位深度
    float volume;             // 音量 (0.0-1.0)
    uint16_t port;            // 端口号
    uint32_t buffer_size;     // 缓冲区大小
    uint32_t period_size;     // 周期大小
    bool auto_connect;        // 自动连接新USB设备
} UsbAudioConfig;

/**
 * USB音频会话信息结构
 */
typedef struct {
    UsbAudioState state;               // 当前状态
    UsbAudioFormat format;             // 音频格式
    time_t active_time;                // 活动开始时间
    uint64_t total_frames;             // 总帧数
    float volume;                      // 当前音量
    bool muted;                        // 静音状态
    snd_pcm_t* pcm_handle;             // ALSA PCM句柄
    char usb_vendor[32];               // USB厂商信息
    char usb_product[32];              // USB产品信息
    char usb_serial[32];               // USB序列号
} UsbAudioSession;

/**
 * USB音频服务结构
 */
typedef struct {
    struct pw_context* context;        // PipeWire上下文
    UsbAudioState state;               // 服务状态
    UsbAudioConfig config;             // 配置
    UsbAudioSession session;           // 会话信息
    struct pw_stream* stream;          // PipeWire流
    pthread_t thread;                  // 工作线程
    pthread_t monitor_thread;          // USB设备监控线程
    pthread_mutex_t mutex;             // 互斥锁
    bool running;                      // 运行标志
    bool monitoring;                   // 监控标志
    char error_msg[256];               // 错误信息
} UsbAudioService;

/**
 * 创建USB音频服务
 * @param context PipeWire上下文
 * @param config USB音频配置
 * @return 新创建的USB音频服务实例，失败则返回NULL
 */
UsbAudioService* usb_audio_create(struct pw_context* context, const UsbAudioConfig* config);

/**
 * 销毁USB音频服务
 * @param service USB音频服务实例
 */
void usb_audio_destroy(UsbAudioService* service);

/**
 * 启动USB音频服务
 * @param service USB音频服务实例
 * @return 成功返回0，失败返回负数
 */
int usb_audio_start(UsbAudioService* service);

/**
 * 停止USB音频服务
 * @param service USB音频服务实例
 */
void usb_audio_stop(UsbAudioService* service);

/**
 * 设置USB音频音量
 * @param service USB音频服务实例
 * @param volume 音量值 (0.0-1.0)
 * @return 成功返回0，失败返回负数
 */
int usb_audio_set_volume(UsbAudioService* service, float volume);

/**
 * 设置USB音频静音状态
 * @param service USB音频服务实例
 * @param muted 静音标志
 * @return 成功返回0，失败返回负数
 */
int usb_audio_set_mute(UsbAudioService* service, bool muted);

/**
 * 获取当前USB音频状态
 * @param service USB音频服务实例
 * @return 当前状态
 */
UsbAudioState usb_audio_get_state(UsbAudioService* service);

/**
 * 获取USB音频会话信息
 * @param service USB音频服务实例
 * @return 会话信息指针，失败返回NULL
 */
const UsbAudioSession* usb_audio_get_session(UsbAudioService* service);

/**
 * 获取错误信息
 * @param service USB音频服务实例
 * @return 错误信息字符串
 */
const char* usb_audio_get_error(UsbAudioService* service);

/**
 * 重新扫描USB音频设备
 * @param service USB音频服务实例
 * @return 成功返回0，失败返回负数
 */
int usb_audio_rescan_devices(UsbAudioService* service);

#endif // __USB_AUDIO_H__