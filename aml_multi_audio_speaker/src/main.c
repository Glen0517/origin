#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include "product_type.h"
#include "common_def.h"
#include "logger.h"
#include "res_manager.h"

// 公共对外头文件
#include "audio_core.h"
#include "audio_source.h"
#include "play_ctrl.h"
#include "volume_ctrl.h"
#include "peripheral.h"
#include "storage.h"
#include "bluetooth.h"
#include "system.h"
#include "comm_mcu.h"  

// 宏控按需加载头文件
#ifdef CONFIG_ENABLE_WIFI_MEDIA
#include "wifi_media.h"
#endif
#ifdef CONFIG_ENABLE_DOLBY_DTS
#include "sound_effects.h"
#endif
#ifdef CONFIG_ENABLE_HDMI_ARC
#include "hdmi_arc.h"
#endif
#ifdef CONFIG_ENABLE_SPDIF
#include "spdif_optical.h"
#endif
#ifdef CONFIG_ENABLE_BT_MESH
#include "subwoofer_comm.h"
#endif

#include "prod_test.h"

static int g_sys_running = 1;

/**
 * @brief 信号处理：优雅退出
 */
static void sig_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        LOG_INFO("System receive exit signal [%d], start deinit...", sig);
        g_sys_running = 0;
    }
}

/**
 * @brief 模块初始化总入口（宏控适配所有产品）
 */
static int module_init_all(void) {
    int ret = 0;
    // 基础核心模块
    ret |= storage_init();
    ret |= peripheral_init();
    ret |= bluetooth_init();
    ret |= audio_core_init();
    ret |= audio_source_init();
    ret |= volume_ctrl_init();
    ret |= play_ctrl_init();
    ret |= comm_mcu_init();  // 适配截图：原uart_mcu_comm_init
    ret |= system_init();
    ret |= prod_test_init();

    // 宏控加载模块
#ifdef CONFIG_ENABLE_HDMI_ARC
    ret |= hdmi_arc_init();
#endif
#ifdef CONFIG_ENABLE_SPDIF
    ret |= spdif_optical_init();
#endif
#ifdef CONFIG_ENABLE_DOLBY_DTS
    ret |= sound_effects_init();
#endif
#ifdef CONFIG_ENABLE_WIFI_MEDIA
    ret |= wifi_media_init();
#endif
#ifdef CONFIG_ENABLE_BT_MESH
    ret |= subwoofer_comm_init();
#endif

    LOG_INFO("✅ All modules init: Product Type=%d, Status=%s", 
             CURRENT_PRODUCT_TYPE, ret == 0 ? "SUCCESS" : "WARN");
    return ret == 0 ? 0 : -1;
}

/**
 * @brief 模块反初始化总入口
 */
static void module_deinit_all(void) {
#ifdef CONFIG_ENABLE_BT_MESH
    subwoofer_comm_deinit();
#endif
#ifdef CONFIG_ENABLE_WIFI_MEDIA
    wifi_media_deinit();
#endif
#ifdef CONFIG_ENABLE_DOLBY_DTS
    sound_effects_deinit();
#endif
#ifdef CONFIG_ENABLE_SPDIF
    spdif_optical_deinit();
#endif
#ifdef CONFIG_ENABLE_HDMI_ARC
    hdmi_arc_deinit();
#endif

    prod_test_deinit();
    system_deinit();
    comm_mcu_deinit();  // 适配截图：原uart_mcu_comm_deinit
    play_ctrl_deinit();
    volume_ctrl_deinit();
    audio_source_deinit();
    audio_core_deinit();
    bluetooth_deinit();
    peripheral_deinit();
    storage_deinit();

    LOG_INFO("✅ All modules deinit success");
}

/**
 * @brief 业务主循环
 */
static void main_business_loop(void) {
    LOG_INFO("Enter business loop...");
    while (g_sys_running) {
        peripheral_event_poll();
        bluetooth_event_poll();
        audio_source_event_poll();
        play_ctrl_event_poll();
        system_event_poll();
#ifdef CONFIG_ENABLE_BT_MESH
        subwoofer_comm_event_poll();
#endif
        usleep(10 * 1000);
    }
}

/**
 * @brief 主函数
 */
int main(int argc, char *argv[]) {
    int ret = 0;
    ResInfo_t boot_tone = {0};

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    // 基础初始化
    ret = log_system_init();
    if (ret != 0) { fprintf(stderr, "Log init failed: %d\n", ret); return ret; }
    ret = res_manager_init();
    if (ret != 0) { LOG_ERROR("Res manager init failed: %d", ret); return ret; }

    // 启动信息
    LOG_INFO("=====================================================");
    LOG_INFO("AML Audio Speaker [V%s] Boot", SYSTEM_VERSION);
    LOG_INFO("Compile: %s %s | Product Type: %d", __DATE__, __TIME__, CURRENT_PRODUCT_TYPE);
    LOG_INFO("=====================================================");

    // 播放开机提示音
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
    res_manager_deinit();
    log_system_deinit();

    LOG_INFO("✅ System Exit Success");
    return ret;
}