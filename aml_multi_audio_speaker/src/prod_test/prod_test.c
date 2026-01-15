#include "prod_test.h"
#include "prod_test_priv.h"
#include "logger.h"
#include "audio_core.h"      // 音频核心模块
#include "play_ctrl.h"       // 播放控制模块
#include "audio_source.h"    // 音频源模块
#include "sound_effects.h"   // 音效模块

static ProdTestCfg_t g_test_cfg = {0};
static bool g_aging_test_running = false;
static int g_aging_test_hours = 0;

int prod_test_init(void)
{
    memset(&g_test_cfg, 0, sizeof(ProdTestCfg_t));
    // 必加载：基础硬件自检 所有产品通用
    hw_selfcheck_init();
    // 宏控加载：硬件校准/老化测试 仅高端支持
    hw_calib_init();
    g_test_cfg.init_ok = 1;
    LOG_INFO("Product test module init success (CALIB: %d)", CONFIG_ENABLE_HW_CALIB);
    return 0;
}

int prod_test_deinit(void)
{
    if (g_test_cfg.init_ok)
    {
        // 停止老化测试
        prod_test_age_stop();
        
        hw_calib_deinit();
        hw_selfcheck_deinit();
        g_test_cfg.init_ok = 0;
        LOG_INFO("Product test module deinit success");
    }
    return 0;
}

/**
 * @brief 硬件自检
 */
int prod_test_hw_check(void)
{
    if (!g_test_cfg.init_ok) {
        LOG_ERROR("Product test module not initialized");
        return -1;
    }
    
    LOG_INFO("=== Product Hardware Self-Check ===");
    
    // 检查音频核心模块
    if (audio_core_is_init()) {
        LOG_INFO("✓ Audio core initialized");
    } else {
        LOG_ERROR("✗ Audio core not initialized");
        return -1;
    }
    
    // 检查播放控制模块
    if (play_ctrl_is_init()) {
        LOG_INFO("✓ Play control initialized");
    } else {
        LOG_ERROR("✗ Play control not initialized");
        return -1;
    }
    
    // 检查音频源模块
    if (audio_source_is_init()) {
        LOG_INFO("✓ Audio source initialized");
    } else {
        LOG_ERROR("✗ Audio source not initialized");
        return -1;
    }
    
    // 检查音效模块
    if (sound_effects_is_init()) {
        LOG_INFO("✓ Sound effects initialized");
    } else {
        LOG_ERROR("✗ Sound effects not initialized");
        return -1;
    }
    
    // 运行硬件自检
    int result = hw_selfcheck_run();
    if (result != 0) {
        LOG_ERROR("Hardware self-check failed");
        return -1;
    }
    
    LOG_INFO("=== Hardware Self-Check PASSED ===");
    return 0;
}

/**
 * @brief 开始老化测试
 */
int prod_test_age_start(int hour)
{
    if (!g_test_cfg.init_ok) {
        LOG_ERROR("Product test module not initialized");
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
    
    g_aging_test_hours = hour;
    g_aging_test_running = true;
    
    // 启动老化测试
    int result = hw_calib_age_start(hour);
    if (result != 0) {
        LOG_ERROR("Aging test start failed");
        g_aging_test_running = false;
        return -1;
    }
    
    LOG_INFO("Aging test started for %d hours", hour);
    return 0;
}

/**
 * @brief 停止老化测试
 */
int prod_test_age_stop(void)
{
    if (!g_test_cfg.init_ok) {
        LOG_ERROR("Product test module not initialized");
        return -1;
    }
    
    if (!g_aging_test_running) {
        LOG_INFO("Aging test not running");
        return 0;
    }
    
    // 停止老化测试
    int result = hw_calib_age_stop();
    if (result != 0) {
        LOG_ERROR("Aging test stop failed");
        return -1;
    }
    
    g_aging_test_running = false;
    g_aging_test_hours = 0;
    
    LOG_INFO("Aging test stopped");
    return 0;
}

/**
 * @brief 校准EQ
 */
int prod_test_calib_eq(void)
{
    if (!g_test_cfg.init_ok) {
        LOG_ERROR("Product test module not initialized");
        return -1;
    }
    
    // 检查是否支持EQ校准
    if (!CONFIG_ENABLE_HW_CALIB) {
        LOG_ERROR("EQ calibration not supported for current product");
        return -1;
    }
    
    LOG_INFO("Starting EQ calibration...");
    
    // 运行EQ校准
    int result = hw_calib_eq_run();
    if (result != 0) {
        LOG_ERROR("EQ calibration failed");
        return -1;
    }
    
    LOG_INFO("EQ calibration completed successfully");
    return 0;
}

/**
 * @brief 检查老化测试状态
 */
bool prod_test_age_is_running(void)
{
    return g_aging_test_running;
}

/**
 * @brief 获取剩余老化测试时间（小时）
 */
int prod_test_age_get_remaining(void)
{
    if (!g_aging_test_running) {
        return 0;
    }
    
    // TODO: 实现获取剩余老化测试时间的逻辑
    return g_aging_test_hours;
}