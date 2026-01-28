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

#include "../lib/flac/product_type.h"      // 产品类型定义
#include "../lib/flac/common_def.h"         // 通用定义
#include "../lib/flac/logger.h"            // 日志系统

// 模块头文件
#include "module_init.h"         // 模块初始化
#include "main_loop.h"           // 主循环

// 定义默认日志分类
AML_LOG_DEFINE(default_log);

/**
 * @brief 系统运行状态标志
 * @details 用于控制主循环的运行，0表示退出，1表示继续运行
 */
int g_sys_running = 1;

/**
 * @brief 依赖注入容器
 * @details 管理所有模块的实例，实现依赖注入
 */
typedef struct {
    // 内存管理
    void *memory_manager;
    // 错误处理
    void *error_handler;
    // 安全管理
    void *security_manager;
    // 功耗管理
    void *power_manager;
    // 配置管理
    void *config_manager;
    // 核心模块（使用ModuleState_e类型）
    ModuleState_e audio_core;
    ModuleState_e audio_source;
    ModuleState_e play_ctrl;
    ModuleState_e volume_ctrl;
    ModuleState_e peripheral;
    ModuleState_e storage;
    ModuleState_e bluetooth;
    ModuleState_e system;
    ModuleState_e comm_mcu;
    // 可选模块（使用ModuleState_e类型）
#ifdef CONFIG_ENABLE_WIFI_MEDIA
    ModuleState_e wifi_media;
#endif
#ifdef CONFIG_ENABLE_DOLBY_DTS
    ModuleState_e sound_effects;
#endif
#ifdef CONFIG_ENABLE_HDMI_ARC
    ModuleState_e hdmi_arc;
#endif
#ifdef CONFIG_ENABLE_SPDIF
    ModuleState_e spdif_optical;
#endif
#ifdef CONFIG_ENABLE_BT_MESH
    ModuleState_e subwoofer_comm;
#endif
#ifdef CONFIG_ENABLE_VOICE_NOISE_REDUCTION
    ModuleState_e voice_noise_reduction;
#endif
} DependencyContainer_t;

/**
 * @brief 依赖注入容器
 * @details 管理所有模块的实例，实现依赖注入
 */
DependencyContainer_t g_di_container = {0};

/**
 * @brief 主函数
 * @details 系统的入口点，负责初始化系统、启动主循环和处理优雅退出
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 退出状态码
 */
int main(int argc, char *argv[]) {
    int ret = 0;
    
    // 1. 初始化日志系统
    LOG_INFO("AML Audio Speaker System Starting...");
    LOG_INFO("Product Type: %d", CURRENT_PRODUCT_TYPE);
    
    // 2. 注册信号处理函数
    signal(SIGINT, sig_handler);   // 处理Ctrl+C
    signal(SIGTERM, sig_handler);  // 处理终止信号
    
    // 3. 初始化所有模块
    ret = module_init_all();
    if (ret != 0) {
        LOG_ERROR("Module init failed, error code: %d", ret);
        // 模块初始化失败，尝试反初始化已初始化的模块
        module_deinit_all();
        return -1;
    }
    
    // 4. 启动业务主循环
    main_business_loop();
    
    // 5. 反初始化所有模块
    module_deinit_all();
    
    // 6. 输出退出信息
    LOG_INFO("AML Audio Speaker System Exited");
    
    return 0;
}
