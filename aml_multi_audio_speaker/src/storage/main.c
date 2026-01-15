#include "config_save.h"
#include "product_type.h"

int main(int argc, char *argv[])
{
    // ========== 第一步：初始化配置【必须最先执行】 ==========
    config_init();

    // 获取全局配置句柄，供其他模块使用
    GlobalConfig_t *g_cfg = config_get_global();

    // 打印配置信息，调试用
    printf("[MAIN] Default sample rate: %d\n", g_cfg->audio.default_sample_rate);
    printf("[MAIN] Default source: %s\n", g_cfg->source.default_source);
    printf("[MAIN] Enable HDMI ARC: %d\n", g_cfg->product_cap.enable_hdmi_arc);

    // 初始化其他模块：音频、蓝牙、外设、播放控制...
    audio_core_init(g_cfg);
    bluetooth_init(g_cfg);
    peripheral_init(g_cfg);
    play_ctrl_init(g_cfg);

    // 主循环
    while (1) {
        main_loop();
    }

    // 程序退出
    config_deinit();
    return 0;
}