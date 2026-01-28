/**
 * @file module_init.c
 * @brief 模块初始化模块实现
 * @details 提供模块初始化和反初始化的功能
 * @author AML Audio Team
 * @date 2026-01-28
 */

#include "module_init.h"

// 外部头文件
#include "config_manager.h"
#include "memory_manager.h"
#include "error_handler.h"
#include "security_manager.h"
#include "power_manager.h"

// 内存池默认配置
#define DEFAULT_MEMORY_POOL_BLOCK_SIZE 256
#define DEFAULT_MEMORY_POOL_BLOCK_COUNT 1024
#define DEFAULT_MEMORY_POOL_ENABLED true

// HAL和PAL层头文件
#include "hal/include/hal.h"
#include "pal/include/pal.h"

// 核心功能模块头文件
#include "audio_core.h"
#include "audio_source.h"
#include "play_ctrl.h"
#include "volume_ctrl.h"
#include "peripheral.h"
#include "storage.h"
#include "bt.h"
#include "system.h"
#include "comm_mcu.h"

// 可选模块头文件
#ifdef CONFIG_ENABLE_HDMI_ARC
#include "hdmi_arc.h"
#endif
#ifdef CONFIG_ENABLE_SPDIF
#include "spdif_optical.h"
#endif
#ifdef CONFIG_ENABLE_DOLBY_DTS
#include "sound_effects.h"
#endif
#ifdef CONFIG_ENABLE_VOICE_NOISE_REDUCTION
#include "voice_noise_reduction.h"
#endif
#ifdef CONFIG_ENABLE_WIFI_MEDIA
#include "wifi_media.h"
#endif
#ifdef CONFIG_ENABLE_BT_MESH
#include "subwoofer_comm.h"
#endif

#include "prod_test.h"

// 包含配置项路径常量定义
#include "config_constants.h"

// 包含依赖注入容器定义
#include "dependency_injection.h"

// 全局依赖注入容器
DependencyContainer_t g_di_container;

// 初始化依赖注入容器
void dependency_container_init(void) {
    g_di_container.memory_manager = NULL;
    g_di_container.config_manager = NULL;
    g_di_container.error_handler = NULL;
    g_di_container.security_manager = NULL;
    g_di_container.power_manager = NULL;
    
    g_di_container.storage = MODULE_STATE_UNINIT;
    g_di_container.peripheral = MODULE_STATE_UNINIT;
    g_di_container.bluetooth = MODULE_STATE_UNINIT;
    g_di_container.audio_core = MODULE_STATE_UNINIT;
    g_di_container.audio_source = MODULE_STATE_UNINIT;
    g_di_container.volume_ctrl = MODULE_STATE_UNINIT;
    g_di_container.play_ctrl = MODULE_STATE_UNINIT;
    g_di_container.comm_mcu = MODULE_STATE_UNINIT;
    g_di_container.system = MODULE_STATE_UNINIT;
    
#if 0
    g_di_container.hdmi_arc = MODULE_STATE_UNINIT;
    g_di_container.spdif_optical = MODULE_STATE_UNINIT;
    g_di_container.sound_effects = MODULE_STATE_UNINIT;
    g_di_container.voice_noise_reduction = MODULE_STATE_UNINIT;
    g_di_container.wifi_media = MODULE_STATE_UNINIT;
    g_di_container.subwoofer_comm = MODULE_STATE_UNINIT;
#endif
}

/**
 * @brief 模块初始化总入口
 * @details 根据宏控配置初始化所有启用的模块，适配不同产品类型
 * @return 初始化结果，0表示成功，非0表示失败
 */
