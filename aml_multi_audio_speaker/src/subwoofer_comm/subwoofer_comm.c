#include "subwoofer_comm.h"
#include "logger.h"
#include "product_type.h"
#include "bt.h"
#include "audio_core.h"
#include "common_def.h"

#ifdef CONFIG_ENABLE_BT_MESH

// 静态全局变量
static SubwooferConfig_t g_sw_cfg = {0};
static SubwooferState_e g_sw_state = SUBWOOFER_STATE_DISCONNECTED;
static bool g_power_on = false;
static int g_current_gain = 50; // 默认增益50%

// Amlogic SDK相关头文件
#include <aml_bt.h>         // 晶晨蓝牙SDK
#include <aml_bt_mesh.h>    // 晶晨蓝牙MESH SDK
#include <aml_audio.h>      // 晶晨音频SDK
#include <aml_gpio.h>       // 晶晨GPIO SDK

/**
 * @brief 低音炮蓝牙配对回调函数
 */
static void bt_pairing_callback(int event, const char *device_name, const char *device_addr)
{
    switch (event) {
        case BT_PAIRING_START:
            LOG_INFO("Subwoofer pairing start: %s", device_name);
            g_sw_state = SUBWOOFER_STATE_CONNECTING;
            break;
        case BT_PAIRING_SUCCESS:
            LOG_INFO("Subwoofer pairing success: %s", device_name);
            g_sw_state = SUBWOOFER_STATE_PAIRED;
            break;
        case BT_PAIRING_FAILED:
            LOG_INFO("Subwoofer pairing failed: %s", device_name);
            g_sw_state = SUBWOOFER_STATE_DISCONNECTED;
            break;
        default:
            break;
    }
}

/**
 * @brief 低音炮蓝牙连接回调函数
 */
static void bt_connection_callback(int event, const char *device_name, const char *device_addr)
{
    switch (event) {
        case BT_CONNECTED:
            LOG_INFO("Subwoofer connected: %s", device_name);
            g_sw_state = SUBWOOFER_STATE_CONNECTED;
            break;
        case BT_DISCONNECTED:
            LOG_INFO("Subwoofer disconnected: %s", device_name);
            g_sw_state = SUBWOOFER_STATE_DISCONNECTED;
            break;
        default:
            break;
    }
}

/**
 * @brief 低音炮音频接收回调函数
 */
static void audio_receive_callback(uint8_t *data, unsigned int len, int sample_rate, int channels)
{
    SubwooferAudioData_t audio_data;
    audio_data.data = data;
    audio_data.len = len;
    audio_data.sample_rate = sample_rate;
    audio_data.channels = channels;
    audio_data.bit_depth = 16; // 默认16位
    
    subwoofer_comm_play_audio(&audio_data);
}

/**
 * @brief 低音炮初始化
 */
int subwoofer_comm_init(SubwooferConfig_t *cfg)
{
    // 初始化配置
    memset(&g_sw_cfg, 0, sizeof(SubwooferConfig_t));
    if (cfg != NULL) {
        memcpy(&g_sw_cfg, cfg, sizeof(SubwooferConfig_t));
        g_current_gain = cfg->bass_gain;
    } else {
        // 默认配置
        strncpy(g_sw_cfg.bt_name, "AML_Subwoofer", sizeof(g_sw_cfg.bt_name) - 1);
        g_sw_cfg.bass_gain = 50;
        g_sw_cfg.vol_sync_en = true;
        g_sw_cfg.auto_connect_en = true;
        g_current_gain = 50;
    }
    
    // 1. 初始化Amlogic蓝牙MESH
    aml_bt_mesh_init();
    aml_bt_mesh_set_device_name(g_sw_cfg.bt_name);
    aml_bt_mesh_set_pairing_callback(bt_pairing_callback);
    aml_bt_mesh_set_connection_callback(bt_connection_callback);
    
    // 2. 初始化Amlogic音频接收
    aml_audio_init();
    aml_audio_set_callback(audio_receive_callback);
    
    // 3. 初始化功放GPIO
    aml_gpio_init();
    aml_gpio_set_output("AMP_EN");
    aml_gpio_set_value("AMP_EN", 0); // 初始关闭功放
    
    // 4. 设置初始状态
    g_sw_state = SUBWOOFER_STATE_DISCONNECTED;
    g_power_on = false;
    
    LOG_INFO("Subwoofer comm module init success");
    LOG_INFO("  BT Name: %s", g_sw_cfg.bt_name);
    LOG_INFO("  Bass Gain: %d%%", g_sw_cfg.bass_gain);
    LOG_INFO("  Auto Connect: %s", g_sw_cfg.auto_connect_en ? "ON" : "OFF");
    
    return SUCCESS;
}

/**
 * @brief 低音炮反初始化
 */
