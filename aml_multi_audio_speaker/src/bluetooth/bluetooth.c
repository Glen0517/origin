#include "bt.h"
#include "bluetooth_priv.h"
#include "logger.h"
#include "event.h"
#include "hal.h"  // 硬件抽象层

static BtCfg_t g_bt_cfg = {0};
static char g_connected_dev_addr[18] = {0};  // 蓝牙设备地址（格式：XX:XX:XX:XX:XX:XX）
static char g_connected_dev_name[32] = {0};  // 蓝牙设备名称
static int g_connected_dev_type = 0;  // 蓝牙设备类型（0：未知，1：手机，2：电脑，3：其他）
static int g_disconnect_reason = 0;  // 断连原因（0：未知，1：正常断开，2：信号丢失，3：设备电量低，4：其他）
static int g_reconnect_attempts = 0;  // 重连尝试次数
static int g_max_reconnect_attempts = 3;  // 最大重连尝试次数
static int g_reconnect_interval = 5;  // 重连间隔（秒）

int bluetooth_init(BluetoothConfig_t *cfg)
{
    LOG_INFO("Initializing Bluetooth module...");
    
    memset(&g_bt_cfg, 0, sizeof(BtCfg_t));
    
    // 初始化配置参数
    if (cfg) {
        LOG_DEBUG("Using provided Bluetooth configuration");
        strncpy(g_bt_cfg.bt_name, cfg->bt_name, sizeof(g_bt_cfg.bt_name) - 1);
        strncpy(g_bt_cfg.bt_pin, cfg->bt_pin, sizeof(g_bt_cfg.bt_pin) - 1);
        g_bt_cfg.bt_auto_connect = cfg->bt_auto_connect;
        g_bt_cfg.mesh_en = cfg->bt_mesh_en;
        LOG_INFO("Bluetooth device name: %s", g_bt_cfg.bt_name);
        LOG_INFO("Bluetooth auto-connect: %d", g_bt_cfg.bt_auto_connect);
        LOG_INFO("Bluetooth MESH enabled: %d", g_bt_cfg.mesh_en);
    } else {
        LOG_DEBUG("Using default Bluetooth configuration");
        // 默认配置
        strncpy(g_bt_cfg.bt_name, "AML Audio Speaker", sizeof(g_bt_cfg.bt_name) - 1);
        strncpy(g_bt_cfg.bt_pin, "0000", sizeof(g_bt_cfg.bt_pin) - 1);
        g_bt_cfg.bt_auto_connect = true;
        g_bt_cfg.mesh_en = false;
        LOG_INFO("Default Bluetooth device name: %s", g_bt_cfg.bt_name);
        LOG_INFO("Default Bluetooth auto-connect: %d", g_bt_cfg.bt_auto_connect);
    }
    
    // 必加载：所有产品都有蓝牙A2DP基础功能
    LOG_INFO("Initializing HAL Bluetooth...");
    if (hal_bt_init() != 0) {
        LOG_ERROR("HAL bluetooth init failed");
        return FAILURE;
    }
    LOG_INFO("HAL Bluetooth init success");
    
    // 初始化蓝牙设备名称
    LOG_INFO("Setting Bluetooth device name to: %s", g_bt_cfg.bt_name);
    if (hal_bt_set_device_name(g_bt_cfg.bt_name) != 0) {
        LOG_ERROR("Failed to set Bluetooth device name");
        LOG_WARN("Continuing with default device name");
    } else {
        LOG_INFO("Bluetooth device name set successfully");
    }
    
    // 初始化蓝牙配对码
    LOG_INFO("Setting Bluetooth pin code...");
    if (hal_bt_set_pin_code(g_bt_cfg.bt_pin) != 0) {
        LOG_ERROR("Failed to set Bluetooth pin code");
        LOG_WARN("Continuing with default pin code");
    } else {
        LOG_INFO("Bluetooth pin code set successfully");
    }
    
    // 宏控加载：仅高端+低音炮支持MESH组网
#ifdef CONFIG_ENABLE_BT_MESH
    if (g_bt_cfg.mesh_en) {
        if (bt_mesh_init() != 0) {
            LOG_ERROR("Bluetooth MESH init failed");
            g_bt_cfg.mesh_en = false;
        } else {
            LOG_INFO("Bluetooth MESH init success");
        }
    }
#endif
    
    // 初始化A2DP功能
    if (bt_a2dp_init() != 0) {
        LOG_ERROR("Bluetooth A2DP init failed");
        // 继续执行，记录错误
    }
    
    g_bt_cfg.init_ok = 1;
    g_bt_cfg.bt_enable = true;
    g_bt_cfg.bt_connected = false;
    g_bt_cfg.bt_media_playing = false;
    g_bt_cfg.bt_media_enable = true;
    
    LOG_INFO("Bluetooth module init success (MESH: %d, Auto-connect: %d)", 
             g_bt_cfg.mesh_en, g_bt_cfg.bt_auto_connect);
    
    return SUCCESS;
}

