#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <alsa/asoundlib.h>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <math.h>
#include <sys/inotify.h>
#include <libudev.h>

#include "usb_audio.h"
#include "../../include/dbus_utils.h"
#include "../../include/alsa_utils.h"

// 前向声明
static void* usb_audio_worker_thread(void* data);
static void* usb_audio_monitor_thread(void* data);
static int usb_audio_setup_alsa_device(UsbAudioService* service);
static void usb_audio_cleanup_alsa_device(UsbAudioService* service);
static int usb_audio_create_pipewire_stream(UsbAudioService* service);
static void usb_audio_set_state(UsbAudioService* service, UsbAudioState state);
static void usb_audio_process_audio(UsbAudioService* service);
static int usb_audio_detect_devices(UsbAudioService* service);
static int usb_audio_get_device_info(UsbAudioService* service);

// 创建USB音频服务
UsbAudioService* usb_audio_create(struct pw_context* context, const UsbAudioConfig* config) {
    if (!context || !config) {
        fprintf(stderr, "Invalid parameters for usb_audio_create\n");
        return NULL;
    }

    UsbAudioService* service = calloc(1, sizeof(UsbAudioService));
    if (!service) {
        fprintf(stderr, "Failed to allocate memory for UsbAudioService\n");
        return NULL;
    }

    // 初始化互斥锁
    pthread_mutex_init(&service->mutex, NULL);

    // 设置上下文
    service->context = context;
    service->state = USB_AUDIO_STATE_DISABLED;
    service->running = false;
    service->monitoring = false;
    service->stream = NULL;
    memset(&service->session, 0, sizeof(UsbAudioSession));
    service->session.volume = 1.0f;
    service->session.muted = false;

    // 复制配置
    service->config = *config;
    if (!service->config.device_name[0]) {
        strncpy(service->config.device_name, "USB Audio", sizeof(service->config.device_name)-1);
    }
    if (!service->config.alsa_device[0]) {
        strncpy(service->config.alsa_device, "hw:USB", sizeof(service->config.alsa_device)-1);
    }
    if (service->config.sample_rate == 0) {
        service->config.sample_rate = 48000;
    }
    if (service->config.channels == 0) {
        service->config.channels = 2;
    }
    if (service->config.bit_depth == 0) {
        service->config.bit_depth = 16;
    }
    service->config.volume = fmaxf(0.0f, fminf(1.0f, service->config.volume));
    if (service->config.port == 0) {
        service->config.port = 10032;
    }
    if (service->config.buffer_size == 0) {
        service->config.buffer_size = 4096;
    }
    if (service->config.period_size == 0) {
        service->config.period_size = 1024;
    }

    // 初始化D-Bus
    if (!dbus_initialize()) {
        fprintf(stderr, "Failed to initialize D-Bus connection for USB Audio\n");
        // 继续初始化但记录警告
    }

    return service;
}

// 销毁USB音频服务
void usb_audio_destroy(UsbAudioService* service) {
    if (!service) return;

    usb_audio_stop(service);

    // 清理ALSA设备
    usb_audio_cleanup_alsa_device(service);

    // 清理互斥锁
    pthread_mutex_destroy(&service->mutex);

    // 清理D-Bus
    dbus_cleanup();

    free(service);
}

