#include "comm_mcu_priv.h"
#include "logger.h"
#include "product_type.h"

#include <aml_uart.h>        // 晶晨UART SDK

#include "peripheral_priv.h" // 外设私有定义，包含按键/红外变量声明

static bool g_uart_init = false;
static int g_uart_fd = -1;
static int g_uart_baudrate = UART_BAUDRATE;

/**
 * @brief 计算校验和
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return 校验和值
 */
uint8_t uart_calculate_checksum(uint8_t *data, int len) {
    uint8_t checksum = 0;
    for (int i = 0; i < len; i++) {
        checksum += data[i];
    }
    return checksum;
}

/**
 * @brief 打包数据
 * @param cmd 命令码
 * @param data 数据缓冲区
 * @param data_len 数据长度
 * @param packet 打包后的数据包
 * @param packet_len 数据包长度
 * @return 0表示成功，非0表示失败
 */
int uart_pack_data(uint8_t cmd, uint8_t *data, int data_len, uint8_t *packet, int *packet_len) {
    if (!packet || !packet_len || data_len > MAX_PACKET_LEN - 6) {
        LOG_ERROR("Invalid parameters or data too long");
        return -1;
    }
    
    // 构建数据包
    int index = 0;
    packet[index++] = PACKET_HEADER;    // 头部
    packet[index++] = cmd;              // 命令码
    packet[index++] = (uint8_t)data_len;// 数据长度
    
    // 数据部分
    if (data_len > 0 && data) {
        memcpy(&packet[index], data, data_len);
        index += data_len;
    }
    
    // 校验和
    uint8_t checksum = uart_calculate_checksum(&packet[1], index - 1);
    packet[index++] = checksum;
    
    // 尾部
    packet[index++] = PACKET_TAIL;
    
    *packet_len = index;
    return 0;
}

/**
 * @brief 解析数据
 * @param packet 数据包
 * @param packet_len 数据包长度
 * @param cmd 命令码
 * @param data 数据缓冲区
 * @param data_len 数据长度
 * @return 0表示成功，非0表示失败
 */
int uart_unpack_data(uint8_t *packet, int packet_len, uint8_t *cmd, uint8_t *data, int *data_len) {
    if (!packet || !cmd || !data_len) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    // 检查数据包长度
    if (packet_len < 5) {
        LOG_ERROR("Invalid packet length: %d", packet_len);
        return UART_ERR_INVALID_PACKET;
    }
    
    // 检查头部和尾部
    if (packet[0] != PACKET_HEADER || packet[packet_len - 1] != PACKET_TAIL) {
        LOG_ERROR("Invalid packet header/tail");
        return UART_ERR_INVALID_PACKET;
    }
    
    // 提取命令码和数据长度
    *cmd = packet[1];
    uint8_t expected_data_len = packet[2];
    
    // 检查数据包长度是否匹配
    if (packet_len != expected_data_len + 5) {
        LOG_ERROR("Packet length mismatch: expected %d, actual %d", expected_data_len + 5, packet_len);
        return UART_ERR_INVALID_PACKET;
    }
    
    // 验证校验和
    uint8_t expected_checksum = packet[packet_len - 2];
    uint8_t actual_checksum = uart_calculate_checksum(&packet[1], packet_len - 3);
    if (expected_checksum != actual_checksum) {
        LOG_ERROR("Checksum mismatch: expected 0x%02X, actual 0x%02X", expected_checksum, actual_checksum);
        return UART_ERR_CHECKSUM;
    }
    
    // 提取数据
    if (expected_data_len > 0 && data) {
        memcpy(data, &packet[3], expected_data_len);
    }
    *data_len = expected_data_len;
    
    return UART_ERR_NONE;
}

/**
 * @brief UART接收回调函数
 */
