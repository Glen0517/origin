/**
 * @file main.c
 * @brief 系统主程序入口
 * @details 负责系统的初始化、模块管理、业务主循环和优雅退出
 * @author AML Audio Team
 * @date 2026-01-15
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include "product_type.h"      // 产品类型定义
#include "common_def.h"         // 通用定义
#include "logger.h"            // 日志系统
#include "res_manager.h"        // 资源管理器
#include "event.h"             // 事件系统

// 公共对外头文件 - 核心功能模块
#include "audio_core.h"        // 音频核心处理
#include "audio_source.h"      // 音频源管理
#include "play_ctrl.h"         // 播放控制
#include "volume_ctrl.h"       // 音量控制
#include "peripheral.h"        // 外设管理
#include "storage.h"           // 存储管理
#include "bt.h"         // 蓝牙模块
#include "system.h"            // 系统管理
#include "comm_mcu.h"          // MCU通信  

// 宏控按需加载头文件 - 根据产品配置加载相应功能模块
#ifdef CONFIG_ENABLE_WIFI_MEDIA
#include "wifi_media.h"         // WIFI媒体功能（DLNA、AirPlay）
#endif
#ifdef CONFIG_ENABLE_DOLBY_DTS
#include "sound_effects.h"      // 音效处理（杜比、DTS）
#endif
#ifdef CONFIG_ENABLE_HDMI_ARC
#include "hdmi_arc.h"           // HDMI ARC功能
#endif
#ifdef CONFIG_ENABLE_SPDIF
#include "spdif_optical.h"      // SPDIF光纤输入
#endif
#ifdef CONFIG_ENABLE_BT_MESH
#include "subwoofer_comm.h"     // 低音炮通信
#endif
#ifdef CONFIG_ENABLE_VOICE_NOISE_REDUCTION
#include "voice_noise_reduction.h" // 语音降噪功能
#endif

#include "prod_test.h"          // 生产测试模块

// 线程池管理
#include "system/thread_pool.h"  // 线程池管理模块
// 进程管理
#include "system/process.h"      // 进程管理模块
#include "system/process_manager.h"  // 进程管理器模块
// 进程间通信
#include "system/ipc.h"          // 进程间通信模块
// 资源管理
#include "system/resource.h"      // 资源管理模块
// 监控和诊断
#include "system/monitor.h"       // 监控和诊断模块

// HAL和PAL层头文件
#include "hal.h"                // 硬件抽象层
#include "pal.h"                // 平台抽象层

// 定义默认日志分类
AML_LOG_DEFINE(default_log);

/**
 * @brief 系统运行状态标志
 * @details 用于控制主循环的运行，0表示退出，1表示继续运行
 */
static int g_sys_running = 1;

/**
 * @brief 信号处理函数
 * @details 处理系统信号，实现优雅退出
 * @param sig 接收到的信号类型
 * @return 无
 */
static void sig_handler(int sig) {
    // 处理中断信号和终止信号
    if (sig == SIGINT || sig == SIGTERM) {
        LOG_INFO("System receive exit signal [%d], start deinit...", sig);
        // 设置系统运行状态为退出
        g_sys_running = 0;
    }
}

/**
 * @brief 模块初始化总入口
 * @details 根据宏控配置初始化所有启用的模块，适配不同产品类型
 * @return 初始化结果，0表示成功，非0表示失败
 */
