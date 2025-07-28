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

#include "hdmi_earc.h"
#include "../../include/dbus_utils.h"
#include "../../include/alsa_utils.h"
#include "../../include/hdmi_utils.h"

// 前向声明
static void* hdmi_earc_worker_thread(void* data);
static void* hdmi_earc_monitor_thread(void* data);
static int hdmi_earc_setup_alsa_device(HdmiEarcService* service);
static void hdmi_earc_cleanup_alsa_device(HdmiEarcService* service);
static int hdmi_earc_create_pipewire_stream(HdmiEarcService* service);
static void hdmi_earc_set_state(HdmiEarcService* service, HdmiEarcState state);
static void hdmi_earc_process_audio(HdmiEarcService* service);
static int hdmi_earc_detect_devices(HdmiEarcService* service);
static int hdmi_earc_get_device_info(HdmiEarcService* service);
static int hdmi_earc_activate_earc(HdmiEarcService* service, bool activate);

// 创建HDMI e-ARC服务
HdmiEarcService* hdmi_earc_create(struct pw_context* context, const HdmiEarcConfig* config) {
    if (!context || !config) {
        fprintf(stderr, "Invalid parameters for hdmi_earc_create\n");
        return NULL;
    }

    HdmiEarcService* service = calloc(1, sizeof(HdmiEarcService));
    if (!service) {
        fprintf(stderr, "Failed to allocate memory for HdmiEarcService\n");
        return NULL;
    }

    // 初始化互斥锁
    pthread_mutex_init(&service->mutex, NULL);

    // 设置上下文
    service->context = context;
    service->state = HDMI_EARC_STATE_DISABLED;
    service->running = false;
    service->monitoring = false;
    service->stream = NULL;
    memset(&service->session, 0, sizeof(HdmiEarcSession));
    service->session.volume = 1.0f;
    service->session.muted = false;

    // 复制配置
    service->config = *config;
    if (!service->config.device_name[0]) {
        strncpy(service->config.device_name, "HDMI e-ARC", sizeof(service->config.device_name)-1);
    }
    if (!service->config.alsa_device[0]) {
        strncpy(service->config.alsa_device, "hw:HDMI", sizeof(service->config.alsa_device)-1);
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
        service->config.port = 10033;
    }
    if (service->config.buffer_size == 0) {
        service->config.buffer_size = 4096;
    }
    if (service->config.period_size == 0) {
        service->config.period_size = 1024;
    }

    // 初始化D-Bus
    if (!dbus_initialize()) {
        fprintf(stderr, "Failed to initialize D-Bus connection for HDMI e-ARC\n");
        // 继续初始化但记录警告
    }

    return service;
}

// 销毁HDMI e-ARC服务
void hdmi_earc_destroy(HdmiEarcService* service) {
    if (!service) return;

    hdmi_earc_stop(service);

    // 清理ALSA设备
    hdmi_earc_cleanup_alsa_device(service);

    // 清理互斥锁
    pthread_mutex_destroy(&service->mutex);

    // 清理D-Bus
    dbus_cleanup();

    free(service);
}

