#ifndef __UART_MCU_COMM_H__
#define __UART_MCU_COMM_H__

#include "common_def.h"

#define UART_DEV_PATH       "/dev/ttyS0"
#define UART_BAUDRATE       9600
#define UART_DATA_BITS      8
#define UART_STOP_BITS      1
#define UART_PARITY         0

// 命令码定义
#define CMD_QUERY_KEY_STATUS     0x01
#define CMD_KEY_STATUS_RESP      0x03
#define CMD_QUERY_TEMP_HUMID     0x02
#define CMD_TEMP_HUMID_RESP      0x04
#define CMD_QUERY_VERSION        0x05
#define CMD_VERSION_RESP         0x06
#define CMD_SET_LED_STATE        0x07
#define CMD_SET_LED_RESP         0x08

// 按键状态位定义
#define KEY_BIT_PLAY_PAUSE       (1 << 0)
#define KEY_BIT_VOL_UP           (1 << 1)
#define KEY_BIT_VOL_DOWN         (1 << 2)
#define KEY_BIT_SOURCE_SWITCH    (1 << 3)
#define KEY_BIT_SOUND_MODE       (1 << 4)
#define KEY_BIT_BASS_UP          (1 << 5)
#define KEY_BIT_TREBLE_UP        (1 << 6)
#define KEY_BIT_IR_LEARN         (1 << 7)

// 数据包格式定义
#define PACKET_HEADER            0xAA
#define PACKET_TAIL              0x55
#define MAX_PACKET_LEN           128

// 错误码定义
#define UART_ERR_NONE            0
#define UART_ERR_TIMEOUT         1
#define UART_ERR_CHECKSUM        2
#define UART_ERR_INVALID_PACKET  3
#define UART_ERR_UNKNOWN_CMD     4

int comm_mcu_init(void);
void comm_mcu_deinit(void);
int comm_mcu_send(unsigned char *data, int len);

/**
 * @brief 发送命令到MCU（带响应）
 * @param cmd 命令码
 * @param data 数据缓冲区
 * @param data_len 数据长度
 * @return 成功返回响应长度，失败返回-1
 */
int comm_mcu_send_cmd(uint8_t cmd, uint8_t *data, int data_len);

/**
 * @brief 设置LED状态
 * @param led_idx LED索引
 * @param led_state LED状态
 * @return 成功返回响应长度，失败返回-1
 */
int comm_mcu_set_led(int led_idx, int led_state);

/**
 * @brief 查询按键/红外状态
 * @return 成功返回响应长度，失败返回-1
 */
int comm_mcu_query_key_status(void);

/**
 * @brief 查询温度湿度
 * @return 成功返回响应长度，失败返回-1
 */
int comm_mcu_query_temp_humid(void);

/**
 * @brief 查询固件版本
 * @return 成功返回响应长度，失败返回-1
 */
int comm_mcu_query_version(void);

/**
 * @brief 发送命令到MCU并获取详细响应
 * @param cmd 命令码
 * @param data 数据缓冲区
 * @param data_len 数据长度
 * @param resp_data 响应数据缓冲区
 * @param resp_len 响应数据长度
 * @return 成功返回0，失败返回-1
 */
int comm_mcu_send_cmd_with_response(uint8_t cmd, uint8_t *data, int data_len, uint8_t *resp_data, int *resp_len);

#endif // __UART_MCU_COMM_H__