#include "comm_mcu_priv.h"
#include "logger.h"
#include "product_type.h"

#include <aml_gpio.h>        // 晶晨GPIO SDK
#include <aml_pwm.h>         // 晶晨PWM SDK

// 功放控制仅中/高端支持
#if (CURRENT_PRODUCT_TYPE == PRODUCT_LOW_END) || (CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER)

int amp_ctrl_init(void) {
    LOG_INFO("AMP control not supported for current product type");
    return 0;
}

void amp_ctrl_deinit(void) {}

#else

static bool g_amp_init = false;
static bool g_amp_enabled = false;
static int g_amp_volume = 80;    // 默认音量80%

// 功放控制GPIO定义（示例值，实际需根据硬件调整）
#define AMP_POWER_PIN      12     // 功放电源控制GPIO
#define AMP_MUTE_PIN       13     // 功放静音控制GPIO
#define AMP_VOLUME_PWM     0      // 功放音量控制PWM

/**
 * @brief 控制功放电源
 */
static int amp_ctrl_power(bool on) {
    if (!g_amp_init) {
        return -1;
    }
    
    int result = aml_gpio_set_value(AMP_POWER_PIN, on);
    
    if (result == 0) {
        g_amp_enabled = on;
        LOG_INFO("AMP power %s", on ? "ON" : "OFF");
    } else {
        LOG_ERROR("AMP power control failed");
    }
    
    return result;
}

/**
 * @brief 控制功放静音
 */
static int amp_ctrl_mute(bool mute) {
    if (!g_amp_init) {
        return -1;
    }
    
    int result = aml_gpio_set_value(AMP_MUTE_PIN, mute);
    
    if (result == 0) {
        LOG_INFO("AMP mute %s", mute ? "ON" : "OFF");
    } else {
        LOG_ERROR("AMP mute control failed");
    }
    
    return result;
}

/**
 * @brief 设置功放音量
 */
static int amp_ctrl_set_vol(int volume) {
    if (!g_amp_init || volume < 0 || volume > 100) {
        return -1;
    }
    
    // 音量范围转换：0-100 转换为 PWM 占空比（0-255）
    int pwm_value = (volume * 255) / 100;
    
    int result = aml_pwm_set_duty(AMP_VOLUME_PWM, pwm_value);
    
    if (result == 0) {
        g_amp_volume = volume;
        LOG_INFO("AMP volume set to %d%%", volume);
    } else {
        LOG_ERROR("AMP volume control failed");
    }
    
    return result;
}

int amp_ctrl_init(void) {
    if (g_amp_init) {
        LOG_INFO("AMP control already initialized");
        return 0;
    }
    
    // 初始化Amlogic GPIO SDK
    if (aml_gpio_init() != 0) {
        LOG_ERROR("AMP control init failed: GPIO SDK init error");
        return -1;
    }
    
    // 初始化Amlogic PWM SDK
    if (aml_pwm_init() != 0) {
        LOG_ERROR("AMP control init failed: PWM SDK init error");
        aml_gpio_deinit();
        return -1;
    }
    
    // 配置功放电源GPIO
    aml_gpio_set_direction(AMP_POWER_PIN, GPIO_DIR_OUTPUT);
    aml_gpio_set_value(AMP_POWER_PIN, 0);  // 默认关闭
    
    // 配置功放静音GPIO
    aml_gpio_set_direction(AMP_MUTE_PIN, GPIO_DIR_OUTPUT);
    aml_gpio_set_value(AMP_MUTE_PIN, 1);  // 默认静音
    
    // 配置功放音量PWM
    aml_pwm_set_frequency(AMP_VOLUME_PWM, 100000);  // 100kHz
    aml_pwm_set_duty(AMP_VOLUME_PWM, 0);  // 默认音量0
    aml_pwm_enable(AMP_VOLUME_PWM, true);
    
    g_amp_init = true;
    g_amp_enabled = false;
    g_amp_volume = 0;
    
    LOG_INFO("AMP control init success");
    LOG_INFO("  Power pin: GPIO%d", AMP_POWER_PIN);
    LOG_INFO("  Mute pin: GPIO%d", AMP_MUTE_PIN);
    LOG_INFO("  Volume PWM: PWM%d", AMP_VOLUME_PWM);
    
    return 0;
}

void amp_ctrl_deinit(void) {
    if (g_amp_init) {
        // 关闭功放
        amp_ctrl_power(false);
        
        // 禁用PWM
        aml_pwm_enable(AMP_VOLUME_PWM, false);
        
        // 反初始化PWM SDK
        aml_pwm_deinit();
        
        // 反初始化GPIO SDK
        aml_gpio_deinit();
        
        g_amp_init = false;
        g_amp_enabled = false;
        g_amp_volume = 0;
        
        LOG_INFO("AMP control deinit success");
    }
}

/**
 * @brief 开启功放
 */
int amp_ctrl_turn_on(void) {
    if (!g_amp_init) {
        LOG_ERROR("AMP control: not initialized");
        return -1;
    }
    
    // 开启功放电源
    if (amp_ctrl_power(true) != 0) {
        return -1;
    }
    
    // 设置音量
    if (amp_ctrl_set_vol(g_amp_volume) != 0) {
        return -1;
    }
    
    // 取消静音
    if (amp_ctrl_mute(false) != 0) {
        return -1;
    }
    
    return 0;
}

/**
 * @brief 关闭功放
 */
int amp_ctrl_turn_off(void) {
    if (!g_amp_init) {
        LOG_ERROR("AMP control: not initialized");
        return -1;
    }
    
    // 静音
    amp_ctrl_mute(true);
    
    // 关闭电源
    return amp_ctrl_power(false);
}

/**
 * @brief 设置功放音量
 */
int amp_ctrl_set_volume(int volume) {
    if (!g_amp_init) {
        LOG_ERROR("AMP control: not initialized");
        return -1;
    }
    
    return amp_ctrl_set_vol(volume);
}

/**
 * @brief 获取当前功放音量
 */
int amp_ctrl_get_volume(void) {
    return g_amp_init ? g_amp_volume : 0;
}

/**
 * @brief 检查功放是否开启
 */
bool amp_ctrl_is_enabled(void) {
    return g_amp_init && g_amp_enabled;
}

#endif