int subwoofer_comm_deinit(void)
{
    // 关闭功放
    aml_gpio_set_value("AMP_EN", 0);
    
    // 反初始化音频
    aml_audio_deinit();
    
    // 反初始化蓝牙MESH
    aml_bt_mesh_deinit();
    
    // 反初始化GPIO
    aml_gpio_deinit();
    
    g_sw_state = SUBWOOFER_STATE_DISCONNECTED;
    g_power_on = false;
    
    LOG_INFO("Subwoofer comm module deinit success");
    
    return SUCCESS;
}

/**
 * @brief 获取低音炮连接状态
 */
SubwooferState_e subwoofer_comm_get_state(void)
{
    return g_sw_state;
}

/**
 * @brief 设置低音增益
 */
int subwoofer_comm_set_bass_gain(int gain)
{
    if (gain < 0 || gain > 100) {
        LOG_ERROR("Invalid bass gain: %d, range [0-100]", gain);
        return FAILURE;
    }
    
    g_current_gain = gain;
    g_sw_cfg.bass_gain = gain;
    
    // 使用Amlogic音频SDK设置低音增益
    aml_audio_set_bass_gain(gain);
    
    LOG_INFO("Set bass gain to %d%%", gain);
    
    return SUCCESS;
}

/**
 * @brief 获取当前低音增益
 */
int subwoofer_comm_get_bass_gain(void)
{
    return g_current_gain;
}

/**
 * @brief 发送音量同步指令
 */
int subwoofer_comm_sync_volume(int master_vol, bool is_mute)
{
    if (!g_sw_cfg.vol_sync_en) {
        return SUCCESS;
    }
    
    // 使用Amlogic音频SDK设置音量
    aml_audio_set_volume(master_vol);
    aml_audio_set_mute(is_mute);
    
    LOG_INFO("Sync volume: %d, Mute: %s", master_vol, is_mute ? "ON" : "OFF");
    
    return SUCCESS;
}

/**
 * @brief 发送开关指令
 */
int subwoofer_comm_set_power(bool power_on)
{
    g_power_on = power_on;
    
    if (power_on) {
        // 开启功放
        aml_gpio_set_value("AMP_EN", 1);
        LOG_INFO("Subwoofer power ON");
    } else {
        // 关闭功放
        aml_gpio_set_value("AMP_EN", 0);
        LOG_INFO("Subwoofer power OFF");
    }
    
    return SUCCESS;
}

/**
 * @brief 接收音频数据并发送到功放
 */
int subwoofer_comm_play_audio(SubwooferAudioData_t *audio_data)
{
    if (!audio_data || !audio_data->data || audio_data->len == 0) {
        LOG_ERROR("Invalid audio data");
        return FAILURE;
    }
    
    // 确保功放在开启状态
    if (!g_power_on) {
        return FAILURE;
    }
    
    // 使用Amlogic音频SDK播放PCM数据
    aml_audio_play_pcm(audio_data->data, audio_data->len, 
                       audio_data->sample_rate, audio_data->channels, 
                       audio_data->bit_depth);
    
    return SUCCESS;
}

/**
 * @brief 轮询处理低音炮事件
 */
void subwoofer_comm_event_poll(void)
{
    if (g_sw_cfg.auto_connect_en && g_sw_state == SUBWOOFER_STATE_DISCONNECTED) {
        // 自动连接尝试
        aml_bt_mesh_start_scan();
    }
    
    // 轮询蓝牙MESH事件
    aml_bt_mesh_event_poll();
    
    // 轮询音频事件
    aml_audio_event_poll();
}

#else

// 非蓝牙MESH模式下的空实现
int subwoofer_comm_init(SubwooferConfig_t *cfg) { return 0; }
int subwoofer_comm_deinit(void) { return 0; }
SubwooferState_e subwoofer_comm_get_state(void) { return SUBWOOFER_STATE_DISCONNECTED; }
int subwoofer_comm_set_bass_gain(int gain) { return 0; }
int subwoofer_comm_get_bass_gain(void) { return 0; }
int subwoofer_comm_sync_volume(int master_vol, bool is_mute) { return 0; }
int subwoofer_comm_set_power(bool power_on) { return 0; }
int subwoofer_comm_play_audio(SubwooferAudioData_t *audio_data) { return 0; }
/**
 * @brief 轮询处理低音炮事件
 */
void subwoofer_comm_event_poll(void) {
    LOG_DEBUG("Subwoofer comm event poll called (BT MESH not enabled)");
    // 虽然蓝牙MESH未启用，但保持函数接口一致
    // 可以添加简单的状态检查逻辑
    static bool init_warn_logged = false;
    if (!init_warn_logged) {
        LOG_INFO("Subwoofer BT MESH not enabled, event poll skipped");
        init_warn_logged = true;
    }
}

#endif