int module_init_all(void) {
    int ret = 0;
    
    // 1. 初始化依赖注入容器
    dependency_container_init();
    
    // 2. 初始化HAL和PAL层
    // HAL层：硬件抽象层，封装硬件相关操作
    // PAL层：平台抽象层，封装平台相关服务
    ret |= hal_init();              // 初始化硬件抽象层
    ret |= pal_init();              // 初始化平台抽象层
    
    // 2. 初始化内存管理器
    // 内存管理器负责内存分配、释放和泄漏检测
    // 根据产品类型设置不同的内存池配置
    MemoryPoolConfig_t mem_pool_config = {
        .block_size = DEFAULT_MEMORY_POOL_BLOCK_SIZE,
        .block_count = DEFAULT_MEMORY_POOL_BLOCK_COUNT,
        .enable_memory_pool = DEFAULT_MEMORY_POOL_ENABLED
    };
    
    // 高端产品使用更大的内存池
    #ifdef CONFIG_ENABLE_GAME_SPEAKER
    if (CURRENT_PRODUCT_TYPE == PRODUCT_GAME_HIGH_END) {
        mem_pool_config.block_size = 512;
        mem_pool_config.block_count = 2048;
    }
    #endif
    
    g_di_container.memory_manager = memory_manager_init(&mem_pool_config);
    if (!g_di_container.memory_manager) {
        LOG_ERROR("Memory manager init failed");
        ret |= -1;
    }
    
    // 3. 初始化错误处理器
    // 错误处理器负责错误统计和恢复
    g_di_container.error_handler = error_handler_init();
    if (!g_di_container.error_handler) {
        LOG_ERROR("Error handler init failed");
        ret |= -1;
    }
    
    // 4. 初始化安全管理器
    // 安全管理器负责安全检查和更新
    g_di_container.security_manager = security_manager_init();
    if (!g_di_container.security_manager) {
        LOG_ERROR("Security manager init failed");
        ret |= -1;
    }
    
    // 5. 初始化功耗管理器
    // 功耗管理器负责动态功耗管理
    g_di_container.power_manager = power_manager_init();
    if (!g_di_container.power_manager) {
        LOG_ERROR("Power manager init failed");
        ret |= -1;
    }
    
    // 6. 初始化配置管理器
    // 配置管理器负责读取和解析配置文件，提供配置项访问
    g_di_container.config_manager = config_manager_init("/etc/aml_audio/config.json");
    if (!g_di_container.config_manager) {
        LOG_ERROR("Config manager init failed");
        ret |= -1;
    }
    
    // 3. 从配置文件加载配置
    // 创建并初始化音频核心配置
    AudioCoreConfig_t audio_cfg = {
        .sample_rate = config_manager_get_int(g_di_container.config_manager, CONFIG_AUDIO_SAMPLE_RATE, 48000),
        .channel_num = config_manager_get_int(g_di_container.config_manager, CONFIG_AUDIO_CHANNEL_NUM, 2),
        .pcm_buffer_size = config_manager_get_int(g_di_container.config_manager, CONFIG_AUDIO_PCM_BUFFER_SIZE, 4096),
        .hw_decode_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_AUDIO_HW_DECODE_EN, true),
        .dolby_dts_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_AUDIO_DOLBY_DTS_EN, false)
    };
    
    // 创建并初始化HDMI ARC配置
#ifdef CONFIG_ENABLE_HDMI_ARC
    HdmiArcConfig_t hdmi_cfg = {
        .cec_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_HDMI_CEC_EN, true),
        .auto_switch_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_HDMI_AUTO_SWITCH_EN, true),
        .sample_rate = config_manager_get_int(g_di_container.config_manager, CONFIG_HDMI_SAMPLE_RATE, 48000)
    };
#endif
    
    // 创建并初始化SPDIF配置
#ifdef CONFIG_ENABLE_SPDIF
    SpdifConfig_t spdif_cfg = {
        .auto_switch_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_SPDIF_AUTO_SWITCH_EN, true),
        .sample_rate = config_manager_get_int(g_di_container.config_manager, CONFIG_SPDIF_SAMPLE_RATE, 48000),
        .bits_per_sample = config_manager_get_int(g_di_container.config_manager, CONFIG_SPDIF_BITS_PER_SAMPLE, 16)
    };