static int module_init_all(void) {
    int ret = 0;
    
    // 1. 初始化HAL和PAL层
    // HAL层：硬件抽象层，封装硬件相关操作
    // PAL层：平台抽象层，封装平台相关服务
    ret |= hal_init();              // 初始化硬件抽象层
    ret |= pal_init();              // 初始化平台抽象层
    
    // 2. 创建并初始化音频核心配置
    // 音频核心负责音频解码、混音和输出，是系统的核心组件
    AudioCoreConfig_t audio_cfg = {
        .sample_rate = 48000,       // 采样率：48kHz
        .channel_num = 2,           // 声道数：立体声
        .pcm_buffer_size = 4096,    // PCM缓冲区大小：4KB
        .hw_decode_en = true,       // 启用硬件解码
        .dolby_dts_en = false       // 默认禁用杜比DTS解码
    };
    
    // 2. 创建并初始化HDMI ARC配置
    // HDMI ARC用于接收电视的音频输出
#ifdef CONFIG_ENABLE_HDMI_ARC
    HdmiArcConfig_t hdmi_cfg = {
        .cec_en = true,            // 启用CEC控制
        .auto_switch_en = true,     // 启用自动切换
        .sample_rate = 48000        // 采样率：48kHz
    };
#endif
    
    // 3. 创建并初始化SPDIF配置
    // SPDIF用于接收光纤/同轴数字音频输入
#ifdef CONFIG_ENABLE_SPDIF
    SpdifConfig_t spdif_cfg = {
        .auto_switch_en = true,     // 启用自动切换
        .sample_rate = 48000,       // 采样率：48kHz
        .bits_per_sample = 16       // 位深度：16位
    };
#endif
    
    // 4. 创建并初始化外设配置
    // 外设包括按键、红外、LED、LCD等
    PeripheralConfig_t peri_cfg = {
        .key_debounce_ms = 50,       // 按键防抖时间：50ms
        .long_press_ms = 1000,       // 长按时间：1秒
        .ir_learn_en = true,        // 启用红外学习
        .mic_mute_en = true         // 启用麦克风静音功能
    };
    
    // 5. 根据产品类型调整配置
    switch (CURRENT_PRODUCT_TYPE) {
        case PRODUCT_GAME_HIGH_END:
            // 高端游戏音响配置
            LOG_INFO("=== Initializing HIGH END GAME SPEAKER ===");
            audio_cfg.pcm_buffer_size = 8192; // 增大缓冲区，支持复杂音效
            audio_cfg.dolby_dts_en = true;   // 启用杜比DTS解码
            peri_cfg.ir_learn_en = true;      // 启用红外学习
            break;
        
        case PRODUCT_GAME_MID_END:
            // 中端游戏音响配置
            LOG_INFO("=== Initializing MID END GAME SPEAKER ===");
            audio_cfg.pcm_buffer_size = 4096; // 标准缓冲区大小
            audio_cfg.dolby_dts_en = false;  // 禁用杜比DTS解码
            peri_cfg.ir_learn_en = false;     // 禁用红外学习
            break;
        
        case PRODUCT_GAME_LOW_END:
            // 低端游戏音响配置
            LOG_INFO("=== Initializing LOW END GAME SPEAKER ===");
            audio_cfg.pcm_buffer_size = 2048; // 减小缓冲区，降低资源占用
            audio_cfg.dolby_dts_en = false;  // 禁用杜比DTS解码
            peri_cfg.ir_learn_en = false;     // 禁用红外学习
            peri_cfg.mic_mute_en = false;     // 禁用麦克风静音功能
            break;
        
        default:
            // 其他产品类型保持默认配置
            break;
    }
    
    // 6. 初始化基础核心模块
    // 初始化顺序：存储 -> 外设 -> 蓝牙 -> 音频核心 -> 音频源 -> 音量控制 -> 播放控制 -> MCU通信 -> 系统 -> 生产测试
    // 低端游戏音响极致裁剪，只保留核心模块
    if (CURRENT_PRODUCT_TYPE != PRODUCT_GAME_LOW_END) {
        ret |= storage_init();                  // 存储管理模块，负责U盘挂载和媒体扫描
        ret |= peripheral_init(&peri_cfg);       // 外设管理模块，负责按键、红外等
        // 初始化蓝牙模块，使用默认配置
        BluetoothConfig_t bt_cfg = {
            .bt_name = "AML Audio Speaker",
            .bt_pin = "0000",
            .bt_auto_connect = true,
            .bt_mesh_en = false
        };
#ifdef CONFIG_ENABLE_BT_MESH
        bt_cfg.bt_mesh_en = true;
#endif
        ret |= bluetooth_init(&bt_cfg);         // 蓝牙模块，负责蓝牙连接和音频传输
    } else {
        // 低端游戏音响：仅初始化必要模块
        ret |= audio_core_init(&audio_cfg);     // 音频核心模块，负责音频解码和输出
        // 初始化音量控制模块，使用默认音量设置
        VolumeInfo_t default_vol = {
            .master_volume = DEFAULT_VOLUME_VAL,
            .bass_volume = DEFAULT_VOLUME_VAL,
            .treble_volume = DEFAULT_VOLUME_VAL,
            .is_mute = false
        };
        ret |= volume_ctrl_init(&default_vol);  // 音量控制模块，负责音量调节
        ret |= system_init();                   // 系统管理模块，负责系统状态和异常处理
    }
    
    // 非低端游戏音响初始化其他模块
    if (CURRENT_PRODUCT_TYPE != PRODUCT_GAME_LOW_END) {
        ret |= audio_core_init(&audio_cfg);     // 音频核心模块，负责音频解码和输出
        // 初始化音频源模块，使用默认配置
        AudioSourceConfig_t as_cfg = {
            .source_list = {SOURCE_BLUETOOTH, SOURCE_USB, SOURCE_HDMI_ARC, SOURCE_SPDIF, SOURCE_AUX, SOURCE_WIFI},
            .source_cnt = 6,
            .default_source = SOURCE_BLUETOOTH,
            .auto_switch_en = true
        };
        ret |= audio_source_init(&as_cfg);      // 音频源模块，负责管理各种音频输入源
        // 初始化音量控制模块，使用默认音量设置
        VolumeInfo_t default_vol = {
            .master_volume = DEFAULT_VOLUME_VAL,
            .bass_volume = DEFAULT_VOLUME_VAL,
            .treble_volume = DEFAULT_VOLUME_VAL,
            .is_mute = false
        };
        ret |= volume_ctrl_init(&default_vol);  // 音量控制模块，负责音量调节
        // 初始化播放控制模块，使用默认配置
        PlayCtrlConfig_t pc_cfg = {
            .power_off_resume_en = true,
            .boot_default_play_en = false,
            .bt_reconnect_timeout = 10,
            .default_mode = SOUND_MODE_NORMAL
        };
        ret |= play_ctrl_init(&pc_cfg);         // 播放控制模块，负责播放状态和音效
        ret |= comm_mcu_init();                 // MCU通信模块，负责与外设MCU通信
        ret |= system_init();                   // 系统管理模块，负责系统状态和异常处理
        ret |= prod_test_init();                // 生产测试模块，负责生产过程中的测试
    }

    // 7. 根据宏控配置初始化可选模块
    // 这些模块根据产品配置条件编译，实现不同产品的功能差异化
#ifdef CONFIG_ENABLE_HDMI_ARC
    ret |= hdmi_arc_init(&hdmi_cfg);        // HDMI ARC模块，负责HDMI音频接收
#endif
#ifdef CONFIG_ENABLE_SPDIF
    ret |= spdif_optical_init(&spdif_cfg); // SPDIF模块，负责光纤/同轴音频接收
#endif
#ifdef CONFIG_ENABLE_DOLBY_DTS
    // 初始化音效模块，使用默认配置
    SoundEffectsConfig_t se_cfg = {
        .dolby_en = true,
        .virtual_5_1_en = false
    };
    ret |= sound_effects_init(&se_cfg);    // 音效模块，负责杜比DTS解码和音效处理
#endif
#ifdef CONFIG_ENABLE_VOICE_NOISE_REDUCTION
    ret |= voice_noise_reduction_init();   // 语音降噪模块，负责语音处理
#endif
#ifdef CONFIG_ENABLE_WIFI_MEDIA
    // WIFI媒体配置，用于DLNA和AirPlay功能
    WifiMediaConfig_t wifi_cfg = {
        .wifi_name = "Aml_Soundbar",      // 设备名称
        .dlna_en = true,                   // 启用DLNA
        .airplay_en = true                 // 启用AirPlay
    };
    ret |= wifi_media_init(&wifi_cfg);     // WIFI媒体模块，负责DLNA和AirPlay
#endif
#ifdef CONFIG_ENABLE_BT_MESH
    // 初始化低音炮通信模块，使用默认配置
    SubwooferConfig_t sw_cfg = {
        .bt_name = "AML Subwoofer",
        .bass_gain = 50,
        .vol_sync_en = true,
        .auto_connect_en = true
    };
    ret |= subwoofer_comm_init(&sw_cfg);   // 低音炮通信模块，负责与低音炮的蓝牙通信
#endif

    // 8. 输出初始化结果
    LOG_INFO("All modules init: Product Type=%d, Status=%s", 
             CURRENT_PRODUCT_TYPE, ret == 0 ? "SUCCESS" : "WARN");
    // 返回初始化结果，0表示成功，非0表示失败
    return ret == 0 ? 0 : -1;
}

