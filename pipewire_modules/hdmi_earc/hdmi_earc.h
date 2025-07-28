#ifndef __HDMI_EARC_H__
#define __HDMI_EARC_H__

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <alsa/asoundlib.h>
#include <pipewire/pipewire.h>

/**
 * HDMI e-ARC服务状态枚举
 */
typedef enum {
    HDMI_EARC_STATE_DISABLED,    // 禁用状态
    HDMI_EARC_STATE_ENABLED,     // 启用状态
    HDMI_EARC_STATE_ACTIVE,      // 活动状态（有音频输入）
    HDMI_EARC_STATE_CONNECTING,  // 连接中
    HDMI_EARC_STATE_DISCONNECTED,// 断开连接
    HDMI_EARC_STATE_ERROR        // 错误状态
} HdmiEarcState;

/**
 * HDMI e-ARC音频格式结构
 */
typedef struct {
    snd_pcm_format_t format;  // 音频格式
    uint32_t channels;        // 声道数
    uint32_t rate;            // 采样率
    uint32_t bit_depth;       // 位深度
    bool is_hdmi;             // 是否为HDMI格式
    bool is_arc;              // 是否为ARC
    bool is_earc;             // 是否为e-ARC
    bool dolby_atmos;         // 是否启用Dolby ATMOS
    bool dolby_eac3;          // 是否启用Dolby E-AC3
} HdmiEarcAudioFormat;

/**
 * HDMI e-ARC配置结构
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
    bool auto_connect;        // 自动连接HDMI设备
    bool enable_earc;         // 启用e-ARC
} HdmiEarcConfig;

/**
 * HDMI e-ARC会话信息结构
 */
typedef struct {
    HdmiEarcState state;               // 当前状态
    HdmiEarcAudioFormat format;        // 音频格式
    time_t active_time;                // 活动开始时间
    uint64_t total_frames;             // 总帧数
    float volume;                      // 当前音量
    bool muted;                        // 静音状态
    snd_pcm_t* pcm_handle;             // ALSA PCM句柄
    char hdmi_vendor[32];              // HDMI厂商信息
    char hdmi_model[32];               // HDMI型号信息
    char hdmi_version[16];             // HDMI版本
    uint8_t hdmi_port;                 // HDMI端口号
    bool earc_active;                  // e-ARC激活状态
} HdmiEarcSession;

/**
 * HDMI e-ARC服务结构
 */
typedef struct {
    struct pw_context* context;        // PipeWire上下文
    HdmiEarcState state;               // 服务状态
    HdmiEarcConfig config;             // 配置
    HdmiEarcSession session;           // 会话信息
    struct pw_stream* stream;          // PipeWire流
    pthread_t thread;                  // 工作线程
    pthread_t monitor_thread;          // HDMI设备监控线程
    pthread_mutex_t mutex;             // 互斥锁
    bool running;                      // 运行标志
    bool monitoring;                   // 监控标志
    char error_msg[256];               // 错误信息
} HdmiEarcService;

/**
 * 创建HDMI e-ARC服务
 * @param context PipeWire上下文
 * @param config HDMI e-ARC配置
 * @return 新创建的HDMI e-ARC服务实例，失败则返回NULL
 */
HdmiEarcService* hdmi_earc_create(struct pw_context* context, const HdmiEarcConfig* config);

/**
 * 销毁HDMI e-ARC服务
 * @param service HDMI e-ARC服务实例
 */
void hdmi_earc_destroy(HdmiEarcService* service);

/**
 * 启动HDMI e-ARC服务
 * @param service HDMI e-ARC服务实例
 * @return 成功返回0，失败返回负数
 */
int hdmi_earc_start(HdmiEarcService* service);

/**
 * 停止HDMI e-ARC服务
 * @param service HDMI e-ARC服务实例
 */
void hdmi_earc_stop(HdmiEarcService* service);

/**
 * 设置HDMI e-ARC音量
 * @param service HDMI e-ARC服务实例
 * @param volume 音量值 (0.0-1.0)
 * @return 成功返回0，失败返回负数
 */
int hdmi_earc_set_volume(HdmiEarcService* service, float volume);

/**
 * 设置HDMI e-ARC静音状态
 * @param service HDMI e-ARC服务实例
 * @param muted 静音标志
 * @return 成功返回0，失败返回负数
 */
int hdmi_earc_set_mute(HdmiEarcService* service, bool muted);

/**
 * 获取当前HDMI e-ARC状态
 * @param service HDMI e-ARC服务实例
 * @return 当前状态
 */
HdmiEarcState hdmi_earc_get_state(HdmiEarcService* service);

/**
 * 获取HDMI e-ARC会话信息
 * @param service HDMI e-ARC服务实例
 * @return 会话信息指针，失败返回NULL
 */
const HdmiEarcSession* hdmi_earc_get_session(HdmiEarcService* service);

/**
 * 获取错误信息
 * @param service HDMI e-ARC服务实例
 * @return 错误信息字符串
 */
const char* hdmi_earc_get_error(HdmiEarcService* service);

/**
 * 重新扫描HDMI e-ARC设备
 * @param service HDMI e-ARC服务实例
 * @return 成功返回0，失败返回负数
 */
int hdmi_earc_rescan_devices(HdmiEarcService* service);

/**
 * 切换e-ARC启用状态
 * @param service HDMI e-ARC服务实例
 * @param enable 是否启用e-ARC
 * @return 成功返回0，失败返回负数
 */
int hdmi_earc_toggle_earc(HdmiEarcService* service, bool enable);

#endif // __HDMI_EARC_H__