static void uart_rx_callback(uint8_t *data, int len) {
    if (g_uart_init && data && len > 0) {
        LOG_DEBUG("UART received %d bytes", len);
        
        // 解析接收到的数据包
        uint8_t cmd = 0;
        uint8_t payload[MAX_PACKET_LEN] = {0};
        int payload_len = 0;
        
        int ret = uart_unpack_data(data, len, &cmd, payload, &payload_len);
        if (ret != UART_ERR_NONE) {
            LOG_ERROR("Failed to unpack data: %d", ret);
            return;
        }
        
        LOG_DEBUG("UART unpacked cmd: 0x%02X, data_len: %d", cmd, payload_len);
        
        // 根据命令码处理数据
        switch (cmd) {
            case CMD_TEMP_HUMID_RESP: // 温度湿度响应
                if (payload_len >= 4) {
                    int temp = (payload[0] << 8) | payload[1];
                    int humidity = (payload[2] << 8) | payload[3];
                    LOG_INFO("MCU temperature: %d°C, humidity: %d%%", temp, humidity);
                    // 可以发送温度湿度事件通知
                }
                break;
                
            case CMD_KEY_STATUS_RESP: // 按键/红外状态响应
                if (payload_len >= 1) {
                    uint8_t key_state = payload[0];
                    LOG_INFO("MCU key state: 0x%02X", key_state);
                    
                    // 将MCU返回的按键状态转换为系统内部的KeyEvent_e事件
                    if (g_key_ir_init) {
                        KeyEvent_e event = KEY_EVENT_NONE;
                        
                        // 检查按键状态位
                        if (key_state & KEY_BIT_PLAY_PAUSE) {
                            event = KEY_EVENT_PLAY_PAUSE;
                        } else if (key_state & KEY_BIT_VOL_UP) {
                            event = KEY_EVENT_VOL_UP;
                        } else if (key_state & KEY_BIT_VOL_DOWN) {
                            event = KEY_EVENT_VOL_DOWN;
                        } else if (key_state & KEY_BIT_SOURCE_SWITCH) {
                            event = KEY_EVENT_SOURCE_SWITCH;
                        } else if (key_state & KEY_BIT_SOUND_MODE) {
                            event = KEY_EVENT_SOUND_MODE;
                        } else if (key_state & KEY_BIT_BASS_UP) {
                            event = KEY_EVENT_BASS_UP;
                        } else if (key_state & KEY_BIT_TREBLE_UP) {
                            event = KEY_EVENT_TREBLE_UP;
                        } else if (key_state & KEY_BIT_IR_LEARN) {
                            event = KEY_EVENT_IR_LEARN;
                        } else if (key_state & KEY_BIT_NEXT) {
                            event = KEY_EVENT_NEXT;
                        } else if (key_state & KEY_BIT_PREV) {
                            event = KEY_EVENT_PREV;
                        }
                        
                        // 如果有按键事件，存储到全局变量
                        if (event != KEY_EVENT_NONE) {
                            g_last_key_event = event;
                            LOG_INFO("Key/IR event from MCU: %d", event);
                        }
                    }
                }
                break;
                
            case CMD_VERSION_RESP: // 固件版本响应
                if (payload_len >= 3) {
                    int major = payload[0];
                    int minor = payload[1];
                    int patch = payload[2];
                    LOG_INFO("MCU firmware version: %d.%d.%d", major, minor, patch);
                }
                break;
                
            case CMD_SET_LED_RESP: // LED状态设置响应
                if (payload_len >= 1) {
                    uint8_t status = payload[0];
                    LOG_INFO("LED state set response: %s", status ? "success" : "failed");
                }
                break;
                
            default:
                LOG_DEBUG("Unknown MCU command: 0x%02X", cmd);
                break;
        }
    }
}

int uart_txrx_init(void) {
    if (g_uart_init) {
        LOG_INFO("UART TX/RX already initialized");
        return 0;
    }
    
    // 初始化Amlogic UART SDK - 初始化UART通信子系统
    // 返回值：0表示成功，非0表示失败
    if (aml_uart_init() != 0) {
        LOG_ERROR("UART TX/RX init failed: UART SDK init error");
        return -1;
    }
    
    // 打开UART设备 - 打开指定的UART设备文件
    // 参数1：UART设备路径，如"/dev/ttyS0"
    // 参数2：波特率，如115200
    // 参数3：数据位，如8
    // 参数4：停止位，如1
    // 参数5：校验位，如0（无校验）
    // 返回值：成功返回文件描述符，失败返回-1
    g_uart_fd = aml_uart_open(UART_DEV_PATH, g_uart_baudrate, 
                              UART_DATA_BITS, UART_STOP_BITS, UART_PARITY);
    
    if (g_uart_fd < 0) {
        LOG_ERROR("UART TX/RX init failed: open UART device error");
        // 反初始化Amlogic UART SDK - 清理UART通信子系统
        aml_uart_deinit();
        return -1;
    }
    
    // 设置UART接收回调 - 当接收到数据时触发
    // 参数：接收回调函数指针
    aml_uart_set_rx_callback(uart_rx_callback);
    
    // 启用UART接收中断 - 允许UART接收数据时产生中断
    // 参数：true表示启用，false表示禁用
    aml_uart_enable_rx_irq(true);
    
    g_uart_init = true;
    
    LOG_INFO("UART TX/RX init success");
    LOG_INFO("  Device: %s", UART_DEV_PATH);
    LOG_INFO("  Baudrate: %d", g_uart_baudrate);
    
    return 0;
}

