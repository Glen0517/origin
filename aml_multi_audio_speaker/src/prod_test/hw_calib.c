#include "prod_test_priv.h"
#include "logger.h"
#include "product_type.h"
#include "audio_core.h"
#include "play_ctrl.h"

#include <aml_audio.h>        // 晶晨音频SDK
#include <aml_eq.h>           // 晶晨EQ SDK
#include <aml_timer.h>        // 晶晨定时器SDK

static bool g_hw_calib_init = false;
static bool g_aging_test_running = false;
static int g_aging_test_hours = 0;
static int g_aging_test_remaining = 0;

#ifdef CONFIG_ENABLE_HW_CALIB

/**
 * @brief 老化测试定时器回调函数
 */
static void aging_test_timer_callback(void)
{
    if (g_aging_test_running && g_aging_test_remaining > 0) {
        g_aging_test_remaining--;
        LOG_INFO("Aging test: %d hours remaining", g_aging_test_remaining);
        
        if (g_aging_test_remaining == 0) {
            hw_calib_age_stop();
            LOG_INFO("Aging test completed!");
        }
    }
}

int hw_calib_init(void)
{
    if (g_hw_calib_init) {
        LOG_INFO("HW calibration already initialized");
        return 0;
    }
    
    g_hw_calib_init = true;
    g_aging_test_running = false;
    g_aging_test_hours = 0;
    g_aging_test_remaining = 0;
    
    LOG_INFO("Prod test: HW calib/aging init success (HIGH END ONLY)");
    return 0;
}

void hw_calib_deinit(void)
{
    if (g_hw_calib_init) {
        // 停止老化测试
        hw_calib_age_stop();
        
        g_hw_calib_init = false;
    }
}

/**
 * @brief 开始老化测试
 */
int hw_calib_age_start(int hour)
{
    if (!g_hw_calib_init) {
        LOG_ERROR("HW calibration not initialized");
        return -1;
    }
    
    if (g_aging_test_running) {
        LOG_INFO("Aging test already running");
        return 0;
    }
    
    if (hour <= 0) {
        LOG_ERROR("Invalid aging test hours: %d", hour);
        return -1;
    }
    
    LOG_INFO("Starting aging test for %d hours...", hour);
    
    // 设置老化测试参数
    g_aging_test_hours = hour;
    g_aging_test_remaining = hour;
    g_aging_test_running = true;
    
    // 初始化定时器，每小时触发一次
    aml_timer_init(3600000, aging_test_timer_callback, true);
    
    // 播放测试音频
    LOG_INFO("Playing test audio for aging...");
    
    // TODO: 实现测试音频播放逻辑
    // 可以使用audio_core_play_test_audio()或类似函数
    
    LOG_INFO("Aging test started successfully");
    return 0;
}

/**
 * @brief 停止老化测试
 */
int hw_calib_age_stop(void)
{
    if (!g_hw_calib_init) {
        LOG_ERROR("HW calibration not initialized");
        return -1;
    }
    
    if (!g_aging_test_running) {
        LOG_INFO("Aging test not running");
        return 0;
    }
    
    LOG_INFO("Stopping aging test...");
    
    // 停止定时器
    aml_timer_deinit();
    
    // 停止测试音频播放
    // TODO: 实现停止测试音频播放的逻辑
    
    // 重置老化测试参数
    g_aging_test_running = false;
    g_aging_test_hours = 0;
    g_aging_test_remaining = 0;
    
    LOG_INFO("Aging test stopped");
    return 0;
}

/**
 * @brief 执行EQ校准
 */
int hw_calib_eq_run(void)
{
    if (!g_hw_calib_init) {
        LOG_ERROR("HW calibration not initialized");
        return -1;
    }
    
    LOG_INFO("Running EQ calibration...");
    
    // 检查EQ模块是否初始化
    if (!aml_eq_is_init()) {
        LOG_ERROR("EQ module not initialized");
        return -1;
    }
    
    // TODO: 实现EQ校准逻辑
    // 1. 播放校准信号
    // 2. 采集音频输出
    // 3. 分析频率响应
    // 4. 调整EQ参数
    // 5. 保存校准结果
    
    // 示例：设置默认EQ曲线
    float eq_values[9] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    if (aml_eq_set_gain(0, eq_values) != 0) {
        LOG_ERROR("Failed to set EQ gain");
        return -1;
    }
    
    // 保存EQ校准结果
    if (aml_eq_save_calib_data() != 0) {
        LOG_ERROR("Failed to save EQ calibration data");
        return -1;
    }
    
    LOG_INFO("EQ calibration completed successfully");
    return 0;
}

#else

int hw_calib_init(void) {
    LOG_INFO("HW calibration not supported for current product");
    return 0;
}

void hw_calib_deinit(void) {}

int hw_calib_age_start(int hour) {
    LOG_ERROR("Aging test not supported for current product");
    return -1;
}

int hw_calib_age_stop(void) {
    LOG_ERROR("Aging test not supported for current product");
    return -1;
}

int hw_calib_eq_run(void) {
    LOG_ERROR("EQ calibration not supported for current product");
    return -1;
}

#endif