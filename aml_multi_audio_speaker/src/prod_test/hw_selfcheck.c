#include "prod_test_priv.h"
#include "logger.h"
#include <aml_audio.h>        // 晶晨音频SDK
#include <aml_gpio.h>         // 晶晨GPIO SDK
#include <aml_pwm.h>          // 晶晨PWM SDK

static bool g_hw_check_init = false;

int hw_selfcheck_init(void)
{
    g_hw_check_init = true;
    LOG_INFO("Prod test: HW selfcheck init success [ALL PRODUCT NO MACRO]");
    return 0;
}

void hw_selfcheck_deinit(void)
{
    g_hw_check_init = false;
}

/**
 * @brief 执行硬件自检
 */
int hw_selfcheck_run(void)
{
    if (!g_hw_check_init) {
        LOG_ERROR("HW selfcheck not initialized");
        return -1;
    }
    
    LOG_INFO("Running hardware self-check...");
    
    int result = 0;
    
    // 检查音频设备
    LOG_INFO("Checking audio devices...");
    // 检查音频模块是否初始化 - 检查音频SDK是否已初始化
    // 返回值：0表示已初始化，非0表示未初始化
    if (aml_audio_is_init() != 0) {
        LOG_ERROR("Audio device not initialized");
        result = -1;
    } else {
        LOG_INFO("  ✓ Audio device initialized");
        
        // 检查音频输出通道 - 获取当前音频通道数
        // 返回值：当前音频通道数
        int channels = aml_audio_get_channels();
        LOG_INFO("  ✓ Audio channels: %d", channels);
        
        // 检查音频采样率 - 获取当前音频采样率
        // 返回值：当前音频采样率（Hz）
        int sample_rate = aml_audio_get_sample_rate();
        LOG_INFO("  ✓ Sample rate: %d Hz", sample_rate);
    }
    
    // 检查GPIO
    LOG_INFO("Checking GPIO...");
    // 检查GPIO模块是否初始化 - 检查GPIO SDK是否已初始化
    // 返回值：0表示已初始化，非0表示未初始化
    if (aml_gpio_is_init() != 0) {
        LOG_ERROR("GPIO not initialized");
        result = -1;
    } else {
        LOG_INFO("  ✓ GPIO initialized");
    }
    
    // 检查PWM
    LOG_INFO("Checking PWM...");
    // 检查PWM模块是否初始化 - 检查PWM SDK是否已初始化
    // 返回值：0表示已初始化，非0表示未初始化
    if (aml_pwm_is_init() != 0) {
        LOG_ERROR("PWM not initialized");
        result = -1;
    } else {
        LOG_INFO("  ✓ PWM initialized");
    }
    
    // 实现检查其他硬件模块
    // 检查I2C设备
    LOG_INFO("Checking I2C devices...");
    #ifdef CONFIG_ENABLE_I2C
    if (aml_i2c_is_init() != 0) {
        LOG_ERROR("I2C not initialized");
        result = -1;
    } else {
        LOG_INFO("  ✓ I2C initialized");
    }
    #else
    LOG_INFO("  I2C not enabled in config");
    #endif
    
    // 检查SPI设备
    LOG_INFO("Checking SPI devices...");
    #ifdef CONFIG_ENABLE_SPI
    if (aml_spi_is_init() != 0) {
        LOG_ERROR("SPI not initialized");
        result = -1;
    } else {
        LOG_INFO("  ✓ SPI initialized");
    }
    #else
    LOG_INFO("  SPI not enabled in config");
    #endif
    
    // 检查ADC/DAC
    LOG_INFO("Checking ADC/DAC...");
    #ifdef CONFIG_ENABLE_ADC_DAC
    if (aml_adc_is_init() != 0) {
        LOG_ERROR("ADC not initialized");
        result = -1;
    } else {
        LOG_INFO("  ✓ ADC initialized");
    }
    if (aml_dac_is_init() != 0) {
        LOG_ERROR("DAC not initialized");
        result = -1;
    } else {
        LOG_INFO("  ✓ DAC initialized");
    }
    #else
    LOG_INFO("  ADC/DAC not enabled in config");
    #endif
    
    // 检查网络接口
    LOG_INFO("Checking network interfaces...");
    #ifdef CONFIG_ENABLE_NETWORK
    if (aml_network_is_init() != 0) {
        LOG_ERROR("Network not initialized");
        result = -1;
    } else {
        LOG_INFO("  ✓ Network initialized");
    }
    #else
    LOG_INFO("  Network not enabled in config");
    #endif
    
    // 检查存储设备
    LOG_INFO("Checking storage devices...");
    #ifdef CONFIG_ENABLE_STORAGE
    if (aml_storage_is_init() != 0) {
        LOG_ERROR("Storage not initialized");
        result = -1;
    } else {
        LOG_INFO("  ✓ Storage initialized");
    }
    #else
    LOG_INFO("  Storage not enabled in config");
    #endif
    
    LOG_INFO("Hardware self-check completed");
    
    return result;
}