void uart_txrx_deinit(void) {
    if (g_uart_init) {
        // 禁用UART接收中断 - 禁止UART接收数据时产生中断
        // 参数：false表示禁用
        aml_uart_enable_rx_irq(false);
        
        // 关闭UART设备 - 关闭打开的UART设备文件
        // 参数：UART设备文件描述符
        if (g_uart_fd >= 0) {
            aml_uart_close(g_uart_fd);
            g_uart_fd = -1;
        }
        
        // 反初始化Amlogic UART SDK - 清理UART通信子系统
        aml_uart_deinit();
        
        g_uart_init = false;
        
        LOG_INFO("UART TX/RX deinit success");
    }
}

/**
 * @brief 发送命令到MCU
 * @param cmd 命令码
 * @param data 数据缓冲区
 * @param data_len 数据长度
 * @return 成功返回发送的字节数，失败返回-1
 */
int uart_mcu_send_cmd(uint8_t cmd, uint8_t *data, int data_len) {
    if (!g_uart_init || g_uart_fd < 0) {
        LOG_ERROR("UART send failed: not initialized");
        return -1;
    }
    
    // 打包数据
    uint8_t packet[MAX_PACKET_LEN] = {0};
    int packet_len = 0;
    
    int ret = uart_pack_data(cmd, data, data_len, packet, &packet_len);
    if (ret != 0) {
        LOG_ERROR("Failed to pack data: %d", ret);
        return -1;
    }
    
    // 向UART设备写入数据 - 发送数据到MCU
    int sent_len = aml_uart_write(g_uart_fd, packet, packet_len);
    
    if (sent_len != packet_len) {
        LOG_ERROR("UART send failed: sent %d bytes, expected %d bytes", sent_len, packet_len);
        return -1;
    }
    
    LOG_DEBUG("UART sent cmd 0x%02X, %d bytes", cmd, sent_len);
    
    return sent_len;
}

/**
 * @brief 发送原始数据到MCU（兼容旧接口）
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return 成功返回发送的字节数，失败返回-1
 */
int uart_mcu_send_raw_data(uint8_t *data, int len) {
    if (!g_uart_init || !data || len <= 0 || g_uart_fd < 0) {
        LOG_ERROR("UART send failed: invalid parameters or not initialized");
        return -1;
    }
    
    // 向UART设备写入数据 - 发送数据到MCU
    int sent_len = aml_uart_write(g_uart_fd, data, len);
    
    if (sent_len != len) {
        LOG_ERROR("UART send failed: sent %d bytes, expected %d bytes", sent_len, len);
        return -1;
    }
    
    LOG_DEBUG("UART sent raw data: %d bytes", sent_len);
    
    return sent_len;
}

/**
 * @brief 从MCU接收数据
 */
int uart_mcu_recv_data(uint8_t *buf, int len, int timeout_ms) {
    if (!g_uart_init || !buf || len <= 0 || g_uart_fd < 0) {
        LOG_ERROR("UART receive failed: invalid parameters or not initialized");
        return -1;
    }
    
    // 从UART设备读取数据 - 接收MCU发送的数据
    // 参数1：UART设备文件描述符
    // 参数2：接收数据的缓冲区
    // 参数3：期望接收的数据长度
    // 参数4：超时时间（毫秒）
    // 返回值：成功返回实际接收的字节数，失败返回-1
    int recv_len = aml_uart_read(g_uart_fd, buf, len, timeout_ms);
    
    if (recv_len < 0) {
        LOG_ERROR("UART receive failed: timeout or error");
        return -1;
    }
    
    if (recv_len > 0) {
        LOG_DEBUG("UART received %d bytes: %.*s", recv_len, recv_len, buf);
    }
    
    return recv_len;
}

/**
 * @brief 根据命令码获取对应的响应命令码
 * @param cmd 命令码
 * @return 响应命令码，无对应响应返回0
 */
static uint8_t get_response_cmd(uint8_t cmd) {
    switch (cmd) {
        case CMD_QUERY_KEY_STATUS:
            return CMD_KEY_STATUS_RESP;
        case CMD_QUERY_TEMP_HUMID:
            return CMD_TEMP_HUMID_RESP;
        case CMD_QUERY_VERSION:
            return CMD_VERSION_RESP;
        case CMD_SET_LED_STATE:
            return CMD_SET_LED_RESP;
        default:
            return 0;
    }
}