// 启动HDMI e-ARC服务
int hdmi_earc_start(HdmiEarcService* service) {
    if (!service || service->running) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 设置状态为启用中
    hdmi_earc_set_state(service, HDMI_EARC_STATE_ENABLED);

    // 检测HDMI audio设备
    if (hdmi_earc_detect_devices(service) < 0) {
        fprintf(stderr, "No HDMI audio devices found\n");
        // 如果不是自动连接模式，仍然启动服务但保持禁用状态
        if (!service->config.auto_connect) {
            hdmi_earc_set_state(service, HDMI_EARC_STATE_DISABLED);
            pthread_mutex_unlock(&service->mutex);
            return 0;
        }
        // 自动连接模式下，没有设备则进入错误状态
        hdmi_earc_set_state(service, HDMI_EARC_STATE_ERROR);
        snprintf(service->error_msg, sizeof(service->error_msg), "No HDMI audio devices detected");
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 设置ALSA设备
    if (hdmi_earc_setup_alsa_device(service) < 0) {
        fprintf(stderr, "Failed to setup ALSA device\n");
        hdmi_earc_set_state(service, HDMI_EARC_STATE_ERROR);
        snprintf(service->error_msg, sizeof(service->error_msg), "ALSA device initialization failed");
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 如果启用e-ARC，尝试激活
    if (service->config.enable_earc) {
        if (hdmi_earc_activate_earc(service, true) < 0) {
            fprintf(stderr, "Failed to activate e-ARC\n");
            // 非致命错误，继续启动但e-ARC将保持禁用
            strncat(service->error_msg, " (Failed to activate e-ARC)", sizeof(service->error_msg)-strlen(service->error_msg)-1);
        } else {
            service->session.earc_active = true;
        }
    }

    // 获取HDMI设备信息
    hdmi_earc_get_device_info(service);

    // 创建PipeWire流
    if (hdmi_earc_create_pipewire_stream(service) < 0) {
        fprintf(stderr, "Failed to create PipeWire stream\n");
        hdmi_earc_cleanup_alsa_device(service);
        hdmi_earc_set_state(service, HDMI_EARC_STATE_ERROR);
        snprintf(service->error_msg, sizeof(service->error_msg), "PipeWire stream creation failed");
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 设置运行标志
    service->running = true;

    // 创建工作线程
    if (pthread_create(&service->thread, NULL, hdmi_earc_worker_thread, service) != 0) {
        fprintf(stderr, "Failed to create HDMI e-ARC worker thread\n");
        service->running = false;
        hdmi_earc_cleanup_alsa_device(service);
        if (service->stream) {
            pw_stream_destroy(service->stream);
            service->stream = NULL;
        }
        hdmi_earc_set_state(service, HDMI_EARC_STATE_ERROR);
        snprintf(service->error_msg, sizeof(service->error_msg), "Thread creation failed");
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 如果启用自动连接，启动监控线程
    if (service->config.auto_connect) {
        service->monitoring = true;
        if (pthread_create(&service->monitor_thread, NULL, hdmi_earc_monitor_thread, service) != 0) {
            fprintf(stderr, "Failed to create HDMI device monitor thread\n");
            service->monitoring = false;
            // 监控线程创建失败不影响主服务运行
        }
    }

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 停止HDMI e-ARC服务
void hdmi_earc_stop(HdmiEarcService* service) {
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

    // 禁用e-ARC
    if (service->session.earc_active) {
        hdmi_earc_activate_earc(service, false);
    }

    // 清理资源
    hdmi_earc_cleanup_alsa_device(service);

    if (service->stream) {
        pw_stream_destroy(service->stream);
        service->stream = NULL;
    }

    // 设置状态为禁用
    hdmi_earc_set_state(service, HDMI_EARC_STATE_DISABLED);
}

// 设置音量
int hdmi_earc_set_volume(HdmiEarcService* service, float volume) {
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
    json_object_set_new(details, "hdmi_vendor", json_string(service->session.hdmi_vendor));
    json_object_set_new(details, "hdmi_model", json_string(service->session.hdmi_model));
    json_object_set_new(details, "earc_active", json_boolean(service->session.earc_active));
    json_object_set_new(details, "timestamp", json_integer(time(NULL)));

    const char* json_str = json_dumps(details, JSON_COMPACT);
    if (json_str) {
        dbus_emit_signal("com.realtimeaudio.HdmiEarc", DBUS_SIGNAL_TYPE_VOLUME_CHANGED, json_str);
        free((void*)json_str);
    }
    json_decref(details);

    return 0;
}

// 设置静音
int hdmi_earc_set_mute(HdmiEarcService* service, bool muted) {
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
    json_object_set_new(details, "hdmi_vendor", json_string(service->session.hdmi_vendor));
    json_object_set_new(details, "hdmi_model", json_string(service->session.hdmi_model));
    json_object_set_new(details, "earc_active", json_boolean(service->session.earc_active));
    json_object_set_new(details, "timestamp", json_integer(time(NULL)));

    const char* json_str = json_dumps(details, JSON_COMPACT);
    if (json_str) {
        dbus_emit_signal("com.realtimeaudio.HdmiEarc", DBUS_SIGNAL_TYPE_MUTE_CHANGED, json_str);
        free((void*)json_str);
    }
    json_decref(details);

    return 0;
}

// 获取当前状态
HdmiEarcState hdmi_earc_get_state(HdmiEarcService* service) {
    if (!service) {
        return HDMI_EARC_STATE_ERROR;
    }

    HdmiEarcState state;
    pthread_mutex_lock(&service->mutex);
    state = service->state;
    pthread_mutex_unlock(&service->mutex);
    return state;
}

// 获取会话信息
const HdmiEarcSession* hdmi_earc_get_session(HdmiEarcService* service) {
    if (!service || service->state == HDMI_EARC_STATE_DISABLED || service->state == HDMI_EARC_STATE_ERROR) {
        return NULL;
    }

    return &service->session;
}

// 获取错误信息
const char* hdmi_earc_get_error(HdmiEarcService* service) {
    if (!service) {
        return "Invalid service pointer";
    }

    return service->error_msg;
}

// 重新扫描HDMI设备
int hdmi_earc_rescan_devices(HdmiEarcService* service) {
    if (!service) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 保存当前状态
    bool was_running = service->running;
    HdmiEarcState old_state = service->state;
    bool was_earc_active = service->session.earc_active;

    // 如果服务正在运行，先停止它
    if (was_running) {
        service->running = false;
        pthread_mutex_unlock(&service->mutex);

        pthread_join(service->thread, NULL);
        hdmi_earc_cleanup_alsa_device(service);

        pthread_mutex_lock(&service->mutex);
    }

    // 检测HDMI音频设备
    int ret = hdmi_earc_detect_devices(service);
    if (ret < 0) {
        fprintf(stderr, "No HDMI audio devices found during rescan\n");
        hdmi_earc_set_state(service, HDMI_EARC_STATE_ERROR);
        snprintf(service->error_msg, sizeof(service->error_msg), "No HDMI audio devices detected during rescan");
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 设置ALSA设备
    if (hdmi_earc_setup_alsa_device(service) < 0) {
        fprintf(stderr, "Failed to setup ALSA device during rescan\n");
        hdmi_earc_set_state(service, HDMI_EARC_STATE_ERROR);
        snprintf(service->error_msg, sizeof(service->error_msg), "ALSA device initialization failed during rescan");
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 如果之前e-ARC是激活的，重新激活
    if (was_earc_active && service->config.enable_earc) {
        hdmi_earc_activate_earc(service, true);
        service->session.earc_active = true;
    }

    // 获取HDMI设备信息
    hdmi_earc_get_device_info(service);

    // 如果之前服务在运行，重新启动工作线程
    if (was_running) {
        service->running = true;
        if (pthread_create(&service->thread, NULL, hdmi_earc_worker_thread, service) != 0) {
            fprintf(stderr, "Failed to recreate HDMI e-ARC worker thread\n");
            service->running = false;
            hdmi_earc_cleanup_alsa_device(service);
            hdmi_earc_set_state(service, HDMI_EARC_STATE_ERROR);
            snprintf(service->error_msg, sizeof(service->error_msg), "Thread recreation failed during rescan");
            pthread_mutex_unlock(&service->mutex);
            return -1;
        }
        hdmi_earc_set_state(service, old_state);
    }

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 切换e-ARC启用状态
int hdmi_earc_toggle_earc(HdmiEarcService* service, bool enable) {
    if (!service) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);
    bool was_active = service->session.earc_active;
    service->config.enable_earc = enable;

    // 如果服务正在运行，应用更改
    if (service->running) {
        if (enable && !was_active) {
            // 激活e-ARC
            if (hdmi_earc_activate_earc(service, true) == 0) {
                service->session.earc_active = true;
                // 重新配置音频格式以适应e-ARC
                hdmi_earc_cleanup_alsa_device(service);
                hdmi_earc_setup_alsa_device(service);
            } else {
                pthread_mutex_unlock(&service->mutex);
                return -1;
            }
        } else if (!enable && was_active) {
            // 禁用e-ARC
            hdmi_earc_activate_earc(service, false);
            service->session.earc_active = false;
            // 重新配置音频格式
            hdmi_earc_cleanup_alsa_device(service);
            hdmi_earc_setup_alsa_device(service);
        }
    }

    pthread_mutex_unlock(&service->mutex);

    // 发送e-ARC状态变化D-Bus信号
    json_t* details = json_object();
    json_object_set_new(details, "earc_active", json_boolean(service->session.earc_active));
    json_object_set_new(details, "device_name", json_string(service->config.device_name));
    json_object_set_new(details, "hdmi_vendor", json_string(service->session.hdmi_vendor));
    json_object_set_new(details, "hdmi_model", json_string(service->session.hdmi_model));
    json_object_set_new(details, "timestamp", json_integer(time(NULL)));

    const char* json_str = json_dumps(details, JSON_COMPACT);
    if (json_str) {
        dbus_emit_signal("com.realtimeaudio.HdmiEarc", DBUS_SIGNAL_TYPE_EARC_TOGGLED, json_str);
        free((void*)json_str);
    }
    json_decref(details);

    return 0;
}

// 设置状态
static void hdmi_earc_set_state(HdmiEarcService* service, HdmiEarcState state) {
    if (!service) return;

    pthread_mutex_lock(&service->mutex);
    HdmiEarcState old_state = service->state;
    service->state = state;
    service->session.state = state;
    if (state == HDMI_EARC_STATE_ACTIVE && old_state != HDMI_EARC_STATE_ACTIVE) {
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
        json_object_set_new(details, "hdmi_vendor", json_string(service->session.hdmi_vendor));
        json_object_set_new(details, "hdmi_model", json_string(service->session.hdmi_model));
        json_object_set_new(details, "hdmi_port", json_integer(service->session.hdmi_port));
        json_object_set_new(details, "earc_active", json_boolean(service->session.earc_active));
        json_object_set_new(details, "timestamp", json_integer(time(NULL)));

        const char* json_str = json_dumps(details, JSON_COMPACT);
        if (json_str) {
            // 发送D-Bus信号
            dbus_emit_signal("com.realtimeaudio.HdmiEarc", DBUS_SIGNAL_TYPE_STATE_CHANGED, json_str);
            free((void*)json_str);
        }
        json_decref(details);

        // 日志记录
        printf("HDMI e-ARC state changed from %d to %d\n", old_state, state);
    }
}

// 工作线程函数
static void* hdmi_earc_worker_thread(void* data) {
    HdmiEarcService* service = (HdmiEarcService*)data;
    struct timespec timeout;

    while (1) {
        pthread_mutex_lock(&service->mutex);
        bool running = service->running;
        pthread_mutex_unlock(&service->mutex);

        if (!running) {
            break;
        }

        // 处理音频
        hdmi_earc_process_audio(service);

        // 短暂休眠
        timeout.tv_sec = 0;
        timeout.tv_nsec = 10000000; // 10ms
        nanosleep(&timeout, NULL);
    }

    return NULL;
}

// HDMI设备监控线程
static void* hdmi_earc_monitor_thread(void* data) {
    HdmiEarcService* service = (HdmiEarcService*)data;
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
    udev_monitor_filter_add_match_subsystem_devtype(mon, "drm", NULL);
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

                // 检查是否是HDMI相关事件
                bool is_hdmi = false;
                if (subsystem && strcmp(subsystem, "drm") == 0) {
                    is_hdmi = true;
                } else if (subsystem && strcmp(subsystem, "sound") == 0 && devnode && strstr(devnode, "HDMI") != NULL) {
                    is_hdmi = true;
                }

                if (is_hdmi) {
                    if (action && (strcmp(action, "add") == 0 || strcmp(action, "change") == 0)) {
                        printf("HDMI device connected/changed: %s\n", devnode ? devnode : "unknown");
                        // 发送设备连接D-Bus信号
                        json_t* details = json_object();
                        json_object_set_new(details, "device", json_string(devnode ? devnode : "unknown"));
                        json_object_set_new(details, "action", json_string(action));
                        json_object_set_new(details, "timestamp", json_integer(time(NULL)));

                        const char* json_str = json_dumps(details, JSON_COMPACT);
                        if (json_str) {
                            dbus_emit_signal("com.realtimeaudio.HdmiEarc", DBUS_SIGNAL_TYPE_DEVICE_CONNECTED, json_str);
                            free((void*)json_str);
                        }
                        json_decref(details);

                        // 如果服务正在运行，重新扫描设备
                        pthread_mutex_lock(&service->mutex);
                        bool running = service->running;
                        pthread_mutex_unlock(&service->mutex);
                        if (running) {
                            hdmi_earc_rescan_devices(service);
                        }
                    } else if (action && strcmp(action, "remove") == 0) {
                        printf("HDMI device removed: %s\n", devnode ? devnode : "unknown");
                        // 发送设备断开D-Bus信号
                        json_t* details = json_object();
                        json_object_set_new(details, "device", json_string(devnode ? devnode : "unknown"));
                        json_object_set_new(details, "action", json_string(action));
                        json_object_set_new(details, "timestamp", json_integer(time(NULL)));

                        const char* json_str = json_dumps(details, JSON_COMPACT);
                        if (json_str) {
                            dbus_emit_signal("com.realtimeaudio.HdmiEarc", DBUS_SIGNAL_TYPE_DEVICE_DISCONNECTED, json_str);
                            free((void*)json_str);
                        }
                        json_decref(details);

                        // 如果服务正在运行，重新扫描设备
                        pthread_mutex_lock(&service->mutex);
                        bool running = service->running;
                        pthread_mutex_unlock(&service->mutex);
                        if (running) {
                            hdmi_earc_rescan_devices(service);
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

// 检测HDMI音频设备
static int hdmi_earc_detect_devices(HdmiEarcService* service) {
    if (!service) return -1;

    // 检查ALSA设备中是否有HDMI设备
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

        // 检查是否是HDMI声卡
        const char* card_name = snd_ctl_card_info_get_name(info);
        if (card_name && strstr(card_name, "HDMI") != NULL) {
            // 找到HDMI声卡，更新ALSA设备名称
            snprintf(service->config.alsa_device, sizeof(service->config.alsa_device), "hw:%d", card);
            snd_ctl_close(ctl);
            return 0;
        }

        snd_ctl_close(ctl);
    }

    return -1;
}

// 获取HDMI设备信息
static int hdmi_earc_get_device_info(HdmiEarcService* service) {
    if (!service) return -1;

    // 从ALSA设备名称解析卡号
    int card = -1;
    if (sscanf(service->config.alsa_device, "hw:%d", &card) != 1) {
        return -1;
    }

    // 使用hdmi_utils获取详细HDMI信息
    hdmi_get_device_info(card, service->session.hdmi_vendor, sizeof(service->session.hdmi_vendor),
                        service->session.hdmi_model, sizeof(service->session.hdmi_model),
                        service->session.hdmi_version, sizeof(service->session.hdmi_version),
                        &service->session.hdmi_port);

    return 0;
}

// 激活e-ARC
static int hdmi_earc_activate_earc(HdmiEarcService* service, bool activate) {
    // 使用hdmi_utils激活/禁用e-ARC
    return hdmi_activate_earc(service->session.hdmi_port, activate);
}

// 设置ALSA设备
static int hdmi_earc_setup_alsa_device(HdmiEarcService* service) {
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

    // 设置格式 - e-ARC可能支持更高的位深度
    snd_pcm_format_t format;
    if (service->session.earc_active) {
        // e-ARC通常支持更高质量的音频
        switch (service->config.bit_depth) {
            case 8: format = SND_PCM_FORMAT_U8; break;
            case 16: format = SND_PCM_FORMAT_S16_LE; break;
            case 24: format = SND_PCM_FORMAT_S24_LE; break;
            case 32: format = SND_PCM_FORMAT_S32_LE; break;
            case 24_3: format = SND_PCM_FORMAT_S24_3LE; break;
            default: format = SND_PCM_FORMAT_S32_LE; service->config.bit_depth = 32;
        }
    } else {
        switch (service->config.bit_depth) {
            case 8: format = SND_PCM_FORMAT_U8; break;
            case 16: format = SND_PCM_FORMAT_S16_LE; break;
            case 24: format = SND_PCM_FORMAT_S24_LE; break;
            case 32: format = SND_PCM_FORMAT_S32_LE; break;
            default: format = SND_PCM_FORMAT_S16_LE; service->config.bit_depth = 16;
        }
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
    service->session.format.bit_depth = service->config.bit_depth;
    service->session.format.is_hdmi = true;
    service->session.format.is_arc = !service->session.earc_active;
    service->session.format.is_earc = service->session.earc_active;

    return 0;

error:
    snd_pcm_close(service->session.pcm_handle);
    service->session.pcm_handle = NULL;
    return ret;
}

// 清理ALSA设备
static void hdmi_earc_cleanup_alsa_device(HdmiEarcService* service) {
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
static int hdmi_earc_create_pipewire_stream(HdmiEarcService* service) {
    struct pw_properties* props;
    struct spa_audio_info_raw info = {
        .format = service->session.earc_active ? SPA_AUDIO_FORMAT_F32 : SPA_AUDIO_FORMAT_S16,
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
        "hdmi.earc.active", service->session.earc_active ? "true" : "false",
        NULL
    );

    if (!props) {
        fprintf(stderr, "Failed to create properties for HDMI e-ARC stream\n");
        return -1;
    }

    // 创建格式参数
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

    // 创建流
    service->stream = pw_stream_new_simple(
        service->context,
        "hdmi-earc-stream",
        props,
        params
    );

    if (!service->stream) {
        fprintf(stderr, "Failed to create HDMI e-ARC stream\n");
        pw_properties_free(props);
        return -1;
    }

    return 0;
}

// 处理音频
static void hdmi_earc_process_audio(HdmiEarcService* service) {
    if (!service || service->state != HDMI_EARC_STATE_ENABLED || !service->session.pcm_handle || !service->stream) {
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
        } else if (ret == -ESTRPIPE) {
            // 设备暂停，等待恢复
            hdmi_earc_set_state(service, HDMI_EARC_STATE_DISCONNECTED);
            while ((ret = snd_pcm_resume(service->session.pcm_handle)) == -EAGAIN) {
                sleep(1);
            }
            if (ret == 0) {
                snd_pcm_prepare(service->session.pcm_handle);
                hdmi_earc_set_state(service, HDMI_EARC_STATE_ENABLED);
            }
        }
        return;
    }

    if (ret == 0) {
        // 没有数据
        return;
    }

    // 如果有数据且之前未激活，则更新状态
    if (service->state != HDMI_EARC_STATE_ACTIVE) {
        hdmi_earc_set_state(service, HDMI_EARC_STATE_ACTIVE);
    }

    // 应用音量和静音
    pthread_mutex_lock(&service->mutex);
    float volume = service->session.volume;
    bool muted = service->session.muted;
    pthread_mutex_unlock(&service->mutex);

    if (muted) {
        volume = 0.0f;
    }

    // 应用音量（根据位深度处理不同格式）
    if (volume != 1.0f) {
        if (service->config.bit_depth == 16) {
            int16_t* samples = (int16_t*)buffer;
            for (int i = 0; i < ret * service->config.channels; i++) {
                samples[i] = (int16_t)(samples[i] * volume);
            }
        } else if (service->config.bit_depth == 24) {
            // 24位音频处理
            int32_t* samples = (int32_t*)buffer;
            for (int i = 0; i < ret * service->config.channels; i++) {
                samples[i] = (int32_t)(samples[i] * volume);
            }
        } else if (service->config.bit_depth == 32) {
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