#endif
    
    // 创建并初始化外设配置
    PeripheralConfig_t peri_cfg = {
        .key_debounce_ms = config_manager_get_int(g_di_container.config_manager, CONFIG_PERIPHERAL_KEY_DEBOUNCE_MS, 50),
        .long_press_ms = config_manager_get_int(g_di_container.config_manager, CONFIG_PERIPHERAL_LONG_PRESS_MS, 1000),
        .ir_learn_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_PERIPHERAL_IR_LEARN_EN, true),
        .mic_mute_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_PERIPHERAL_MIC_MUTE_EN, true)
    };
    
    // 4. 根据产品类型调整配置
    switch (CURRENT_PRODUCT_TYPE) {
#ifdef CONFIG_ENABLE_GAME_SPEAKER
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
#endif
        default:
            // 其他产品类型保持默认配置
            break;
    }
    
    // 5. 初始化基础核心模块（使用依赖注入）
    // 初始化顺序：存储 -> 外设 -> 蓝牙 -> 音频核心 -> 音频源 -> 音量控制 -> 播放控制 -> MCU通信 -> 系统 -> 生产测试
    // 非游戏音响或非低端游戏音响初始化基础模块
#ifdef CONFIG_ENABLE_GAME_SPEAKER
    if (CURRENT_PRODUCT_TYPE != PRODUCT_GAME_LOW_END) {
#else
    {
#endif
        // 存储模块初始化
        if (storage_init() == SUCCESS) {
            g_di_container.storage = MODULE_STATE_INIT_SUCCESS;
        } else {
            g_di_container.storage = MODULE_STATE_INIT_FAILED;
            ret |= -1;
        }
        
        // 外设模块初始化
        if (peripheral_init(&peri_cfg) == SUCCESS) {
            g_di_container.peripheral = MODULE_STATE_INIT_SUCCESS;
        } else {
            g_di_container.peripheral = MODULE_STATE_INIT_FAILED;
            ret |= -1;
        }
        
        // 蓝牙模块初始化，使用配置文件中的配置
        BluetoothConfig_t bt_cfg = {
            .bt_name = config_manager_get_string(g_di_container.config_manager, CONFIG_BLUETOOTH_BT_NAME, "AML Audio Speaker"),
            .bt_pin = config_manager_get_string(g_di_container.config_manager, CONFIG_BLUETOOTH_BT_PIN, "0000"),
            .bt_auto_connect = config_manager_get_bool(g_di_container.config_manager, CONFIG_BLUETOOTH_BT_AUTO_CONNECT, true),
            .bt_mesh_en = false
        };
#ifdef CONFIG_ENABLE_BT_MESH
        bt_cfg.bt_mesh_en = true;
#endif
        if (bluetooth_init(&bt_cfg) == SUCCESS) {
            g_di_container.bluetooth = MODULE_STATE_INIT_SUCCESS;
        } else {
            g_di_container.bluetooth = MODULE_STATE_INIT_FAILED;
            ret |= -1;
        }
    }

#ifdef CONFIG_ENABLE_GAME_SPEAKER
    // 低端游戏音响：仅初始化必要模块
    if (CURRENT_PRODUCT_TYPE == PRODUCT_GAME_LOW_END) {
        // 音频核心模块初始化
        if (audio_core_init(&audio_cfg) == SUCCESS) {
            g_di_container.audio_core = MODULE_STATE_INIT_SUCCESS;
        } else {
            g_di_container.audio_core = MODULE_STATE_INIT_FAILED;
            ret |= -1;
        }
        
        // 音量控制模块初始化，使用默认音量设置
        VolumeInfo_t default_vol = {
            .master_volume = config_manager_get_int(g_di_container.config_manager, CONFIG_VOLUME_MASTER_VOLUME, DEFAULT_VOLUME_VAL),
            .bass_volume = config_manager_get_int(g_di_container.config_manager, CONFIG_VOLUME_BASS_VOLUME, DEFAULT_VOLUME_VAL),
            .treble_volume = config_manager_get_int(g_di_container.config_manager, CONFIG_VOLUME_TREBLE_VOLUME, DEFAULT_VOLUME_VAL),
            .is_mute = config_manager_get_bool(g_di_container.config_manager, CONFIG_VOLUME_IS_MUTE, false)
        };
        if (volume_ctrl_init(&default_vol) == SUCCESS) {
            g_di_container.volume_ctrl = MODULE_STATE_INIT_SUCCESS;
        } else {
            g_di_container.volume_ctrl = MODULE_STATE_INIT_FAILED;
            ret |= -1;
        }
        
        // 系统模块初始化
        if (system_init() == SUCCESS) {
            g_di_container.system = MODULE_STATE_INIT_SUCCESS;
        } else {
            g_di_container.system = MODULE_STATE_INIT_FAILED;
            ret |= -1;
        }
        
        return ret;
    }
#endif
    
    // 非低端游戏音响初始化其他模块
    // 音频核心模块初始化
    if (audio_core_init(&audio_cfg) == SUCCESS) {
        g_di_container.audio_core = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.audio_core = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
    
    // 初始化音频源模块，使用配置文件中的配置
    AudioSourceConfig_t as_cfg = {
        .source_list = {SOURCE_BLUETOOTH, SOURCE_USB, SOURCE_HDMI_ARC, SOURCE_SPDIF, SOURCE_AUX, SOURCE_WIFI},
        .source_cnt = 6,
        .default_source = config_manager_get_int(g_di_container.config_manager, CONFIG_AUDIO_DEFAULT_SOURCE, SOURCE_BLUETOOTH),
        .auto_switch_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_AUDIO_AUTO_SWITCH_EN, true)
    };
    if (audio_source_init(&as_cfg) == SUCCESS) {
        g_di_container.audio_source = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.audio_source = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
    
    // 初始化音量控制模块，使用配置文件中的配置
    VolumeInfo_t default_vol = {
        .master_volume = config_manager_get_int(g_di_container.config_manager, CONFIG_VOLUME_MASTER_VOLUME, DEFAULT_VOLUME_VAL),
        .bass_volume = config_manager_get_int(g_di_container.config_manager, CONFIG_VOLUME_BASS_VOLUME, DEFAULT_VOLUME_VAL),
        .treble_volume = config_manager_get_int(g_di_container.config_manager, CONFIG_VOLUME_TREBLE_VOLUME, DEFAULT_VOLUME_VAL),
        .is_mute = config_manager_get_bool(g_di_container.config_manager, CONFIG_VOLUME_IS_MUTE, false)
    };
    if (volume_ctrl_init(&default_vol) == SUCCESS) {
        g_di_container.volume_ctrl = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.volume_ctrl = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
    
    // 初始化播放控制模块
    PlayCtrlConfig_t play_cfg = {
        .power_off_resume_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_PLAY_POWER_OFF_RESUME_EN, true),
        .boot_default_play_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_PLAY_BOOT_DEFAULT_PLAY_EN, false),
        .bt_reconnect_timeout = config_manager_get_int(g_di_container.config_manager, CONFIG_PLAY_BT_RECONNECT_TIMEOUT, 10),
        .default_mode = config_manager_get_int(g_di_container.config_manager, CONFIG_PLAY_DEFAULT_MODE, SOUND_MODE_NORMAL)
    };
    if (play_ctrl_init(&play_cfg) == SUCCESS) {
        g_di_container.play_ctrl = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.play_ctrl = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
    
    // MCU通信模块初始化
    if (comm_mcu_init() == SUCCESS) {
        g_di_container.comm_mcu = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.comm_mcu = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
    
    // 系统模块初始化
    if (system_init() == SUCCESS) {
        g_di_container.system = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.system = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
    
    // 生产测试模块初始化
    if (prod_test_init() == SUCCESS) {
        // 生产测试模块不需要在容器中记录状态
    } else {
        ret |= -1;
    }

    // 6. 根据宏控配置初始化可选模块
#ifdef CONFIG_ENABLE_HDMI_ARC
    // 初始化HDMI ARC模块
    if (hdmi_arc_init(&hdmi_cfg) == SUCCESS) {
        g_di_container.hdmi_arc = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.hdmi_arc = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
#endif
#ifdef CONFIG_ENABLE_SPDIF
    // 初始化SPDIF模块
    if (spdif_optical_init(&spdif_cfg) == SUCCESS) {
        g_di_container.spdif_optical = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.spdif_optical = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
#endif
#ifdef CONFIG_ENABLE_DOLBY_DTS
    // 初始化音效模块
    SoundEffectsConfig_t se_cfg = {
        .dolby_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_SOUND_DOLBY_EN, true),
        .virtual_5_1_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_SOUND_VIRTUAL_5_1_EN, false)
    };
    if (sound_effects_init(&se_cfg) == SUCCESS) {
        g_di_container.sound_effects = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.sound_effects = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
#endif
#ifdef CONFIG_ENABLE_VOICE_NOISE_REDUCTION
    // 初始化语音降噪模块
    if (voice_noise_reduction_init() == SUCCESS) {
        g_di_container.voice_noise_reduction = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.voice_noise_reduction = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
#endif
#ifdef CONFIG_ENABLE_WIFI_MEDIA
    // 初始化WIFI媒体模块
    WifiMediaConfig_t wifi_cfg = {
        .wifi_name = config_manager_get_string(g_di_container.config_manager, CONFIG_WIFI_WIFI_NAME, "Aml_Soundbar"),
        .dlna_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_WIFI_DLNA_EN, true),
        .airplay_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_WIFI_AIRPLAY_EN, true)
    };
    if (wifi_media_init(&wifi_cfg) == SUCCESS) {
        g_di_container.wifi_media = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.wifi_media = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
#endif
#ifdef CONFIG_ENABLE_BT_MESH
    // 初始化低音炮通信模块
    SubwooferConfig_t sw_cfg = {
        .bt_name = config_manager_get_string(g_di_container.config_manager, CONFIG_SUBWOOFER_BT_NAME, "AML Subwoofer"),
        .bass_gain = config_manager_get_int(g_di_container.config_manager, CONFIG_SUBWOOFER_BASS_GAIN, 50),
        .vol_sync_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_SUBWOOFER_VOL_SYNC_EN, true),
        .auto_connect_en = config_manager_get_bool(g_di_container.config_manager, CONFIG_SUBWOOFER_AUTO_CONNECT_EN, true)
    };
    if (subwoofer_comm_init(&sw_cfg) == SUCCESS) {
        g_di_container.subwoofer_comm = MODULE_STATE_INIT_SUCCESS;
    } else {
        g_di_container.subwoofer_comm = MODULE_STATE_INIT_FAILED;
        ret |= -1;
    }
#endif

    // 7. 输出初始化结果
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
void module_deinit_all(void) {
    // 1. 首先反初始化可选模块
    // 反初始化顺序与初始化顺序相反
#ifdef CONFIG_ENABLE_BT_MESH
    if (g_di_container.subwoofer_comm == MODULE_STATE_INIT_SUCCESS) {
        subwoofer_comm_deinit();
        g_di_container.subwoofer_comm = MODULE_STATE_UNINIT;
    }
#endif
#ifdef CONFIG_ENABLE_WIFI_MEDIA
    if (g_di_container.wifi_media == MODULE_STATE_INIT_SUCCESS) {
        wifi_media_deinit();
        g_di_container.wifi_media = MODULE_STATE_UNINIT;
    }
#endif
#ifdef CONFIG_ENABLE_DOLBY_DTS
    if (g_di_container.sound_effects == MODULE_STATE_INIT_SUCCESS) {
        sound_effects_deinit();
        g_di_container.sound_effects = MODULE_STATE_UNINIT;
    }
#endif
#ifdef CONFIG_ENABLE_VOICE_NOISE_REDUCTION
    if (g_di_container.voice_noise_reduction == MODULE_STATE_INIT_SUCCESS) {
        voice_noise_reduction_deinit();
        g_di_container.voice_noise_reduction = MODULE_STATE_UNINIT;
    }
#endif
#ifdef CONFIG_ENABLE_SPDIF
    if (g_di_container.spdif_optical == MODULE_STATE_INIT_SUCCESS) {
        spdif_optical_deinit();
        g_di_container.spdif_optical = MODULE_STATE_UNINIT;
    }
#endif
#ifdef CONFIG_ENABLE_HDMI_ARC
    if (g_di_container.hdmi_arc == MODULE_STATE_INIT_SUCCESS) {
        hdmi_arc_deinit();
        g_di_container.hdmi_arc = MODULE_STATE_UNINIT;
    }
#endif

    // 2. 然后反初始化基础核心模块
    // 反初始化顺序：生产测试 -> 系统 -> MCU通信 -> 播放控制 -> 音量控制 -> 音频源 -> 音频核心 -> 蓝牙 -> 外设 -> 存储
    // 非游戏音响或非低端游戏音响反初始化所有模块
#ifdef CONFIG_ENABLE_GAME_SPEAKER
    if (CURRENT_PRODUCT_TYPE != PRODUCT_GAME_LOW_END) {
#else
    {
#endif
        prod_test_deinit();               // 生产测试模块
        
        if (g_di_container.system == MODULE_STATE_INIT_SUCCESS) {
            system_deinit();
            g_di_container.system = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.comm_mcu == MODULE_STATE_INIT_SUCCESS) {
            comm_mcu_deinit();
            g_di_container.comm_mcu = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.play_ctrl == MODULE_STATE_INIT_SUCCESS) {
            play_ctrl_deinit();
            g_di_container.play_ctrl = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.volume_ctrl == MODULE_STATE_INIT_SUCCESS) {
            volume_ctrl_deinit();
            g_di_container.volume_ctrl = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.audio_source == MODULE_STATE_INIT_SUCCESS) {
            audio_source_deinit();
            g_di_container.audio_source = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.audio_core == MODULE_STATE_INIT_SUCCESS) {
            audio_core_deinit();
            g_di_container.audio_core = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.bluetooth == MODULE_STATE_INIT_SUCCESS) {
            bluetooth_deinit();
            g_di_container.bluetooth = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.peripheral == MODULE_STATE_INIT_SUCCESS) {
            peripheral_deinit();
            g_di_container.peripheral = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.storage == MODULE_STATE_INIT_SUCCESS) {
            storage_deinit();
            g_di_container.storage = MODULE_STATE_UNINIT;
        }
    }

#ifdef CONFIG_ENABLE_GAME_SPEAKER
    // 低端游戏音响：仅反初始化必要模块
    if (CURRENT_PRODUCT_TYPE == PRODUCT_GAME_LOW_END) {
        if (g_di_container.audio_core == MODULE_STATE_INIT_SUCCESS) {
            audio_core_deinit();
            g_di_container.audio_core = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.volume_ctrl == MODULE_STATE_INIT_SUCCESS) {
            volume_ctrl_deinit();
            g_di_container.volume_ctrl = MODULE_STATE_UNINIT;
        }
        
        if (g_di_container.system == MODULE_STATE_INIT_SUCCESS) {
            system_deinit();
            g_di_container.system = MODULE_STATE_UNINIT;
        }
    }
#endif

    // 3. 反初始化配置管理器
    if (g_di_container.config_manager) {
        config_manager_deinit(g_di_container.config_manager);
        g_di_container.config_manager = NULL;
    }
    
    // 4. 反初始化内存管理器
    if (g_di_container.memory_manager) {
        memory_manager_deinit(g_di_container.memory_manager);
        g_di_container.memory_manager = NULL;
    }
    
    // 5. 反初始化错误处理器
    if (g_di_container.error_handler) {
        error_handler_deinit(g_di_container.error_handler);
        g_di_container.error_handler = NULL;
    }
    
    // 6. 反初始化安全管理器
    if (g_di_container.security_manager) {
        security_manager_deinit(g_di_container.security_manager);
        g_di_container.security_manager = NULL;
    }
    
    // 7. 反初始化功耗管理器
    if (g_di_container.power_manager) {
        power_manager_deinit(g_di_container.power_manager);
        g_di_container.power_manager = NULL;
    }

    // 最后反初始化HAL和PAL层
    // 反初始化顺序与初始化顺序相反
    pal_deinit();              // 反初始化平台抽象层
    hal_deinit();              // 反初始化硬件抽象层

    LOG_INFO("✅ All modules deinit success");
    LOG_INFO("✅ HAL and PAL layers deinit success");
}