int bluetooth_deinit(void)
{
    if (g_bt_cfg.init_ok)
    {
        // 反初始化A2DP功能
        bt_a2dp_deinit();
        
#ifdef CONFIG_ENABLE_BT_MESH
        if (g_bt_cfg.mesh_en) {
            bt_mesh_deinit();
        }
#endif
        
        if (hal_bt_deinit() != 0) {
            LOG_ERROR("HAL bluetooth deinit failed");
            return FAILURE;
        }
        
        g_bt_cfg.init_ok = 0;
        g_bt_cfg.bt_enable = false;
        g_bt_cfg.bt_connected = false;
        g_bt_cfg.bt_media_playing = false;
        g_bt_cfg.bt_media_enable = false;
        
        LOG_INFO("Bluetooth module deinit success");
    }
    
    return SUCCESS;
}

int bluetooth_start_pair(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    LOG_INFO("Starting Bluetooth pairing...");
    
    // 开启蓝牙可发现模式
    if (hal_bt_set_discoverable(true) != 0) {
        LOG_ERROR("Failed to set Bluetooth discoverable");
        return FAILURE;
    }
    
    // 开启蓝牙可配对模式
    if (hal_bt_set_pairable(true) != 0) {
        LOG_ERROR("Failed to set Bluetooth pairable");
        return FAILURE;
    }
    
    // 设置可发现和可配对的超时时间（30秒）
    if (hal_bt_set_pairable_timeout(30) != 0) {
        LOG_ERROR("Failed to set Bluetooth pairable timeout");
        // 继续执行，使用默认超时
    }
    
    // 更新LED状态
    led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_BLINK);
    
    // 更新LCD显示
    char lcd_msg[32] = {0};
    snprintf(lcd_msg, sizeof(lcd_msg), "BT: Pairing Mode");
    lcd_display_text(0, 0, lcd_msg);
    
    LOG_INFO("Bluetooth pairing started");
    return SUCCESS;
}

int bluetooth_stop_pair(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    LOG_INFO("Stopping Bluetooth pairing...");
    
    // 关闭蓝牙可发现模式
    if (hal_bt_set_discoverable(false) != 0) {
        LOG_ERROR("Failed to disable Bluetooth discoverable");
        // 继续执行
    }
    
    // 关闭蓝牙可配对模式
    if (hal_bt_set_pairable(false) != 0) {
        LOG_ERROR("Failed to disable Bluetooth pairable");
        // 继续执行
    }
    
    // 更新LED状态
    if (g_bt_cfg.bt_connected) {
        led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_ON);
    } else {
        led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_OFF);
    }
    
    // 更新LCD显示
    char lcd_msg[32] = {0};
    if (g_bt_cfg.bt_connected) {
        snprintf(lcd_msg, sizeof(lcd_msg), "BT: Connected");
    } else {
        snprintf(lcd_msg, sizeof(lcd_msg), "BT: Ready");
    }
    lcd_display_text(0, 0, lcd_msg);
    
    LOG_INFO("Bluetooth pairing stopped");
    return SUCCESS;
}

