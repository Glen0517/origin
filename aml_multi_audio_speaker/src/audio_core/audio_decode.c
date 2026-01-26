#include "audio_core_priv.h"
#include "logger.h"
#include "product_type.h"

static AudioDecodeCfg_t g_decode_cfg = {0};

int audio_decode_init(void) {
    memset(&g_decode_cfg, 0, sizeof(AudioDecodeCfg_t));
    
    // 低端游戏音响：仅基础立体声解码
#ifdef CONFIG_ENABLE_GAME_SPEAKER
    if (CURRENT_PRODUCT_TYPE == PRODUCT_GAME_LOW_END) {
        g_decode_cfg.type = AUDIO_DECODE_PCM; // 仅支持PCM解码
        g_decode_cfg.dolby_en = 0;
        g_decode_cfg.dts_en = 0;
        LOG_INFO("Audio decode: PCM only (LOW END GAME SPEAKER)");
        LOG_INFO(" 极致裁剪: 仅保留核心解码");
    } else {
#else
    {
#endif
        g_decode_cfg.type = AUDIO_DECODE_MP3;
    #ifdef CONFIG_ENABLE_DOLBY_DTS
        g_decode_cfg.dolby_en = 1;
        g_decode_cfg.dts_en = 1;
        LOG_INFO("Audio decode: PCM+MP3+AAC (Dolby/DTS ENABLE)");
    #else
        LOG_INFO("Audio decode: PCM+MP3 (Dolby/DTS DISABLE)");
    #endif
    }
    
    g_decode_cfg.init_ok = 1;
    return 0;
}

void audio_decode_deinit(void) {
    if (g_decode_cfg.init_ok) {
        memset(&g_decode_cfg, 0, sizeof(AudioDecodeCfg_t));
    }
}

/**
 * @brief 基础立体声解码处理
 * @details 为低端游戏音响提供最基础的音频解码功能
 */
int audio_decode_basic_process(uint8_t *input_data, int input_len, uint8_t *output_data, int *output_len) {
    if (!g_decode_cfg.init_ok || !input_data || input_len <= 0 || !output_data || !output_len) {
        return -1;
    }
    
    // 对于低端游戏音响，直接复制PCM数据
#ifdef CONFIG_ENABLE_GAME_SPEAKER
    if (CURRENT_PRODUCT_TYPE == PRODUCT_GAME_LOW_END) {
        if (*output_len >= input_len) {
            memcpy(output_data, input_data, input_len);
            *output_len = input_len;
            return 0;
        } else {
            LOG_ERROR("Output buffer too small: %d < %d", *output_len, input_len);
            return -1;
        }
    }
#endif
    
    // 其他产品类型使用标准解码流程
    return -1;
}