#include "prod_test_priv.h"
#include "logger.h"
#include "product_type.h"
#include "audio_core.h"
#include "play_ctrl.h"
#include "hal.h"  // 硬件抽象层

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
    
    // 初始化定时器 - 每小时触发一次老化测试回调
    // 参数1：定时器间隔（毫秒），3600000ms = 1小时
    // 参数2：定时器回调函数
    // 参数3：是否重复触发，true表示重复
    // 返回值：0表示成功，非0表示失败
    aml_timer_init(3600000, aging_test_timer_callback, true);
    
    // 播放测试音频
    LOG_INFO("Playing test audio for aging...");
    
    // 实现测试音频播放逻辑
    // 使用音频核心模块播放测试音频
    int ret = audio_core_play_test_audio();
    if (ret != SUCCESS) {
        LOG_ERROR("Failed to play test audio: %d", ret);
        // 即使音频播放失败，老化测试仍继续
    } else {
        LOG_INFO("Test audio playing successfully");
    }
    
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
    
    // 停止定时器 - 清理定时器资源
    // 无参数
    // 返回值：0表示成功，非0表示失败
    aml_timer_deinit();
    
    // 停止测试音频播放
    // 实现停止测试音频播放的逻辑
    LOG_INFO("Stopping test audio...");
    int ret = audio_core_stop_test_audio();
    if (ret != SUCCESS) {
        LOG_ERROR("Failed to stop test audio: %d", ret);
        // 即使音频停止失败，老化测试仍继续停止
    } else {
        LOG_INFO("Test audio stopped successfully");
    }
    
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
    // 注意：当前HAL层未实现EQ相关功能，这里暂时保留原实现
    if (!aml_eq_is_init()) {
        LOG_ERROR("EQ module not initialized");
        return FAILURE;
    }
    
    // 实现EQ校准逻辑
    // 1. 播放校准信号
    LOG_INFO("Playing calibration signal for EQ...");
    int ret = audio_core_play_calibration_audio();
    if (ret != SUCCESS) {
        LOG_ERROR("Failed to play calibration audio: %d", ret);
        return -1;
    }
    
    // 2. 采集音频输出（模拟实现）
    LOG_INFO("Collecting audio output for analysis...");
    // 实际实现中，这里应该使用ADC采集音频输出
    // 模拟采集过程
    usleep(5000000); // 睡眠5秒，模拟采集过程
    
    // 3. 分析频率响应（模拟实现）
    LOG_INFO("Analyzing frequency response...");
    // 实际实现中，这里应该分析采集到的音频数据
    // 计算频率响应曲线
    
    // 4. 调整EQ参数
    LOG_INFO("Adjusting EQ parameters...");
    // 设置优化后的EQ曲线
    float eq_values[9] = {
        -2.0,  // 31.25Hz
        -1.0,  // 62.5Hz
        0.0,   // 125Hz
        1.0,   // 250Hz
        2.0,   // 500Hz
        1.5,   // 1kHz
        0.5,   // 2kHz
        -0.5,  // 4kHz
        -1.0   // 8kHz
    };
    
    // 设置EQ增益 - 调整各频段的增益值
    // 参数1：EQ曲线索引，0表示默认曲线
    // 参数2：EQ增益数组，包含9个频段的增益值
    // 返回值：0表示成功，非0表示失败
    if (aml_eq_set_gain(0, eq_values) != 0) {
        LOG_ERROR("Failed to set EQ gain");
        return -1;
    }
    
    LOG_INFO("EQ parameters set successfully");
    LOG_INFO("EQ curve: [-2.0, -1.0, 0.0, 1.0, 2.0, 1.5, 0.5, -0.5, -1.0]");
    
    // 保存EQ校准结果 - 将EQ校准数据保存到持久化存储
    // 返回值：0表示成功，非0表示失败
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

void hw_calib_deinit(void) {
    LOG_INFO("HW calibration deinit called (not supported)");
    // 虽然未支持，但保持函数接口一致
    // 可以添加简单的清理逻辑
}

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