bool bluetooth_get_connect_state(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return false;
    }
    
    return g_bt_cfg.bt_connected;
}

int bluetooth_get_dev_name(char *name, int len)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (!name || len <= 0) {
        LOG_ERROR("Invalid parameters");
        return FAILURE;
    }
    
    // 获取当前连接的蓝牙设备名称
    if (hal_bt_get_connected_dev_name(name, len) != 0) {
        LOG_ERROR("Failed to get connected device name");
        return FAILURE;
    }
    
    LOG_INFO("Connected Bluetooth device: %s", name);
    return SUCCESS;
}

int bluetooth_get_dev_addr(char *addr, int len)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (!addr || len <= 0) {
        LOG_ERROR("Invalid parameters");
        return FAILURE;
    }
    
    // 获取当前连接的蓝牙设备地址
    if (hal_bt_get_connected_dev_addr(addr, len) != 0) {
        LOG_ERROR("Failed to get connected device address");
        return FAILURE;
    }
    
    LOG_INFO("Connected Bluetooth device address: %s", addr);
    return SUCCESS;
}

int bluetooth_get_dev_type(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return 0;
    }
    
    return g_connected_dev_type;
}

int bluetooth_connect(const char *addr)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (!addr || strlen(addr) != 17) {
        LOG_ERROR("Invalid Bluetooth device address");
        return FAILURE;
    }
    
    LOG_INFO("Connecting to Bluetooth device: %s", addr);
    
    // 连接指定的蓝牙设备
    if (hal_bt_connect(addr) != 0) {
        LOG_ERROR("Failed to connect to Bluetooth device");
        return FAILURE;
    }
    
    LOG_INFO("Connecting to Bluetooth device...");
    return SUCCESS;
}