/**
 * @brief 模块反初始化总入口
 * @details 按照与初始化相反的顺序反初始化所有模块，确保资源正确释放
 * @return 无
 */
static void module_deinit_all(void) {
    // 1. 首先反初始化可选模块
    // 反初始化顺序与初始化顺序相反
#ifdef CONFIG_ENABLE_BT_MESH
    subwoofer_comm_deinit();          // 低音炮通信模块
#endif
#ifdef CONFIG_ENABLE_WIFI_MEDIA
    wifi_media_deinit();             // WIFI媒体模块
#endif
#ifdef CONFIG_ENABLE_DOLBY_DTS
    sound_effects_deinit();           // 音效模块
#endif
#ifdef CONFIG_ENABLE_VOICE_NOISE_REDUCTION
    voice_noise_reduction_deinit();   // 语音降噪模块
#endif
#ifdef CONFIG_ENABLE_SPDIF
    spdif_optical_deinit();          // SPDIF模块
#endif
#ifdef CONFIG_ENABLE_HDMI_ARC
    hdmi_arc_deinit();                // HDMI ARC模块
#endif

    // 2. 然后反初始化基础核心模块
    // 反初始化顺序：生产测试 -> 系统 -> MCU通信 -> 播放控制 -> 音量控制 -> 音频源 -> 音频核心 -> 蓝牙 -> 外设 -> 存储
    // 低端游戏音响极致裁剪，只反初始化核心模块
    if (CURRENT_PRODUCT_TYPE != PRODUCT_GAME_LOW_END) {
        prod_test_deinit();               // 生产测试模块
        system_deinit();                  // 系统管理模块
        comm_mcu_deinit();                // MCU通信模块
        play_ctrl_deinit();               // 播放控制模块
        volume_ctrl_deinit();             // 音量控制模块
        audio_source_deinit();            // 音频源模块
        audio_core_deinit();              // 音频核心模块
        bluetooth_deinit();               // 蓝牙模块
        peripheral_deinit();             // 外设管理模块
        storage_deinit();                // 存储管理模块
    } else {
        // 低端游戏音响：仅反初始化必要模块
        audio_core_deinit();              // 音频核心模块
        volume_ctrl_deinit();             // 音量控制模块
        system_deinit();                  // 系统管理模块
    }

    // 最后反初始化HAL和PAL层
    // 反初始化顺序与初始化顺序相反
    pal_deinit();              // 反初始化平台抽象层
    hal_deinit();              // 反初始化硬件抽象层

    LOG_INFO("✅ All modules deinit success");
    LOG_INFO("✅ HAL and PAL layers deinit success");
}

