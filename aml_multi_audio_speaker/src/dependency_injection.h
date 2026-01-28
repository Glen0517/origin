/**
 * @file dependency_injection.h
 * @brief 依赖注入容器头文件
 * @details 提供全局依赖注入容器的定义，用于模块间的依赖管理
 * @author AML Audio Team
 * @date 2026-01-28
 */

#ifndef __DEPENDENCY_INJECTION_H__
#define __DEPENDENCY_INJECTION_H__

#include "module_init.h"
#include "memory_manager.h"
#include "config_manager.h"
#include "error_handler.h"
#include "security_manager.h"
#include "power_manager.h"

// 依赖注入容器结构体
typedef struct {
    // 核心管理器
    MemoryManager_t *memory_manager;
    ConfigManager_t *config_manager;
    ErrorHandler_t *error_handler;
    SecurityManager_t *security_manager;
    PowerManager_t *power_manager;
    
    // 基础核心模块状态
    ModuleState_e storage;
    ModuleState_e peripheral;
    ModuleState_e bluetooth;
    ModuleState_e audio_core;
    ModuleState_e audio_source;
    ModuleState_e volume_ctrl;
    ModuleState_e play_ctrl;
    ModuleState_e comm_mcu;
    ModuleState_e system;
    
    // 可选模块状态
#ifdef CONFIG_ENABLE_HDMI_ARC
    ModuleState_e hdmi_arc;
#endif
#ifdef CONFIG_ENABLE_SPDIF
    ModuleState_e spdif_optical;
#endif
#ifdef CONFIG_ENABLE_DOLBY_DTS
    ModuleState_e sound_effects;
#endif
#ifdef CONFIG_ENABLE_VOICE_NOISE_REDUCTION
    ModuleState_e voice_noise_reduction;
#endif
#ifdef CONFIG_ENABLE_WIFI_MEDIA
    ModuleState_e wifi_media;
#endif
#ifdef CONFIG_ENABLE_BT_MESH
    ModuleState_e subwoofer_comm;
#endif
} DependencyContainer_t;

// 初始化依赖注入容器
void dependency_container_init(void);

#endif // __DEPENDENCY_INJECTION_H__