int bluetooth_disconnect(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (!g_bt_cfg.bt_connected) {
        LOG_ERROR("No Bluetooth device connected");
        return FAILURE;
    }
    
    LOG_INFO("Disconnecting Bluetooth device...");
    
    // 停止A2DP音频流
    bt_a2dp_stop_stream();
    
    // 断开当前连接的蓝牙设备
    if (hal_bt_disconnect() != 0) {
        LOG_ERROR("Failed to disconnect Bluetooth device");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth device disconnected");
    return SUCCESS;
}

/**
 * @brief  开始蓝牙音频流传输
 * @return SUCCESS/FAILURE
 */
int bluetooth_start_audio_stream(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (!g_bt_cfg.bt_connected) {
        LOG_ERROR("No Bluetooth device connected");
        return FAILURE;
    }
    
    LOG_INFO("Starting Bluetooth audio stream...");
    
    // 开始A2DP音频流
    if (bt_a2dp_start_stream() != 0) {
        LOG_ERROR("Failed to start Bluetooth audio stream");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth audio stream started");
    return SUCCESS;
}

/**
 * @brief  停止蓝牙音频流传输
 * @return SUCCESS/FAILURE
 */
int bluetooth_stop_audio_stream(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    LOG_INFO("Stopping Bluetooth audio stream...");
    
    // 停止A2DP音频流
    bt_a2dp_stop_stream();
    
    LOG_INFO("Bluetooth audio stream stopped");
    return SUCCESS;
}

/**
 * @brief  设置蓝牙音频流的音量
 * @param  volume 音量值（0-100）
 * @return SUCCESS/FAILURE
 */
int bluetooth_set_audio_volume(int volume)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (volume < 0 || volume > 100) {
        LOG_ERROR("Invalid volume value: %d", volume);
        return FAILURE;
    }
    
    LOG_INFO("Setting Bluetooth audio volume to %d", volume);
    
    // 设置A2DP音频流音量
    if (bt_a2dp_set_volume(volume) != 0) {
        LOG_ERROR("Failed to set Bluetooth audio volume");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief  获取蓝牙音频流的音量
 * @return 音量值（0-100），失败返回-1
 */
int bluetooth_get_audio_volume(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return -1;
    }
    
    // 获取A2DP音频流音量
    int volume = bt_a2dp_get_volume();
    
    LOG_INFO("Current Bluetooth audio volume: %d", volume);
    return volume;
}

/**
 * @brief  设置蓝牙音频流的参数
 * @param  sample_rate 采样率（如44100、48000等）
 * @param  channels 声道数（1或2）
 * @param  bit_depth 比特深度（8、16、24等）
 * @return SUCCESS/FAILURE
 */
int bluetooth_set_audio_params(int sample_rate, int channels, int bit_depth)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (sample_rate <= 0 || channels <= 0 || bit_depth <= 0) {
        LOG_ERROR("Invalid audio parameters");
        return FAILURE;
    }
    
    LOG_INFO("Setting Bluetooth audio parameters: sample_rate=%d, channels=%d, bit_depth=%d", 
             sample_rate, channels, bit_depth);
    
    // 设置A2DP音频参数
    if (hal_bt_a2dp_set_audio_params(sample_rate, channels, bit_depth) != 0) {
        LOG_ERROR("Failed to set Bluetooth audio parameters");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth audio parameters set successfully");
    return SUCCESS;
}

/**
 * @brief  获取蓝牙音频流的状态
 * @return 音频流状态：0表示未启动，1表示正在播放，2表示暂停
 */
int bluetooth_get_audio_stream_state(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return 0;
    }
    
    if (!g_bt_cfg.bt_connected) {
        return 0;
    }
    
    // 获取A2DP媒体状态
    int media_status = hal_bt_a2dp_get_media_status();
    
    LOG_INFO("Current Bluetooth audio stream state: %d", media_status);
    return media_status;
}

/**
 * @brief  蓝牙播放控制：播放
 * @return SUCCESS/FAILURE
 */
int bluetooth_play(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (!g_bt_cfg.bt_connected) {
        LOG_ERROR("No Bluetooth device connected");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth play control: play");
    
    // 发送播放命令
    if (hal_bt_a2dp_play() != 0) {
        LOG_ERROR("Failed to send play command");
        return FAILURE;
    }
    
    // 发送播放事件
    event_notify(EVENT_BT_PLAY_START, NULL);
    event_notify(EVENT_PLAY_START, NULL);
    
    LOG_INFO("Bluetooth play command sent");
    return SUCCESS;
}

/**
 * @brief  蓝牙播放控制：暂停
 * @return SUCCESS/FAILURE
 */
int bluetooth_pause(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (!g_bt_cfg.bt_connected) {
        LOG_ERROR("No Bluetooth device connected");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth play control: pause");
    
    // 发送暂停命令
    if (hal_bt_a2dp_pause() != 0) {
        LOG_ERROR("Failed to send pause command");
        return FAILURE;
    }
    
    // 发送暂停事件
    event_notify(EVENT_BT_PLAY_PAUSE, NULL);
    event_notify(EVENT_PLAY_PAUSE, NULL);
    
    LOG_INFO("Bluetooth pause command sent");
    return SUCCESS;
}

/**
 * @brief  蓝牙播放控制：停止
 * @return SUCCESS/FAILURE
 */
int bluetooth_stop(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (!g_bt_cfg.bt_connected) {
        LOG_ERROR("No Bluetooth device connected");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth play control: stop");
    
    // 发送停止命令
    if (hal_bt_a2dp_stop() != 0) {
        LOG_ERROR("Failed to send stop command");
        return FAILURE;
    }
    
    // 发送停止事件
    event_notify(EVENT_BT_PLAY_STOP, NULL);
    event_notify(EVENT_PLAY_STOP, NULL);
    
    LOG_INFO("Bluetooth stop command sent");
    return SUCCESS;
}

/**
 * @brief  蓝牙播放控制：下一曲
 * @return SUCCESS/FAILURE
 */
int bluetooth_next(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (!g_bt_cfg.bt_connected) {
        LOG_ERROR("No Bluetooth device connected");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth play control: next");
    
    // 发送下一曲命令
    if (hal_bt_a2dp_next() != 0) {
        LOG_ERROR("Failed to send next command");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth next command sent");
    return SUCCESS;
}

/**
 * @brief  蓝牙播放控制：上一曲
 * @return SUCCESS/FAILURE
 */
int bluetooth_prev(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (!g_bt_cfg.bt_connected) {
        LOG_ERROR("No Bluetooth device connected");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth play control: previous");
    
    // 发送上一曲命令
    if (hal_bt_a2dp_prev() != 0) {
        LOG_ERROR("Failed to send previous command");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth previous command sent");
    return SUCCESS;
}

/**
 * @brief  蓝牙播放控制：音量增加
 * @return SUCCESS/FAILURE
 */
int bluetooth_volume_up(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (!g_bt_cfg.bt_connected) {
        LOG_ERROR("No Bluetooth device connected");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth volume control: up");
    
    // 发送音量增加命令
    if (hal_bt_a2dp_volume_up() != 0) {
        LOG_ERROR("Failed to send volume up command");
        return FAILURE;
    }
    
    // 发送音量变化事件
    event_notify(EVENT_BT_VOLUME_CHANGED, NULL);
    
    LOG_INFO("Bluetooth volume up command sent");
    return SUCCESS;
}

/**
 * @brief  蓝牙播放控制：音量减少
 * @return SUCCESS/FAILURE
 */
int bluetooth_volume_down(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (!g_bt_cfg.bt_connected) {
        LOG_ERROR("No Bluetooth device connected");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth volume control: down");
    
    // 发送音量减少命令
    if (hal_bt_a2dp_volume_down() != 0) {
        LOG_ERROR("Failed to send volume down command");
        return FAILURE;
    }
    
    // 发送音量变化事件
    event_notify(EVENT_BT_VOLUME_CHANGED, NULL);
    
    LOG_INFO("Bluetooth volume down command sent");
    return SUCCESS;
}

/**
 * @brief  蓝牙播放控制：静音
 * @return SUCCESS/FAILURE
 */
int bluetooth_mute(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (!g_bt_cfg.bt_connected) {
        LOG_ERROR("No Bluetooth device connected");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth volume control: mute");
    
    // 发送静音命令
    if (hal_bt_a2dp_mute() != 0) {
        LOG_ERROR("Failed to send mute command");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth mute command sent");
    return SUCCESS;
}

/**
 * @brief  蓝牙播放控制：取消静音
 * @return SUCCESS/FAILURE
 */
int bluetooth_unmute(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (!g_bt_cfg.bt_connected) {
        LOG_ERROR("No Bluetooth device connected");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth volume control: unmute");
    
    // 发送取消静音命令
    if (hal_bt_a2dp_unmute() != 0) {
        LOG_ERROR("Failed to send unmute command");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth unmute command sent");
    return SUCCESS;
}

/**
 * @brief  获取蓝牙断连原因
 * @return 断连原因：0表示未知，1表示正常断开，2表示信号丢失，3表示设备电量低，4表示其他
 */
int bluetooth_get_disconnect_reason(void)
{
    return g_disconnect_reason;
}

/**
 * @brief  重置蓝牙重连尝试次数
 * @return SUCCESS/FAILURE
 */
int bluetooth_reset_reconnect_attempts(void)
{
    g_reconnect_attempts = 0;
    LOG_INFO("Bluetooth reconnect attempts reset");
    return SUCCESS;
}

/**
 * @brief  设置蓝牙重连参数
 * @param  max_attempts 最大重连尝试次数
 * @param  interval 重连间隔（秒）
 * @return SUCCESS/FAILURE
 */
int bluetooth_set_reconnect_params(int max_attempts, int interval)
{
    if (max_attempts <= 0 || interval <= 0) {
        LOG_ERROR("Invalid reconnect parameters");
        return FAILURE;
    }
    
    g_max_reconnect_attempts = max_attempts;
    g_reconnect_interval = interval;
    
    LOG_INFO("Bluetooth reconnect parameters set: max_attempts=%d, interval=%d", 
             max_attempts, interval);
    return SUCCESS;
}

/**
 * @brief  获取蓝牙重连参数
 * @param  max_attempts 最大重连尝试次数
 * @param  interval 重连间隔（秒）
 * @return SUCCESS/FAILURE
 */
int bluetooth_get_reconnect_params(int *max_attempts, int *interval)
{
    if (!max_attempts || !interval) {
        LOG_ERROR("Invalid parameters");
        return FAILURE;
    }
    
    *max_attempts = g_max_reconnect_attempts;
    *interval = g_reconnect_interval;
    
    LOG_INFO("Bluetooth reconnect parameters: max_attempts=%d, interval=%d", 
             *max_attempts, *interval);
    return SUCCESS;
}

/**
 * @brief  清理蓝牙连接资源
 * @return SUCCESS/FAILURE
 */
int bluetooth_cleanup_connection(void)
{
    LOG_INFO("Cleaning up Bluetooth connection resources...");
    
    // 停止A2DP音频流
    bt_a2dp_stop_stream();
    
    // 清空连接设备信息
    memset(g_connected_dev_addr, 0, sizeof(g_connected_dev_addr));
    memset(g_connected_dev_name, 0, sizeof(g_connected_dev_name));
    g_connected_dev_type = 0;
    
    // 重置断连原因和重连尝试次数
    g_disconnect_reason = 0;
    g_reconnect_attempts = 0;
    
    // 更新LED状态
    led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_OFF);
    
    // 更新LCD显示
    lcd_display_text(0, 0, "BT: Ready");
    
    LOG_INFO("Bluetooth connection resources cleaned up");
    return SUCCESS;
}

int bluetooth_set_auto_connect(bool auto_connect)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    g_bt_cfg.bt_auto_connect = auto_connect;
    LOG_INFO("Bluetooth auto-connect set to: %d", auto_connect);
    return SUCCESS;
}

bool bluetooth_get_auto_connect(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return false;
    }
    
    return g_bt_cfg.bt_auto_connect;
}

void bluetooth_event_poll(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_DEBUG("Bluetooth module not initialized, skipping event poll");
        return;
    }
    
    // 轮询蓝牙连接状态
    if (g_bt_cfg.bt_enable) {
        LOG_DEBUG("Polling Bluetooth events...");
        
        // 获取蓝牙连接状态 - 检查蓝牙设备是否已连接
        // 返回值：1表示已连接，0表示未连接
        int conn_status = hal_bt_get_connection_status();
        
        if (conn_status < 0) {
            LOG_ERROR("Failed to get Bluetooth connection status: %d", conn_status);
            return;
        }
        
        LOG_DEBUG("Bluetooth connection status: %d", conn_status);
        
        if (conn_status != g_bt_cfg.bt_connected) {
            g_bt_cfg.bt_connected = conn_status;
            
            if (conn_status) {
                LOG_INFO("Bluetooth connected");
                
                // 获取连接的设备信息
                if (hal_bt_get_connected_dev_addr(g_connected_dev_addr, sizeof(g_connected_dev_addr)) == 0) {
                    LOG_INFO("Connected device address: %s", g_connected_dev_addr);
                }
                
                if (hal_bt_get_connected_dev_name(g_connected_dev_name, sizeof(g_connected_dev_name)) == 0) {
                    LOG_INFO("Connected device name: %s", g_connected_dev_name);
                }
                
                if (hal_bt_get_connected_dev_type(&g_connected_dev_type) == 0) {
                    LOG_INFO("Connected device type: %d", g_connected_dev_type);
                }
                
                // 更新LED状态
                led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_ON);
                
                // 更新LCD显示
                char lcd_msg[32] = {0};
                if (strlen(g_connected_dev_name) > 0) {
                    snprintf(lcd_msg, sizeof(lcd_msg), "BT: %s", g_connected_dev_name);
                } else {
                    snprintf(lcd_msg, sizeof(lcd_msg), "BT: Connected");
                }
                lcd_display_text(0, 0, lcd_msg);
                
                // 发送蓝牙连接事件
                event_notify(EVENT_BT_CONNECTED, NULL);
                event_notify(EVENT_SOURCE_BT_CONNECTED, NULL);
                
                // 重置重连尝试次数
                g_reconnect_attempts = 0;
                
                // 自动开始A2DP音频流
                if (g_bt_cfg.bt_media_enable) {
                    bt_a2dp_start_stream();
                }
            } else {
                LOG_INFO("Bluetooth disconnected");
                
                // 获取断连原因
                g_disconnect_reason = hal_bt_get_disconnect_reason();
                LOG_INFO("Bluetooth disconnect reason: %d", g_disconnect_reason);
                
                // 根据断连原因进行处理
                switch (g_disconnect_reason) {
                    case 1:
                        LOG_INFO("Bluetooth disconnected normally");
                        break;
                    case 2:
                        LOG_INFO("Bluetooth disconnected due to signal loss");
                        break;
                    case 3:
                        LOG_INFO("Bluetooth disconnected due to low battery");
                        break;
                    default:
                        LOG_INFO("Bluetooth disconnected for unknown reason");
                        break;
                }
                
                // 停止A2DP音频流
                bt_a2dp_stop_stream();
                g_bt_cfg.bt_media_playing = false;
                
                // 清空连接设备信息
                // 注意：保留设备地址用于自动重连
                memset(g_connected_dev_name, 0, sizeof(g_connected_dev_name));
                g_connected_dev_type = 0;
                
                // 更新LED状态
                led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_OFF);
                
                // 更新LCD显示
                char lcd_msg[32] = {0};
                snprintf(lcd_msg, sizeof(lcd_msg), "BT: Disconnected");
                lcd_display_text(0, 0, lcd_msg);
                
                // 发送蓝牙断开事件
                event_notify(EVENT_BT_DISCONNECTED, NULL);
                event_notify(EVENT_SOURCE_BT_DISCONNECTED, NULL);
                
                // 自动重连
                if (g_bt_cfg.bt_auto_connect && strlen(g_connected_dev_addr) > 0) {
                    // 检查重连尝试次数
                    if (g_reconnect_attempts < g_max_reconnect_attempts) {
                        g_reconnect_attempts++;
                        LOG_INFO("Attempting to reconnect to last device... (Attempt %d/%d)", 
                                 g_reconnect_attempts, g_max_reconnect_attempts);
                        
                        // 延迟重连（实际实现中应该使用定时器）
                        // 这里简化处理，直接尝试重连
                        if (hal_bt_connect(g_connected_dev_addr) != 0) {
                            LOG_ERROR("Failed to initiate reconnect");
                        } else {
                            LOG_INFO("Reconnect initiated");
                        }
                    } else {
                        LOG_INFO("Max reconnect attempts reached (%d), stopping auto-reconnect", 
                                 g_max_reconnect_attempts);
                        // 重置重连尝试次数
                        g_reconnect_attempts = 0;
                        // 清空设备地址，停止重连
                        memset(g_connected_dev_addr, 0, sizeof(g_connected_dev_addr));
                    }
                }
            }
        }
        
        // 轮询蓝牙音频流状态
        if (conn_status && g_bt_cfg.bt_media_enable) {
            // 获取蓝牙A2DP媒体状态 - 检查蓝牙音频流是否正在播放
            // 返回值：1表示正在播放，0表示停止
            int media_status = hal_bt_a2dp_get_media_status();
            
            if (media_status != g_bt_cfg.bt_media_playing) {
                g_bt_cfg.bt_media_playing = media_status;
                
                if (media_status) {
                    LOG_INFO("Bluetooth media playing");
                    // 处理蓝牙媒体播放开始
                    // 更新LED状态
                    led_ctrl_set_state(LED_PLAY, LED_STATE_ON);
                    // 更新LCD显示
                    lcd_display_text(1, 0, "BT: Playing");
                    // 发送事件给其他模块
                    event_notify(EVENT_BT_MEDIA_START, NULL);
                    event_notify(EVENT_BT_PLAY_START, NULL);
                    event_notify(EVENT_PLAY_START, NULL);
                } else {
                    LOG_INFO("Bluetooth media stopped");
                    // 处理蓝牙媒体播放停止
                    // 更新LED状态
                    led_ctrl_set_state(LED_PLAY, LED_STATE_OFF);
                    // 更新LCD显示
                    lcd_display_text(1, 0, "BT: Stopped");
                    // 发送事件给其他模块
                    event_notify(EVENT_BT_MEDIA_STOP, NULL);
                    event_notify(EVENT_BT_PLAY_STOP, NULL);
                    event_notify(EVENT_PLAY_STOP, NULL);
                }
            }
        }
        
        // 轮询蓝牙事件 - 处理蓝牙相关的所有事件
        // 通过HAL层统一处理蓝牙A2DP、HFP和MESH事件
        hal_bt_event_poll();
    }
}

#if CONFIG_ENABLE_BT_MESH
/**
 * @brief  蓝牙MESH组网(高端+低音炮专属)
 * @return SUCCESS/FAILURE
 */
int bluetooth_mesh_create_network(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (!g_bt_cfg.mesh_en) {
        LOG_ERROR("Bluetooth MESH not enabled");
        return FAILURE;
    }
    
    LOG_INFO("Creating Bluetooth MESH network...");
    
    // 创建蓝牙MESH网络
    if (hal_bt_mesh_create_network() != 0) {
        LOG_ERROR("Failed to create Bluetooth MESH network");
        return FAILURE;
    }
    
    // 更新LED状态
    led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_BLINK);
    
    // 更新LCD显示
    lcd_display_text(0, 0, "BT: MESH Network Created");
    
    LOG_INFO("Bluetooth MESH network created successfully");
    return SUCCESS;
}

/**
 * @brief  蓝牙MESH配对(高端+低音炮专属)
 * @return SUCCESS/FAILURE
 */
int bluetooth_mesh_pair(void)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (!g_bt_cfg.mesh_en) {
        LOG_ERROR("Bluetooth MESH not enabled");
        return FAILURE;
    }
    
    LOG_INFO("Starting Bluetooth MESH pairing...");
    
    // 开始蓝牙MESH配对
    if (hal_bt_mesh_pair() != 0) {
        LOG_ERROR("Failed to start Bluetooth MESH pairing");
        return FAILURE;
    }
    
    // 更新LED状态
    led_ctrl_set_state(LED_BLUETOOTH, LED_STATE_BLINK);
    
    // 更新LCD显示
    lcd_display_text(0, 0, "BT: MESH Pairing");
    
    LOG_INFO("Bluetooth MESH pairing started");
    return SUCCESS;
}

/**
 * @brief  蓝牙MESH音量同步(高端+低音炮专属)
 * @param  vol 音量值
 * @return SUCCESS/FAILURE
 */
int bluetooth_mesh_sync_volume(int vol)
{
    if (!g_bt_cfg.init_ok) {
        LOG_ERROR("Bluetooth not initialized");
        return FAILURE;
    }
    
    if (!g_bt_cfg.mesh_en) {
        LOG_ERROR("Bluetooth MESH not enabled");
        return FAILURE;
    }
    
    if (vol < 0 || vol > 100) {
        LOG_ERROR("Invalid volume value: %d", vol);
        return FAILURE;
    }
    
    LOG_INFO("Syncing Bluetooth MESH volume to %d", vol);
    
    // 同步蓝牙MESH音量
    if (hal_bt_mesh_sync_volume(vol) != 0) {
        LOG_ERROR("Failed to sync Bluetooth MESH volume");
        return FAILURE;
    }
    
    LOG_INFO("Bluetooth MESH volume synced successfully");
    return SUCCESS;
}
#endif