/**
 * @brief 业务主循环
 * @details 定期轮询各个模块的事件，处理系统的核心业务逻辑
 * @return 无
 */
static void main_business_loop(void) {
    LOG_INFO("Enter business loop...");
    
    // 启动资源监控
    resource_start_monitoring(1000);
    // 启动系统监控
    monitor_start(1000);
    
    // 主循环，直到系统运行状态为0时退出
    while (g_sys_running) {
        // 轮询各个模块的事件
        // 低端游戏音响极致裁剪，只轮询必要模块
        if (CURRENT_PRODUCT_TYPE != PRODUCT_GAME_LOW_END) {
            peripheral_event_poll();        // 外设事件：按键、红外等
            bluetooth_event_poll();         // 蓝牙事件：连接、媒体流等
            audio_source_event_poll();      // 音频源事件：源切换、状态变化等
            play_ctrl_event_poll();         // 播放控制事件：播放状态、音效等
            system_event_poll();            // 系统事件：系统状态、资源使用等
            
            // 轮询可选模块的事件
#ifdef CONFIG_ENABLE_BT_MESH
            subwoofer_comm_event_poll();    // 低音炮通信事件
#endif
#ifdef CONFIG_ENABLE_HDMI_ARC
            hdmi_arc_event_poll();          // HDMI ARC事件：连接、音频流等
#endif
#ifdef CONFIG_ENABLE_SPDIF
            spdif_optical_event_poll();     // SPDIF事件：连接、音频流等
#endif
        } else {
            // 低端游戏音响：仅轮询必要模块
            system_event_poll();            // 系统事件：系统状态、资源使用等
        }
        
        // 监控系统进程状态
        process_manager_monitor();
        
        // 休眠10毫秒，降低CPU占用
        usleep(10 * 1000);
    }
}

