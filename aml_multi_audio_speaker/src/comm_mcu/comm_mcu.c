#include "comm_mcu.h"
#include "comm_mcu_priv.h"
#include "logger.h"
#include "audio_source.h"
#include "common_def.h"

static CommMcuCfg_t g_mcu_cfg = {0};

int comm_mcu_init(void) {
    memset(&g_mcu_cfg, 0, sizeof(CommMcuCfg_t));
    uart_txrx_init();
    amp_ctrl_init();
    g_mcu_cfg.baud = 9600;
    g_mcu_cfg.init_ok = 1;
    LOG_INFO("Comm MCU init: 9600bps");
    return 0;
}

void comm_mcu_deinit(void) {
    if (g_mcu_cfg.init_ok) {
        amp_ctrl_deinit();
        uart_txrx_deinit();
        g_mcu_cfg.init_ok = 0;
        LOG_INFO("Comm MCU deinit success");
    }
}

int comm_mcu_send(unsigned char *data, int len) {
    if (!g_mcu_cfg.init_ok || !data || len <= 0) {
        LOG_ERROR("Send data failed: invalid param");
        return -1;
    }
    LOG_DEBUG("Send %d bytes to MCU", len);
    return uart_mcu_send_raw_data(data, len);
}

/**
 * @brief 发送命令到MCU
 * @param cmd 命令码
 * @param data 数据缓冲区
 * @param data_len 数据长度
 * @return 成功返回发送的字节数，失败返回-1
 */
int comm_mcu_send_cmd(uint8_t cmd, uint8_t *data, int data_len) {
    if (!g_mcu_cfg.init_ok) {
        LOG_ERROR("Send cmd failed: not initialized");
        return -1;
    }
    
    // 发送命令并等待响应
    uint8_t resp_data[MAX_PACKET_LEN] = {0};
    int resp_len = 0;
    int ret = uart_mcu_send_cmd_with_resp(cmd, data, data_len, resp_data, &resp_len, 1000);
    
    if (ret != 0) {
        LOG_ERROR("Send cmd with resp failed: %d", ret);
        return -1;
    }
    
    LOG_DEBUG("Command %02X executed successfully, response length: %d", cmd, resp_len);
    return resp_len;
}

/**
 * @brief 设置LED状态
 * @param led_idx LED索引
 * @param led_state LED状态
 * @return 成功返回响应长度，失败返回-1
 */
int comm_mcu_set_led(int led_idx, int led_state) {
    uint8_t data[2] = {0};
    data[0] = (uint8_t)led_idx;
    data[1] = (uint8_t)led_state;
    return comm_mcu_send_cmd(CMD_SET_LED_STATE, data, 2);
}

/**
 * @brief 查询按键/红外状态
 * @return 成功返回响应长度，失败返回-1
 */
int comm_mcu_query_key_status(void) {
    uint8_t resp_data[MAX_PACKET_LEN] = {0};
    int resp_len = 0;
    int ret = comm_mcu_send_cmd_with_response(CMD_QUERY_KEY_STATUS, NULL, 0, resp_data, &resp_len);
    
    if (ret == 0 && resp_len > 0) {
        // 解析按键和红外指令，实现输入源切换
        // 假设resp_data[0]是指令类型，resp_data[1]是指令值
        uint8_t cmd_type = resp_data[0];
        uint8_t cmd_value = resp_data[1];
        
        LOG_DEBUG("Received key/IR command: type=%02X, value=%02X", cmd_type, cmd_value);
        
        // 处理输入源切换指令
        switch (cmd_type) {
            case 0x01: // 按键指令
                switch (cmd_value) {
                    case 0x01: // 切换到蓝牙
                        audio_source_switch(SOURCE_BLUETOOTH);
                        LOG_INFO("Switch to Bluetooth source via key");
                        break;
                    case 0x02: // 切换到USB
                        audio_source_switch(SOURCE_USB);
                        LOG_INFO("Switch to USB source via key");
                        break;
                    case 0x03: // 切换到HDMI ARC
                        audio_source_switch(SOURCE_HDMI_ARC);
                        LOG_INFO("Switch to HDMI ARC source via key");
                        break;
                    case 0x04: // 切换到SPDIF
                        audio_source_switch(SOURCE_SPDIF);
                        LOG_INFO("Switch to SPDIF source via key");
                        break;
                    case 0x05: // 切换到AUX
                        audio_source_switch(SOURCE_AUX);
                        LOG_INFO("Switch to AUX source via key");
                        break;
                    case 0x06: // 切换到下一个音源
                        audio_source_switch_next();
                        LOG_INFO("Switch to next source via key");
                        break;
                    default:
                        LOG_DEBUG("Unknown key command: %02X", cmd_value);
                        break;
                }
                break;
            case 0x02: // 红外指令
                switch (cmd_value) {
                    case 0x11: // 红外切换到蓝牙
                        audio_source_switch(SOURCE_BLUETOOTH);
                        LOG_INFO("Switch to Bluetooth source via IR");
                        break;
                    case 0x12: // 红外切换到USB
                        audio_source_switch(SOURCE_USB);
                        LOG_INFO("Switch to USB source via IR");
                        break;
                    case 0x13: // 红外切换到HDMI ARC
                        audio_source_switch(SOURCE_HDMI_ARC);
                        LOG_INFO("Switch to HDMI ARC source via IR");
                        break;
                    case 0x14: // 红外切换到SPDIF
                        audio_source_switch(SOURCE_SPDIF);
                        LOG_INFO("Switch to SPDIF source via IR");
                        break;
                    case 0x15: // 红外切换到AUX
                        audio_source_switch(SOURCE_AUX);
                        LOG_INFO("Switch to AUX source via IR");
                        break;
                    case 0x16: // 红外切换到下一个音源
                        audio_source_switch_next();
                        LOG_INFO("Switch to next source via IR");
                        break;
                    default:
                        LOG_DEBUG("Unknown IR command: %02X", cmd_value);
                        break;
                }
                break;
            default:
                LOG_DEBUG("Unknown command type: %02X", cmd_type);
                break;
        }
    }
    
    return ret == 0 ? resp_len : -1;
}

/**
 * @brief 查询温度湿度
 * @return 成功返回响应长度，失败返回-1
 */
int comm_mcu_query_temp_humid(void) {
    return comm_mcu_send_cmd(CMD_QUERY_TEMP_HUMID, NULL, 0);
}

/**
 * @brief 查询固件版本
 * @return 成功返回响应长度，失败返回-1
 */
int comm_mcu_query_version(void) {
    return comm_mcu_send_cmd(CMD_QUERY_VERSION, NULL, 0);
}

/**
 * @brief 发送命令到MCU并获取详细响应
 * @param cmd 命令码
 * @param data 数据缓冲区
 * @param data_len 数据长度
 * @param resp_data 响应数据缓冲区
 * @param resp_len 响应数据长度
 * @return 成功返回0，失败返回-1
 */
int comm_mcu_send_cmd_with_response(uint8_t cmd, uint8_t *data, int data_len, uint8_t *resp_data, int *resp_len) {
    if (!g_mcu_cfg.init_ok) {
        LOG_ERROR("Send cmd failed: not initialized");
        return -1;
    }
    
    if (!resp_len) {
        LOG_ERROR("Invalid resp_len parameter");
        return -1;
    }
    
    return uart_mcu_send_cmd_with_resp(cmd, data, data_len, resp_data, resp_len, 1000);
}