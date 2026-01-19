#include "peripheral_priv.h"
#include "logger.h"
#include "product_type.h"
#include "common_def.h"
#include "comm_mcu.h"        // MCU通信接口

static bool g_prompt_init = false;

// 提示音ID定义
typedef enum {
    PROMPT_SOUND_POWER_ON = 1,
    PROMPT_SOUND_POWER_OFF,
    PROMPT_SOUND_BT_CONNECT,
    PROMPT_SOUND_BT_DISCONNECT,
    PROMPT_SOUND_VOLUME_UP,
    PROMPT_SOUND_VOLUME_DOWN,
    PROMPT_SOUND_SOURCE_SWITCH,
    PROMPT_SOUND_ERROR,
    PROMPT_SOUND_MAX
} PromptSoundId_e;

// 提示音命令定义（用于UART通信）
#define PROMPT_CMD_PLAY_SOUND    0x01

int prompt_sound_init(void)
{
    if (g_prompt_init) {
        LOG_INFO("Prompt sound already initialized");
        return 0;
    }
    
    g_prompt_init = false;
    
    // 根据产品类型初始化提示音
#if (CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END)
    // 高端产品：支持所有提示音
    LOG_INFO("Peripheral: Prompt sound init (full support)");
    
#elif (CURRENT_PRODUCT_TYPE == PRODUCT_MID_END)
    // 中端产品：支持部分提示音
    LOG_INFO("Peripheral: Prompt sound init (mid support)");
    
#else
    // 低端产品：仅支持基本提示音
    LOG_INFO("Peripheral: Prompt sound init (basic support) [ALL PRODUCT]");
#endif
    
    g_prompt_init = true;
    
    LOG_INFO("Prompt sound init success");
    
    return 0;
}

void prompt_sound_deinit(void)
{
    if (!g_prompt_init) {
        return;
    }
    
    g_prompt_init = false;
    
    LOG_INFO("Prompt sound deinitialized");
}

/**
 * @brief 播放提示音
 */
void prompt_sound_play(int sound_id)
{
    if (!g_prompt_init || sound_id <= 0 || sound_id >= PROMPT_SOUND_MAX) {
        LOG_ERROR("Invalid prompt sound ID or not initialized");
        return;
    }
    
    // 构建提示音播放命令数据
    uint8_t data[2] = {0};
    data[0] = PROMPT_CMD_PLAY_SOUND;
    data[1] = (uint8_t)sound_id;
    
    // 通过UART发送命令给MCU
    // 注意：这里需要根据实际的UART协议扩展来实现
    // 目前暂未实现具体的命令发送，需要在MCU端添加相应的处理逻辑
    LOG_INFO("Playing prompt sound: %d", sound_id);
}
