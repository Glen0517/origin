#include "sound_effects_priv.h"
#include "logger.h"
#include "product_type.h"

#include <aml_dolby.h>       // 晶晨Dolby SDK
#include <aml_dts.h>         // 晶晨DTS SDK
#include <aml_dolby_atmos.h> // 晶晨Dolby Atmos SDK
#include <aml_dts_x.h>       // 晶晨DTS:X SDK
#include <aml_3d_audio.h>    // 晶晨3D Audio SDK

#ifdef CONFIG_ENABLE_DOLBY_DTS

static bool g_dolby_enabled = false;
static bool g_dts_enabled = false;
static bool g_dolby_dts_init = false;

// 高端游戏音响功能支持
#ifdef CONFIG_ENABLE_DOLBY_ATMOS
static bool g_dolby_atmos_enabled = false;
#endif

#ifdef CONFIG_ENABLE_DTS_X
static bool g_dts_x_enabled = false;
#endif

#ifdef CONFIG_ENABLE_3D_AUDIO
static bool g_3d_audio_enabled = false;
#endif

#ifdef CONFIG_ENABLE_HRTF
static bool g_hrtf_enabled = false;
#endif

/**
 * @brief Dolby状态回调函数
 */
static void dolby_status_callback(int status) {
    LOG_INFO("Dolby status: %d", status);
}

/**
 * @brief DTS状态回调函数
 */
static void dts_status_callback(int status) {
    LOG_INFO("DTS status: %d", status);
}

int dolby_dts_init(void)
{
    if (g_dolby_dts_init) {
        LOG_INFO("Dolby/DTS already initialized");
        return 0;
    }
    
    // 初始化Amlogic Dolby SDK
    if (aml_dolby_init() != 0) {
        LOG_ERROR("Dolby init failed");
    } else {
        aml_dolby_set_status_callback(dolby_status_callback);
        g_dolby_enabled = true;
        LOG_INFO("Dolby module init success");
    }
    
    // 初始化Amlogic DTS SDK
    if (aml_dts_init() != 0) {
        LOG_ERROR("DTS init failed");
    } else {
        aml_dts_set_status_callback(dts_status_callback);
        g_dts_enabled = true;
        LOG_INFO("DTS module init success");
    }
    
    // 高端游戏音响功能初始化
    #ifdef CONFIG_ENABLE_DOLBY_ATMOS
    if (aml_dolby_atmos_init() != 0) {
        LOG_ERROR("Dolby Atmos init failed");
    } else {
        g_dolby_atmos_enabled = true;
        LOG_INFO("Dolby Atmos module init success");
    }
    #endif
    
    #ifdef CONFIG_ENABLE_DTS_X
    if (aml_dts_x_init() != 0) {
        LOG_ERROR("DTS:X init failed");
    } else {
        g_dts_x_enabled = true;
        LOG_INFO("DTS:X module init success");
    }
    #endif
    
    #ifdef CONFIG_ENABLE_3D_AUDIO
    if (aml_3d_audio_init() != 0) {
        LOG_ERROR("3D Audio init failed");
    } else {
        g_3d_audio_enabled = true;
        LOG_INFO("3D Audio module init success");
    }
    #endif
    
    #ifdef CONFIG_ENABLE_HRTF
    if (aml_hrtf_init() != 0) {
        LOG_ERROR("HRTF init failed");
    } else {
        g_hrtf_enabled = true;
        LOG_INFO("HRTF module init success");
    }
    #endif
    
    g_dolby_dts_init = true;
    
    LOG_INFO("Sound: Dolby/DTS decode enhance init success (HIGH END)");
    LOG_INFO("  Dolby enabled: %s", g_dolby_enabled ? "YES" : "NO");
    LOG_INFO("  DTS enabled: %s", g_dts_enabled ? "YES" : "NO");
    
    // 高端游戏音响功能状态日志
    #ifdef CONFIG_ENABLE_DOLBY_ATMOS
    LOG_INFO("  Dolby Atmos enabled: %s", g_dolby_atmos_enabled ? "YES" : "NO");
    #endif
    
    #ifdef CONFIG_ENABLE_DTS_X
    LOG_INFO("  DTS:X enabled: %s", g_dts_x_enabled ? "YES" : "NO");
    #endif
    
    #ifdef CONFIG_ENABLE_3D_AUDIO
    LOG_INFO("  3D Audio enabled: %s", g_3d_audio_enabled ? "YES" : "NO");
    #endif
    
    #ifdef CONFIG_ENABLE_HRTF
    LOG_INFO("  HRTF enabled: %s", g_hrtf_enabled ? "YES" : "NO");
    #endif
    
    return 0;
}

