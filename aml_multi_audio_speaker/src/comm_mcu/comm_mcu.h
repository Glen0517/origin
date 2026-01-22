#ifndef __COMM_MCU_H__
#define __COMM_MCU_H__

#include <stdint.h>

// 通信MCU模块对外接口

/**
 * @brief 初始化通信MCU模块
 * @return 成功返回0，失败返回-1
 */
int comm_mcu_init(void);

/**
 * @brief 反初始化通信MCU模块
 */
void comm_mcu_deinit(void);

/**
 * @brief 发送原始数据到MCU
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return 成功返回发送的字节数，失败返回-1
 */
int comm_mcu_send(unsigned char *data, int len);

/**
 * @brief 发送命令到MCU
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

#endif // __COMM_MCU_H__
