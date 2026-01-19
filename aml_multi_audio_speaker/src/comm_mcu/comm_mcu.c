#include "comm_mcu.h"
#include "comm_mcu_priv.h"
#include "logger.h"

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
    return uart_mcu_send_cmd(cmd, data, data_len);
}

/**
 * @brief 设置LED状态
 * @param led_idx LED索引
 * @param led_state LED状态
 * @return 成功返回发送的字节数，失败返回-1
 */
int comm_mcu_set_led(int led_idx, int led_state) {
    uint8_t data[2] = {0};
    data[0] = (uint8_t)led_idx;
    data[1] = (uint8_t)led_state;
    return comm_mcu_send_cmd(CMD_SET_LED_STATE, data, 2);
}

/**
 * @brief 查询按键/红外状态
 * @return 成功返回发送的字节数，失败返回-1
 */
int comm_mcu_query_key_status(void) {
    return comm_mcu_send_cmd(CMD_QUERY_KEY_STATUS, NULL, 0);
}

/**
 * @brief 查询温度湿度
 * @return 成功返回发送的字节数，失败返回-1
 */
int comm_mcu_query_temp_humid(void) {
    return comm_mcu_send_cmd(CMD_QUERY_TEMP_HUMID, NULL, 0);
}

/**
 * @brief 查询固件版本
 * @return 成功返回发送的字节数，失败返回-1
 */
int comm_mcu_query_version(void) {
    return comm_mcu_send_cmd(CMD_QUERY_VERSION, NULL, 0);
}