void dolby_dts_deinit(void)
{
    if (g_dolby_dts_init) {
        // 反初始化Amlogic DTS SDK
        if (g_dts_enabled) {
            aml_dts_deinit();
            g_dts_enabled = false;
        }
        
        // 反初始化Amlogic Dolby SDK
        if (g_dolby_enabled) {
            aml_dolby_deinit();
            g_dolby_enabled = false;
        }
        
        // 高端游戏音响功能反初始化
        #ifdef CONFIG_ENABLE_DOLBY_ATMOS
        if (g_dolby_atmos_enabled) {
            aml_dolby_atmos_deinit();
            g_dolby_atmos_enabled = false;
            LOG_INFO("Dolby Atmos module deinit success");
        }
        #endif
        
        #ifdef CONFIG_ENABLE_DTS_X
        if (g_dts_x_enabled) {
            aml_dts_x_deinit();
            g_dts_x_enabled = false;
            LOG_INFO("DTS:X module deinit success");
        }
        #endif
        
        #ifdef CONFIG_ENABLE_3D_AUDIO
        if (g_3d_audio_enabled) {
            aml_3d_audio_deinit();
            g_3d_audio_enabled = false;
            LOG_INFO("3D Audio module deinit success");
        }
        #endif
        
        #ifdef CONFIG_ENABLE_HRTF
        if (g_hrtf_enabled) {
            aml_hrtf_deinit();
            g_hrtf_enabled = false;
            LOG_INFO("HRTF module deinit success");
        }
        #endif
        
        g_dolby_dts_init = false;
        
        LOG_INFO("Dolby/DTS module deinit success");
    }
}

/**
 * @brief 设置Dolby状态
 */
int dolby_dts_set_dolby(bool en)
{
    if (!g_dolby_dts_init || !g_dolby_enabled) {
        return -1;
    }
    
    if (aml_dolby_enable(en) != 0) {
        LOG_ERROR("Set Dolby status failed");
        return -1;
    }
    
    LOG_INFO("Dolby %s", en ? "enabled" : "disabled");
    
    return 0;
}

/**
 * @brief 设置DTS状态
 */
int dolby_dts_set_dts(bool en)
{
    if (!g_dolby_dts_init || !g_dts_enabled) {
        return -1;
    }
    
    if (aml_dts_enable(en) != 0) {
        LOG_ERROR("Set DTS status failed");
        return -1;
    }
    
    LOG_INFO("DTS %s", en ? "enabled" : "disabled");
    
    return 0;
}

/**
 * @brief 处理Dolby/DTS音频数据
 */
int dolby_dts_process(uint8_t *input_data, int input_len, uint8_t *output_data, int *output_len)
{
    if (!g_dolby_dts_init || !input_data || input_len <= 0 || !output_data || !output_len) {
        return -1;
    }
    
    int processed_len = 0;
    
    // 高端游戏音响功能处理
    #ifdef CONFIG_ENABLE_DOLBY_ATMOS
    if (g_dolby_atmos_enabled) {
        processed_len = aml_dolby_atmos_process(input_data, input_len, output_data, *output_len);
        if (processed_len > 0) {
            *output_len = processed_len;
            return 0;
        }
    }
    #endif
    
    #ifdef CONFIG_ENABLE_DTS_X
    if (g_dts_x_enabled) {
        processed_len = aml_dts_x_process(input_data, input_len, output_data, *output_len);
        if (processed_len > 0) {
            *output_len = processed_len;
            return 0;
        }
    }
    #endif
    
    #ifdef CONFIG_ENABLE_3D_AUDIO
    if (g_3d_audio_enabled) {
        processed_len = aml_3d_audio_process(input_data, input_len, output_data, *output_len);
        if (processed_len > 0) {
            *output_len = processed_len;
            return 0;
        }
    }
    #endif
    
    #ifdef CONFIG_ENABLE_HRTF
    if (g_hrtf_enabled) {
        processed_len = aml_hrtf_process(input_data, input_len, output_data, *output_len);
        if (processed_len > 0) {
            *output_len = processed_len;
            return 0;
        }
    }
    #endif
    
    // 基础Dolby/DTS处理
    if (g_dolby_enabled) {
        processed_len = aml_dolby_process(input_data, input_len, output_data, *output_len);
    } else if (g_dts_enabled) {
        processed_len = aml_dts_process(input_data, input_len, output_data, *output_len);
    } else {
        // 如果都没有启用，直接复制数据
        if (*output_len >= input_len) {
            memcpy(output_data, input_data, input_len);
            processed_len = input_len;
        }
    }
    
    *output_len = processed_len;
    
    return processed_len > 0 ? 0 : -1;
}

#else
int dolby_dts_init(void) { return 0; }
void dolby_dts_deinit(void) {
    LOG_INFO("Dolby/DTS deinit called");
    // 清理Dolby/DTS相关资源
    // 虽然是空实现，但保持函数接口一致
}
int dolby_dts_set_dolby(bool en) { return 0; }
int dolby_dts_set_dts(bool en) { return 0; }
int dolby_dts_process(uint8_t *input_data, int input_len, uint8_t *output_data, int *output_len) { return 0; }
#endif