/**
 * @brief 系统主函数
 * @details 系统的入口函数，负责初始化系统、启动业务循环和优雅退出
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 系统退出码，0表示成功，非0表示失败
 */
int main(int argc, char *argv[]) {
    int ret = 0;
    ResInfo_t boot_tone = {0};

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    // 基础初始化
    // 日志系统已经通过aml_log.h初始化，不需要单独调用log_system_init
    LOG_INFO("Log system initialized via aml_log.h");
    ret = res_manager_init();
    if (ret != 0) {
        LOG_ERROR("Res manager init failed: %d", ret);
        return ret;
    }
    ret = event_system_init();
    if (ret != 0) {
        LOG_ERROR("Event system init failed: %d", ret);
        return ret;
    }
    
    // 初始化线程池，设置4个线程
    ret = thread_pool_init(4);
    if (ret != 0) {
        LOG_ERROR("Thread pool init failed: %d", ret);
        return ret;
    }
    
    // 初始化进程管理器
    ret = process_manager_init();
    if (ret != 0) {
        LOG_ERROR("Process manager init failed: %d", ret);
        return ret;
    }
    
    // 初始化IPC模块
    ret = ipc_init();
    if (ret != 0) {
        LOG_ERROR("IPC init failed: %d", ret);
        return ret;
    }
    
    // 初始化资源管理模块
    ret = resource_init();
    if (ret != 0) {
        LOG_ERROR("Resource init failed: %d", ret);
        return ret;
    }
    
    // 初始化监控和诊断模块
    ret = monitor_init();
    if (ret != 0) {
        LOG_ERROR("Monitor init failed: %d", ret);
        return ret;
    }

    // 启动信息
    LOG_INFO("=====================================================");
    LOG_INFO("AML Audio Speaker [V%s] Boot", SYSTEM_VERSION);
    LOG_INFO("Compile: %s %s | Product Type: %d", __DATE__, __TIME__, CURRENT_PRODUCT_TYPE);
    LOG_INFO("=====================================================");

    // 播放开机提示音 + 开机显示图标
    boot_tone = res_load_resource(RES_TYPE_TONE_BOOT);
    if (boot_tone.res_valid) {
        audio_core_play_tone(boot_tone.res_data, boot_tone.res_size);
        res_free_resource(&boot_tone);
    }

    // 初始化模块
    ret = module_init_all();
    if (ret != 0) {
        LOG_ERROR("Module init failed: %d", ret);
        goto exit_sys;
    }

    // 业务循环
    main_business_loop();

exit_sys:
    module_deinit_all();
    event_system_deinit();
    thread_pool_deinit();
    process_manager_deinit();
    // 反初始化新模块
    monitor_deinit();
    resource_deinit();
    ipc_deinit();
    res_manager_deinit();
    // 日志系统已经通过aml_log.h管理，不需要单独调用log_system_deinit

    LOG_INFO("✅ System Exit Success");
    return ret;
}