/**
 * @brief 发送命令到MCU并等待响应
 * @param cmd 命令码
 * @param data 数据缓冲区
 * @param data_len 数据长度
 * @param resp_data 响应数据缓冲区
 * @param resp_len 响应数据长度
 * @param timeout_ms 超时时间（毫秒）
 * @return 成功返回0，失败返回-1
 */
int uart_mcu_send_cmd_with_resp(uint8_t cmd, uint8_t *data, int data_len, uint8_t *resp_data, int *resp_len, int timeout_ms) {
    if (!g_uart_init || g_uart_fd < 0) {
        LOG_ERROR("UART send failed: not initialized");
        return -1;
    }
    
    if (!resp_len) {
        LOG_ERROR("Invalid resp_len parameter");
        return -1;
    }
    
    // 打包数据
    uint8_t packet[MAX_PACKET_LEN] = {0};
    int packet_len = 0;
    
    int ret = uart_pack_data(cmd, data, data_len, packet, &packet_len);
    if (ret != 0) {
        LOG_ERROR("Failed to pack data: %d", ret);
        return -1;
    }
    
    // 向UART设备写入数据 - 发送数据到MCU
    int sent_len = aml_uart_write(g_uart_fd, packet, packet_len);
    
    if (sent_len != packet_len) {
        LOG_ERROR("UART send failed: sent %d bytes, expected %d bytes", sent_len, packet_len);
        return -1;
    }
    
    LOG_DEBUG("UART sent cmd 0x%02X, %d bytes", cmd, sent_len);
    
    // 计算期望的响应命令码
    uint8_t expected_resp_cmd = get_response_cmd(cmd);
    if (expected_resp_cmd == 0) {
        LOG_WARN("No expected response for cmd 0x%02X", cmd);
        *resp_len = 0;
        return 0;
    }
    
    // 等待响应
    uint8_t resp_packet[MAX_PACKET_LEN] = {0};
    int resp_packet_len = 0;
    int total_recv = 0;
    int max_recv = MAX_PACKET_LEN;
    int timeout = timeout_ms;
    
    // 接收响应数据
    while (total_recv < max_recv) {
        int recv_len = aml_uart_read(g_uart_fd, &resp_packet[total_recv], max_recv - total_recv, timeout);
        
        if (recv_len < 0) {
            LOG_ERROR("UART receive timeout waiting for response");
            return -1;
        }
        
        if (recv_len > 0) {
            total_recv += recv_len;
            LOG_DEBUG("Received %d bytes, total %d bytes", recv_len, total_recv);
            
            // 检查是否接收到完整的数据包
            if (total_recv >= 5) {
                // 查找数据包头部
                int header_idx = -1;
                for (int i = 0; i < total_recv - 4; i++) {
                    if (resp_packet[i] == PACKET_HEADER) {
                        header_idx = i;
                        break;
                    }
                }
                
                if (header_idx != -1) {
                    // 解析数据包长度
                    int data_len_field = header_idx + 2;
                    if (data_len_field < total_recv) {
                        int expected_len = resp_packet[data_len_field] + 5;
                        if (total_recv >= header_idx + expected_len) {
                            // 有完整的数据包
                            resp_packet_len = header_idx + expected_len;
                            break;
                        }
                    }
                }
            }
        }
        
        // 如果已经超时，退出循环
        if (timeout <= 0) {
            break;
        }
        
        // 减少超时时间
        timeout -= 10;
        usleep(10 * 1000); // 10ms
    }
    
    if (resp_packet_len == 0) {
        LOG_ERROR("No complete response received");
        return -1;
    }
    
    // 解析响应数据包
    uint8_t resp_cmd = 0;
    int payload_len = 0;
    
    ret = uart_unpack_data(resp_packet, resp_packet_len, &resp_cmd, resp_data, &payload_len);
    if (ret != UART_ERR_NONE) {
        LOG_ERROR("Failed to unpack response: %d", ret);
        return -1;
    }
    
    // 检查响应命令是否匹配
    if (resp_cmd != expected_resp_cmd) {
        LOG_ERROR("Response cmd mismatch: expected 0x%02X, got 0x%02X", expected_resp_cmd, resp_cmd);
        return -1;
    }
    
    *resp_len = payload_len;
    LOG_DEBUG("Received response cmd 0x%02X, data len %d", resp_cmd, payload_len);
    
    return 0;
}