// 启动USB音频服务
int usb_audio_start(UsbAudioService* service) {
    if (!service || service->running) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 设置状态为启用中
    usb_audio_set_state(service, USB_AUDIO_STATE_ENABLED);

    // 检测USB音频设备
    if (usb_audio_detect_devices(service) < 0) {
        fprintf(stderr, "No USB audio devices found\n");
        // 如果不是自动连接模式，仍然启动服务但保持禁用状态
        if (!service->config.auto_connect) {
            usb_audio_set_state(service, USB_AUDIO_STATE_DISABLED);
            pthread_mutex_unlock(&service->mutex);
            return 0;
        }
        // 自动连接模式下，没有设备则进入错误状态
        usb_audio_set_state(service, USB_AUDIO_STATE_ERROR);
        snprintf(service->error_msg, sizeof(service->error_msg), "No USB audio devices detected");
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 设置ALSA设备
    if (usb_audio_setup_alsa_device(service) < 0) {
        fprintf(stderr, "Failed to setup ALSA device\n");
        usb_audio_set_state(service, USB_AUDIO_STATE_ERROR);
        snprintf(service->error_msg, sizeof(service->error_msg), "ALSA device initialization failed");
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 获取USB设备信息
    usb_audio_get_device_info(service);

    // 创建PipeWire流
    if (usb_audio_create_pipewire_stream(service) < 0) {
        fprintf(stderr, "Failed to create PipeWire stream\n");
        usb_audio_cleanup_alsa_device(service);
        usb_audio_set_state(service, USB_AUDIO_STATE_ERROR);
        snprintf(service->error_msg, sizeof(service->error_msg), "PipeWire stream creation failed");
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 设置运行标志
    service->running = true;

    // 创建工作线程
    if (pthread_create(&service->thread, NULL, usb_audio_worker_thread, service) != 0) {
        fprintf(stderr, "Failed to create USB Audio worker thread\n");
        service->running = false;
        usb_audio_cleanup_alsa_device(service);
        if (service->stream) {
            pw_stream_destroy(service->stream);
            service->stream = NULL;
        }
        usb_audio_set_state(service, USB_AUDIO_STATE_ERROR);
        snprintf(service->error_msg, sizeof(service->error_msg), "Thread creation failed");
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 如果启用自动连接，启动监控线程
    if (service->config.auto_connect) {
        service->monitoring = true;
        if (pthread_create(&service->monitor_thread, NULL, usb_audio_monitor_thread, service) != 0) {
            fprintf(stderr, "Failed to create USB device monitor thread\n");
            service->monitoring = false;
            // 监控线程创建失败不影响主服务运行
        }
    }

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 停止USB音频服务
void usb_audio_stop(UsbAudioService* service) {
    if (!service || !service->running) {
        return;
    }

    pthread_mutex_lock(&service->mutex);
    service->running = false;
    service->monitoring = false;
    pthread_mutex_unlock(&service->mutex);

    // 等待工作线程结束
    pthread_join(service->thread, NULL);

    // 如果监控线程正在运行，等待其结束
    if (service->config.auto_connect) {
        pthread_join(service->monitor_thread, NULL);
    }

    // 清理资源
    usb_audio_cleanup_alsa_device(service);

    if (service->stream) {
        pw_stream_destroy(service->stream);
        service->stream = NULL;
    }

    // 设置状态为禁用
    usb_audio_set_state(service, USB_AUDIO_STATE_DISABLED);
}

// 设置音量
int usb_audio_set_volume(UsbAudioService* service, float volume) {
    if (!service) {
        return -1;
    }

    volume = fmaxf(0.0f, fminf(1.0f, volume));

    pthread_mutex_lock(&service->mutex);
    service->session.volume = volume;
    pthread_mutex_unlock(&service->mutex);

    // 发送音量变化D-Bus信号
    json_t* details = json_object();
    json_object_set_new(details, "volume", json_real(volume));
    json_object_set_new(details, "muted", json_boolean(service->session.muted));
    json_object_set_new(details, "device_name", json_string(service->config.device_name));
    json_object_set_new(details, "usb_vendor", json_string(service->session.usb_vendor));
    json_object_set_new(details, "usb_product", json_string(service->session.usb_product));
    json_object_set_new(details, "timestamp", json_integer(time(NULL)));

    const char* json_str = json_dumps(details, JSON_COMPACT);
    if (json_str) {
        dbus_emit_signal("com.realtimeaudio.UsbAudio", DBUS_SIGNAL_TYPE_VOLUME_CHANGED, json_str);
        free((void*)json_str);
    }
    json_decref(details);

    return 0;
}

// 设置静音
int usb_audio_set_mute(UsbAudioService* service, bool muted) {
    if (!service) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);
    service->session.muted = muted;
    pthread_mutex_unlock(&service->mutex);

    // 发送静音变化D-Bus信号
    json_t* details = json_object();
    json_object_set_new(details, "muted", json_boolean(muted));
    json_object_set_new(details, "volume", json_real(service->session.volume));
    json_object_set_new(details, "device_name", json_string(service->config.device_name));
    json_object_set_new(details, "usb_vendor", json_string(service->session.usb_vendor));
    json_object_set_new(details, "usb_product", json_string(service->session.usb_product));
    json_object_set_new(details, "timestamp", json_integer(time(NULL)));

    const char* json_str = json_dumps(details, JSON_COMPACT);
    if (json_str) {
        dbus_emit_signal("com.realtimeaudio.UsbAudio", DBUS_SIGNAL_TYPE_MUTE_CHANGED, json_str);
        free((void*)json_str);
    }
    json_decref(details);

    return 0;
}

// 获取当前状态
UsbAudioState usb_audio_get_state(UsbAudioService* service) {
    if (!service) {
        return USB_AUDIO_STATE_ERROR;
    }

    UsbAudioState state;
    pthread_mutex_lock(&service->mutex);
    state = service->state;
    pthread_mutex_unlock(&service->mutex);
    return state;
}

// 获取会话信息
const UsbAudioSession* usb_audio_get_session(UsbAudioService* service) {
    if (!service || service->state == USB_AUDIO_STATE_DISABLED || service->state == USB_AUDIO_STATE_ERROR) {
        return NULL;
    }

    return &service->session;
}

// 获取错误信息
const char* usb_audio_get_error(UsbAudioService* service) {
    if (!service) {
        return "Invalid service pointer";
    }

    return service->error_msg;
}

// 重新扫描USB设备
int usb_audio_rescan_devices(UsbAudioService* service) {
    if (!service) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 保存当前状态
    bool was_running = service->running;
    UsbAudioState old_state = service->state;

    // 如果服务正在运行，先停止它
    if (was_running) {
        service->running = false;
        pthread_mutex_unlock(&service->mutex);

        pthread_join(service->thread, NULL);
        usb_audio_cleanup_alsa_device(service);

        pthread_mutex_lock(&service->mutex);
    }

    // 检测USB音频设备
    int ret = usb_audio_detect_devices(service);
    if (ret < 0) {
        fprintf(stderr, "No USB audio devices found during rescan\n");
        usb_audio_set_state(service, USB_AUDIO_STATE_ERROR);
        snprintf(service->error_msg, sizeof(service->error_msg), "No USB audio devices detected during rescan");
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 设置ALSA设备
    if (usb_audio_setup_alsa_device(service) < 0) {
        fprintf(stderr, "Failed to setup ALSA device during rescan\n");
        usb_audio_set_state(service, USB_AUDIO_STATE_ERROR);
        snprintf(service->error_msg, sizeof(service->error_msg), "ALSA device initialization failed during rescan");
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 获取USB设备信息
    usb_audio_get_device_info(service);

    // 如果之前服务在运行，重新启动工作线程
    if (was_running) {
        service->running = true;
        if (pthread_create(&service->thread, NULL, usb_audio_worker_thread, service) != 0) {
            fprintf(stderr, "Failed to recreate USB Audio worker thread\n");
            service->running = false;
            usb_audio_cleanup_alsa_device(service);
            usb_audio_set_state(service, USB_AUDIO_STATE_ERROR);
            snprintf(service->error_msg, sizeof(service->error_msg), "Thread recreation failed during rescan");
            pthread_mutex_unlock(&service->mutex);
            return -1;
        }
        usb_audio_set_state(service, old_state);
    }

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 设置状态
static void usb_audio_set_state(UsbAudioService* service, UsbAudioState state) {
    if (!service) return;

    pthread_mutex_lock(&service->mutex);
    UsbAudioState old_state = service->state;
    service->state = state;
    service->session.state = state;
    if (state == USB_AUDIO_STATE_ACTIVE && old_state != USB_AUDIO_STATE_ACTIVE) {
        service->session.active_time = time(NULL);
    }
    pthread_mutex_unlock(&service->mutex);

    // 只有状态实际改变时才发送信号
    if (old_state != state) {
        // 创建JSON payload
        json_t* details = json_object();
        json_object_set_new(details, "old_state", json_integer(old_state));
        json_object_set_new(details, "new_state", json_integer(state));
        json_object_set_new(details, "device_name", json_string(service->config.device_name));
        json_object_set_new(details, "alsa_device", json_string(service->config.alsa_device));
        json_object_set_new(details, "usb_vendor", json_string(service->session.usb_vendor));
        json_object_set_new(details, "usb_product", json_string(service->session.usb_product));
        json_object_set_new(details, "timestamp", json_integer(time(NULL)));

        const char* json_str = json_dumps(details, JSON_COMPACT);
        if (json_str) {
            // 发送D-Bus信号
            dbus_emit_signal("com.realtimeaudio.UsbAudio", DBUS_SIGNAL_TYPE_STATE_CHANGED, json_str);
            free((void*)json_str);
        }
        json_decref(details);

        // 日志记录
        printf("USB Audio state changed from %d to %d\n", old_state, state);
    }
}

// 工作线程函数
static void* usb_audio_worker_thread(void* data) {
    UsbAudioService* service = (UsbAudioService*)data;
    struct timespec timeout;

    while (1) {
        pthread_mutex_lock(&service->mutex);
        bool running = service->running;
        pthread_mutex_unlock(&service->mutex);

        if (!running) {
            break;
        }

        // 处理音频
        usb_audio_process_audio(service);

        // 短暂休眠
        timeout.tv_sec = 0;
        timeout.tv_nsec = 10000000; // 10ms
        nanosleep(&timeout, NULL);
    }

    return NULL;
}

// USB设备监控线程
static void* usb_audio_monitor_thread(void* data) {
    UsbAudioService* service = (UsbAudioService*)data;
    struct udev *udev;
    struct udev_monitor *mon;
    int fd;

    // 创建udev上下文
    udev = udev_new();
    if (!udev) {
        fprintf(stderr, "Failed to create udev context\n");
        return NULL;
    }

    // 创建监控器
    mon = udev_monitor_new_from_netlink(udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(mon, "sound", NULL);
    udev_monitor_enable_receiving(mon);
    fd = udev_monitor_get_fd(mon);

    // 监控循环
    while (1) {
        pthread_mutex_lock(&service->mutex);
        bool monitoring = service->monitoring;
        pthread_mutex_unlock(&service->mutex);

        if (!monitoring) {
            break;
        }

        // 等待设备事件
        fd_set fds;
        struct timeval tv;
        int ret;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = 1; // 1秒超时
        tv.tv_usec = 0;

        ret = select(fd+1, &fds, NULL, NULL, &tv);

        // 检查是否有事件
        if (ret > 0 && FD_ISSET(fd, &fds)) {
            // 接收设备事件
            struct udev_device *dev = udev_monitor_receive_device(mon);
            if (dev) {
                const char* action = udev_device_get_action(dev);
                const char* subsystem = udev_device_get_subsystem(dev);
                const char* devnode = udev_device_get_devnode(dev);

                // 检查是否是声卡设备事件
                if (subsystem && strcmp(subsystem, "sound") == 0 && devnode && strstr(devnode, "controlC") != NULL) {
                    if (action && strcmp(action, "add") == 0) {
                        printf("USB audio device added: %s\n", devnode);
                        // 发送设备连接D-Bus信号
                        json_t* details = json_object();
                        json_object_set_new(details, "device", json_string(devnode));
                        json_object_set_new(details, "action", json_string("add"));
                        json_object_set_new(details, "timestamp", json_integer(time(NULL)));

                        const char* json_str = json_dumps(details, JSON_COMPACT);
                        if (json_str) {
                            dbus_emit_signal("com.realtimeaudio.UsbAudio", DBUS_SIGNAL_TYPE_DEVICE_CONNECTED, json_str);
                            free((void*)json_str);
                        }
                        json_decref(details);

                        // 如果服务正在运行，重新扫描设备
                        pthread_mutex_lock(&service->mutex);
                        bool running = service->running;
                        pthread_mutex_unlock(&service->mutex);
                        if (running) {
                            usb_audio_rescan_devices(service);
                        }
                    } else if (action && strcmp(action, "remove") == 0) {
                        printf("USB audio device removed: %s\n", devnode);
                        // 发送设备断开D-Bus信号
                        json_t* details = json_object();
                        json_object_set_new(details, "device", json_string(devnode));
                        json_object_set_new(details, "action", json_string("remove"));
                        json_object_set_new(details, "timestamp", json_integer(time(NULL)));

                        const char* json_str = json_dumps(details, JSON_COMPACT);
                        if (json_str) {
                            dbus_emit_signal("com.realtimeaudio.UsbAudio", DBUS_SIGNAL_TYPE_DEVICE_DISCONNECTED, json_str);
                            free((void*)json_str);
                        }
                        json_decref(details);

                        // 如果服务正在运行，重新扫描设备
                        pthread_mutex_lock(&service->mutex);
                        bool running = service->running;
                        pthread_mutex_unlock(&service->mutex);
                        if (running) {
                            usb_audio_rescan_devices(service);
                        }
                    }
                }
                udev_device_unref(dev);
            }
        }
    }

    // 清理udev资源
    udev_monitor_unref(mon);
    udev_unref(udev);

    return NULL;
}

// 检测USB音频设备
static int usb_audio_detect_devices(UsbAudioService* service) {
    if (!service) return -1;

    // 简单实现：检查ALSA设备中是否有USB设备
    int card, err;
    snd_ctl_card_info_t* info;

    snd_ctl_card_info_alloca(&info);

    // 遍历所有声卡
    for (card = -1; ; card++) {
        char name[32];
        sprintf(name, "hw:%d", card);

        // 打开声卡控制接口
        snd_ctl_t* ctl;
        err = snd_ctl_open(&ctl, name, 0);
        if (err < 0) {
            break;
        }

        // 获取声卡信息
        err = snd_ctl_card_info(ctl, info);
        if (err < 0) {
            snd_ctl_close(ctl);
            continue;
        }

        // 检查是否是USB声卡
        const char* card_name = snd_ctl_card_info_get_name(info);
        if (card_name && strstr(card_name, "USB") != NULL) {
            // 找到USB声卡，更新ALSA设备名称
            snprintf(service->config.alsa_device, sizeof(service->config.alsa_device), "hw:%d", card);
            snd_ctl_close(ctl);
            return 0;
        }

        snd_ctl_close(ctl);
    }

    return -1;
}

// 获取USB设备信息
static int usb_audio_get_device_info(UsbAudioService* service) {
    if (!service) return -1;

    // 从ALSA设备名称解析卡号
    int card = -1;
    if (sscanf(service->config.alsa_device, "hw:%d", &card) != 1) {
        return -1;
    }

    // 打开声卡控制接口
    char ctl_name[32];
    sprintf(ctl_name, "hw:%d", card);
    snd_ctl_t* ctl;
    int err = snd_ctl_open(&ctl, ctl_name, 0);
    if (err < 0) {
        return err;
    }

    // 获取声卡信息
    snd_ctl_card_info_t* info;
    snd_ctl_card_info_alloca(&info);
    err = snd_ctl_card_info(ctl, info);
    if (err < 0) {
        snd_ctl_close(ctl);
        return err;
    }

    // 获取厂商和产品信息
    const char* card_name = snd_ctl_card_info_get_name(info);
    const char* card_id = snd_ctl_card_info_get_id(info);

    if (card_name) {
        // 尝试从名称中提取厂商和产品信息
        // 格式通常为："USB Audio Device, USB Audio"
        char* comma_pos = strchr(card_name, ',');
        if (comma_pos) {
            *comma_pos = '\0';
            strncpy(service->session.usb_product, card_name, sizeof(service->session.usb_product)-1);
            strncpy(service->session.usb_vendor, comma_pos+2, sizeof(service->session.usb_vendor)-1);
        } else {
            strncpy(service->session.usb_product, card_name, sizeof(service->session.usb_product)-1);
        }
    }

    if (card_id) {
        strncpy(service->session.usb_serial, card_id, sizeof(service->session.usb_serial)-1);
    }

    snd_ctl_close(ctl);
    return 0;
}

// 设置ALSA设备
static int usb_audio_setup_alsa_device(UsbAudioService* service) {
    int ret;
    snd_pcm_hw_params_t* params;
    unsigned int sample_rate = service->config.sample_rate;
    snd_pcm_uframes_t frames = service->config.period_size;

    // 打开PCM设备
    ret = snd_pcm_open(&service->session.pcm_handle, service->config.alsa_device, SND_PCM_STREAM_CAPTURE, 0);
    if (ret < 0) {
        fprintf(stderr, "Failed to open ALSA device %s: %s\n", service->config.alsa_device, snd_strerror(ret));
        return ret;
    }

    // 分配硬件参数对象
    snd_pcm_hw_params_alloca(&params);

    // 填充默认参数
    ret = snd_pcm_hw_params_any(service->session.pcm_handle, params);
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize ALSA hw params: %s\n", snd_strerror(ret));
        goto error;
    }

    // 设置交错模式
    ret = snd_pcm_hw_params_set_access(service->session.pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (ret < 0) {
        fprintf(stderr, "Failed to set interleaved mode: %s\n", snd_strerror(ret));
        goto error;
    }

    // 设置格式
    snd_pcm_format_t format;
    switch (service->config.bit_depth) {
        case 8: format = SND_PCM_FORMAT_U8; break;
        case 16: format = SND_PCM_FORMAT_S16_LE; break;
        case 24: format = SND_PCM_FORMAT_S24_LE; break;
        case 32: format = SND_PCM_FORMAT_S32_LE; break;
        default: format = SND_PCM_FORMAT_S16_LE; service->config.bit_depth = 16;
    }

    ret = snd_pcm_hw_params_set_format(service->session.pcm_handle, params, format);
    if (ret < 0) {
        fprintf(stderr, "Failed to set format: %s\n", snd_strerror(ret));
        goto error;
    }

    // 设置声道数
    ret = snd_pcm_hw_params_set_channels(service->session.pcm_handle, params, service->config.channels);
    if (ret < 0) {
        fprintf(stderr, "Failed to set channels: %s\n", snd_strerror(ret));
        goto error;
    }

    // 设置采样率
    ret = snd_pcm_hw_params_set_rate_near(service->session.pcm_handle, params, &sample_rate, NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to set sample rate: %s\n", snd_strerror(ret));
        goto error;
    }
    service->config.sample_rate = sample_rate;

    // 设置缓冲区大小
    ret = snd_pcm_hw_params_set_buffer_size_near(service->session.pcm_handle, params, &service->config.buffer_size);
    if (ret < 0) {
        fprintf(stderr, "Failed to set buffer size: %s\n", snd_strerror(ret));
        goto error;
    }

    // 设置周期大小
    ret = snd_pcm_hw_params_set_period_size_near(service->session.pcm_handle, params, &frames, NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to set period size: %s\n", snd_strerror(ret));
        goto error;
    }
    service->config.period_size = frames;

    // 应用参数
    ret = snd_pcm_hw_params(service->session.pcm_handle, params);
    if (ret < 0) {
        fprintf(stderr, "Failed to apply hw params: %s\n", snd_strerror(ret));
        goto error;
    }

    // 准备接口
    ret = snd_pcm_prepare(service->session.pcm_handle);
    if (ret < 0) {
        fprintf(stderr, "Failed to prepare PCM interface: %s\n", snd_strerror(ret));
        goto error;
    }

    // 配置音频格式信息
    service->session.format.format = format;
    service->session.format.channels = service->config.channels;
    service->session.format.rate = sample_rate;

    return 0;

error:
    snd_pcm_close(service->session.pcm_handle);
    service->session.pcm_handle = NULL;
    return ret;
}

// 清理ALSA设备
static void usb_audio_cleanup_alsa_device(UsbAudioService* service) {
    if (!service) return;

    pthread_mutex_lock(&service->mutex);
    if (service->session.pcm_handle) {
        snd_pcm_drain(service->session.pcm_handle);
        snd_pcm_close(service->session.pcm_handle);
        service->session.pcm_handle = NULL;
    }
    pthread_mutex_unlock(&service->mutex);
}

// 创建PipeWire流
static int usb_audio_create_pipewire_stream(UsbAudioService* service) {
    struct pw_properties* props;
    struct spa_audio_info_raw info = {
        .format = SPA_AUDIO_FORMAT_F32,
        .channels = service->config.channels,
        .rate = service->config.sample_rate,
    };
    const struct spa_pod* params[1];
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    // 创建流属性
    props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_STREAM_NAME, service->config.device_name,
        PW_KEY_DEVICE, service->config.alsa_device,
        NULL
    );

    if (!props) {
        fprintf(stderr, "Failed to create properties for USB Audio stream\n");
        return -1;
    }

    // 创建格式参数
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

    // 创建流
    service->stream = pw_stream_new_simple(
        service->context,
        "usb-audio-stream",
        props,
        params
    );

    if (!service->stream) {
        fprintf(stderr, "Failed to create USB Audio stream\n");
        pw_properties_free(props);
        return -1;
    }

    return 0;
}

// 处理音频
static void usb_audio_process_audio(UsbAudioService* service) {
    if (!service || service->state != USB_AUDIO_STATE_ENABLED || !service->session.pcm_handle || !service->stream) {
        return;
    }

    // 读取ALSA数据
    char buffer[service->config.buffer_size];
    snd_pcm_uframes_t frames = service->config.period_size;
    int ret = snd_pcm_readi(service->session.pcm_handle, buffer, frames);

    if (ret < 0) {
        fprintf(stderr, "ALSA read error: %s\n", snd_strerror(ret));
        // 尝试恢复
        if (ret == -EPIPE) {
            snd_pcm_prepare(service->session.pcm_handle);
        }
        return;
    }

    if (ret == 0) {
        // 没有数据
        return;
    }

    // 如果有数据且之前未激活，则更新状态
    if (service->state != USB_AUDIO_STATE_ACTIVE) {
        usb_audio_set_state(service, USB_AUDIO_STATE_ACTIVE);
    }

    // 应用音量和静音
    pthread_mutex_lock(&service->mutex);
    float volume = service->session.volume;
    bool muted = service->session.muted;
    pthread_mutex_unlock(&service->mutex);

    if (muted) {
        volume = 0.0f;
    }

    // 应用音量（简化实现）
    if (volume != 1.0f) {
        // 根据位深度处理不同格式
        if (service->config.bit_depth == 16) {
            int16_t* samples = (int16_t*)buffer;
            for (int i = 0; i < ret * service->config.channels; i++) {
                samples[i] = (int16_t)(samples[i] * volume);
            }
        } else if (service->config.bit_depth == 32 && service->session.format.format == SND_PCM_FORMAT_S32_LE) {
            int32_t* samples = (int32_t*)buffer;
            for (int i = 0; i < ret * service->config.channels; i++) {
                samples[i] = (int32_t)(samples[i] * volume);
            }
        }
    }

    // 将数据写入PipeWire流
    struct pw_buffer* pw_buf = pw_stream_dequeue_buffer(service->stream);
    if (!pw_buf) {
        fprintf(stderr, "Failed to dequeue buffer\n");
        return;
    }

    struct spa_buffer* spa_buf = pw_buf->buffer;
    if (spa_buf->datas[0].data && spa_buf->datas[0].maxsize >= ret * service->config.channels * (service->config.bit_depth / 8)) {
        memcpy(spa_buf->datas[0].data, buffer, ret * service->config.channels * (service->config.bit_depth / 8));
        spa_buf->datas[0].chunk->offset = 0;
        spa_buf->datas[0].chunk->size = ret * service->config.channels * (service->config.bit_depth / 8);
        service->session.total_frames += ret;
    }

    pw_stream_queue_buffer(service->stream, pw_buf);
}