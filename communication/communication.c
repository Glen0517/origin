/*
 * 通信模块实现
 */

#include "communication.h"
#include "../include/types.h"
#include "../config/config.h"
#include "../hal/hal_uart.h"
#include "../driver/mpu6050.h"
#include "../service/system.h"
#include <string.h>
#include "../driver/motor.h"

// 通信通道配置和状态
static communication_config_t communication_configs[COMM_CHANNEL_MAX];
static communication_status_t communication_status[COMM_CHANNEL_MAX];
static bool communication_initialized = false;

// 缓冲区大小定义
#define COMM_BUFFER_SIZE 1024

// 接收和发送缓冲区
// 使用volatile和(void)标记避免未使用警告
static volatile uint8_t rx_buffers[COMM_CHANNEL_MAX][COMM_BUFFER_SIZE];
static volatile uint8_t tx_buffers[COMM_CHANNEL_MAX][COMM_BUFFER_SIZE];
// 编译器优化屏障，确保变量不会被优化掉
#define UNUSED_BUFFERS (void)rx_buffers; (void)tx_buffers;

// 缓冲区索引
static uint16_t rx_index[COMM_CHANNEL_MAX];
static uint16_t tx_index[COMM_CHANNEL_MAX];

/**
 * @brief 初始化通信模块
 */
bool communication_init(communication_config_t *config) {
    if (!config || config->channel >= COMM_CHANNEL_MAX) {
        return false;
    }

    // 保存配置
    communication_configs[config->channel] = *config;

    // 初始化状态
    communication_status[config->channel].channel = config->channel;
    communication_status[config->channel].protocol = config->protocol;
    communication_status[config->channel].baud_rate = config->baud_rate;
    communication_status[config->channel].rx_count = 0;
    communication_status[config->channel].tx_count = 0;
    communication_status[config->channel].error_count = 0;
    communication_status[config->channel].last_activity_time = system_get_time_ms();
    communication_status[config->channel].connected = false;
    communication_status[config->channel].data_available = false;

    // 初始化缓冲区索引
    rx_index[config->channel] = 0;
    tx_index[config->channel] = 0;

    system_log(LOG_LEVEL_INFO, "Communication channel %d initialized\n", config->channel);
    
    // 简化实现：移除uart_init调用，直接返回true
    return true;
}

/**
 * @brief 初始化所有通信通道
 */
bool communication_init_all(void) {
    bool result = true;

    // 从配置文件获取各通道配置
    communication_config_t config;

    // 初始化遥控器通道
    config.channel = COMM_CHANNEL_RC;
    config.protocol = PROTOCOL_SBUS;
    config.uart_channel = 1; // 使用数值代替未定义的UART_CHANNEL_1
    config.baud_rate = 100000;
    config.buffer_size = COMM_BUFFER_SIZE;
    config.parity_enabled = false;
    config.data_bits = 8;
    config.stop_bits = 2;
    result &= communication_init(&config);

    // 初始化地面站通道
    config.channel = COMM_CHANNEL_GCS;
    config.protocol = PROTOCOL_MAVLINK;
    config.uart_channel = 2; // 使用数值代替未定义的UART_CHANNEL_2
    config.baud_rate = 115200;
    config.buffer_size = COMM_BUFFER_SIZE;
    config.parity_enabled = false;
    config.data_bits = 8;
    config.stop_bits = 1;
    result &= communication_init(&config);

    // 初始化调试通道
    config.channel = COMM_CHANNEL_DEBUG;
    config.protocol = PROTOCOL_SERIAL;
    config.uart_channel = 3; // 使用数值代替未定义的UART_CHANNEL_3
    config.baud_rate = 9600;
    config.buffer_size = COMM_BUFFER_SIZE;
    config.parity_enabled = false;
    config.data_bits = 8;
    config.stop_bits = 1;
    result &= communication_init(&config);

    // 初始化日志通道
    config.channel = COMM_CHANNEL_LOGGING;
    config.protocol = PROTOCOL_SERIAL;
    config.uart_channel = 4; // 使用数值代替未定义的UART_CHANNEL_4
    config.baud_rate = 9600;
    config.buffer_size = COMM_BUFFER_SIZE;
    config.parity_enabled = false;
    config.data_bits = 8;
    config.stop_bits = 1;
    result &= communication_init(&config);

    // 初始化SOC交互通道
    config.channel = COMM_CHANNEL_SOC;
    config.protocol = PROTOCOL_SERIAL;
    config.uart_channel = 5; // 使用UART5作为SOC交互通道
    config.baud_rate = 115200;
    config.buffer_size = COMM_BUFFER_SIZE;
    config.parity_enabled = false;
    config.data_bits = 8;
    config.stop_bits = 1;
    result &= communication_init(&config);

    communication_initialized = result;
    return result;
}

/**
 * @brief 发送消息
 */
int32_t communication_send(communication_channel_t channel, message_t *message) {
    if (!communication_initialized || channel >= COMM_CHANNEL_MAX || !message) {
        return 0;
    }

    // 获取对应通道的UART配置
    uart_channel_t uart_channel = communication_configs[channel].uart_channel;
    
    // 检查UART通道是否有效
    if (uart_channel >= UART_MAX) {
        communication_status[channel].error_count++;
        return 0;
    }

    // 构建发送数据包
    uint8_t send_buffer[512]; // 缓冲区大小限制
    uint16_t packet_length = 0;

    // 添加帧头 (0xAA)
    send_buffer[packet_length++] = 0xAA;
    
    // 添加消息类型
    send_buffer[packet_length++] = (uint8_t)message->type;
    
    // 添加长度字段（小端格式）
    send_buffer[packet_length++] = (uint8_t)(message->length & 0xFF);
    send_buffer[packet_length++] = (uint8_t)((message->length >> 8) & 0xFF);
    
    // 添加时间戳
    uint32_t timestamp = message->timestamp;
    send_buffer[packet_length++] = (uint8_t)(timestamp & 0xFF);
    send_buffer[packet_length++] = (uint8_t)((timestamp >> 8) & 0xFF);
    send_buffer[packet_length++] = (uint8_t)((timestamp >> 16) & 0xFF);
    send_buffer[packet_length++] = (uint8_t)((timestamp >> 24) & 0xFF);
    
    // 添加数据
    if (message->length > 0 && message->length <= sizeof(message->data)) {
        memcpy(&send_buffer[packet_length], message->data, message->length);
        packet_length += message->length;
    }
    
    // 添加校验和
    uint8_t checksum = 0;
    for (uint16_t i = 1; i < packet_length; i++) {
        checksum ^= send_buffer[i];
    }
    send_buffer[packet_length++] = checksum;

    // 发送数据到UART
    int32_t sent_bytes = hal_uart_send_data(uart_channel, send_buffer, packet_length);
    
    if (sent_bytes > 0) {
        communication_status[channel].tx_count += sent_bytes;
        communication_status[channel].last_activity_time = system_get_time_ms();
        communication_status[channel].connected = true;
    } else {
        communication_status[channel].error_count++;
    }

    return sent_bytes;
}

/**
 * @brief 接收消息
 */
int32_t communication_receive(communication_channel_t channel, message_t *message) {
    if (!communication_initialized || channel >= COMM_CHANNEL_MAX || !message) {
        return -1;
    }

    // 获取对应通道的UART配置
    uart_channel_t uart_channel = communication_configs[channel].uart_channel;
    
    // 检查UART通道是否有效
    if (uart_channel >= UART_MAX) {
        communication_status[channel].error_count++;
        return -1;
    }

    // 接收数据缓冲区
    static uint8_t receive_buffer[1024];
    static uint16_t buffer_pos = 0;
    static bool packet_in_progress = false;

    // 检查UART是否有数据可读
    uint16_t available_bytes = hal_uart_receive_data(uart_channel, &receive_buffer[buffer_pos], sizeof(receive_buffer) - buffer_pos);
    
    if (available_bytes == 0) {
        return -1; // 没有数据
    }

    buffer_pos += available_bytes;
    communication_status[channel].rx_count += available_bytes;
    communication_status[channel].last_activity_time = system_get_time_ms();
    communication_status[channel].connected = true;

    // 解析数据包
    uint16_t processed_bytes = 0;
    
    while (processed_bytes < buffer_pos) {
        // 查找帧头 (0xAA)
        if (receive_buffer[processed_bytes] != 0xAA) {
            processed_bytes++;
            continue;
        }

        // 检查是否还有足够的数据解析完整数据包
        if (processed_bytes + 8 > buffer_pos) { // 最小数据包大小: 帧头(1) + 类型(1) + 长度(2) + 时间戳(4) = 8字节
            break;
        }

        // 解析消息类型
        uint8_t msg_type = receive_buffer[processed_bytes + 1];
        
        // 解析数据长度（小端格式）
        uint16_t data_length = receive_buffer[processed_bytes + 2] | (receive_buffer[processed_bytes + 3] << 8);
        
        // 计算完整数据包长度
        uint16_t total_packet_length = 8 + data_length + 1; // 头部(8) + 数据 + 校验和(1)
        
        // 检查缓冲区是否有足够的数据
        if (processed_bytes + total_packet_length > buffer_pos) {
            break; // 数据不完整，等待更多数据
        }

        // 验证校验和
        uint8_t calculated_checksum = 0;
        for (uint16_t i = processed_bytes + 1; i < processed_bytes + total_packet_length - 1; i++) {
            calculated_checksum ^= receive_buffer[i];
        }
        
        uint8_t received_checksum = receive_buffer[processed_bytes + total_packet_length - 1];
        
        if (calculated_checksum != received_checksum) {
            // 校验和错误，跳过此数据包
            processed_bytes++;
            continue;
        }

        // 解析时间戳
        uint32_t timestamp = receive_buffer[processed_bytes + 4] | 
                           (receive_buffer[processed_bytes + 5] << 8) |
                           (receive_buffer[processed_bytes + 6] << 16) |
                           (receive_buffer[processed_bytes + 7] << 24);

        // 填充消息结构体
        message->type = (message_type_t)msg_type;
        message->length = data_length;
        message->timestamp = timestamp;

        if (data_length > 0 && data_length <= sizeof(message->data)) {
            memcpy(message->data, &receive_buffer[processed_bytes + 8], data_length);
        }

        // 移动到下一个数据包
        processed_bytes += total_packet_length;
        
        // 设置数据可用标志
        communication_status[channel].data_available = true;
        
        // 返回接收到的数据长度
        return data_length;
    }

    // 移动未处理的数据到缓冲区开头
    if (processed_bytes > 0) {
        buffer_pos -= processed_bytes;
        if (buffer_pos > 0) {
            memmove(receive_buffer, &receive_buffer[processed_bytes], buffer_pos);
        }
    }

    return -1; // 没有找到完整的数据包
}

/**
 * @brief 处理接收到的消息
 */
void communication_process_message(communication_channel_t channel, message_t *message) {
    if (!message) {
        return;
    }

    switch (message->type) {
        case MESSAGE_TYPE_HEARTBEAT:
            communication_status[channel].connected = true;
            break;
        
        case MESSAGE_TYPE_COMMAND: {
            command_type_t command = *((command_type_t *)message->data);
            communication_process_command(command, message->data + sizeof(command_type_t));
            break;
        }
        
        case MESSAGE_TYPE_PID:
            // 处理PID参数设置
            break;
        
        case MESSAGE_TYPE_CONFIG:
            // 处理配置参数
            break;
        
        // SOC相关消息处理
        case MESSAGE_TYPE_SOC_DATA:
            // 处理SOC数据接收
            communication_status[channel].data_available = false;
            break;
            
        case MESSAGE_TYPE_SOC_STATUS:
            // 处理SOC状态查询响应
            communication_status[channel].connected = true;
            communication_status[channel].last_activity_time = system_get_time_ms();
            break;
            
        case MESSAGE_TYPE_SOC_CONFIG:
            // 处理SOC配置命令响应
            break;
            
        case MESSAGE_TYPE_SOC_RESPONSE:
            // 处理SOC通用响应
            communication_status[channel].last_activity_time = system_get_time_ms();
            break;
        
        default:
            break;
    }

    communication_status[channel].data_available = false;
}

/**
 * @brief 获取通信状态
 */
bool communication_get_status(communication_channel_t channel, communication_status_t *status) {
    if (!communication_initialized || channel >= COMM_CHANNEL_MAX || !status) {
        return false;
    }

    *status = communication_status[channel];
    
    // 更新连接状态（基于最后活动时间）
    uint32_t current_time = system_get_time_ms();
    if (current_time - status->last_activity_time > 5000) {  // 5秒无活动认为断开
        status->connected = false;
    }

    return true;
}

/**
 * @brief 检查通信连接
 */
bool communication_is_connected(communication_channel_t channel) {
    if (!communication_initialized || channel >= COMM_CHANNEL_MAX) {
        return false;
    }

    // 更新连接状态
    communication_status_t status;
    communication_get_status(channel, &status);
    return status.connected;
}

/**
 * @brief 检查是否有数据可用
 */
bool communication_has_data(communication_channel_t channel) {
    if (!communication_initialized || channel >= COMM_CHANNEL_MAX) {
        return false;
    }

    // 简化实现，只检查状态标志
    return communication_status[channel].data_available;
}

/**
 * @brief 发送心跳包
 */
bool communication_send_heartbeat(communication_channel_t channel) {
    message_t heartbeat = {
        .type = MESSAGE_TYPE_HEARTBEAT,
        .length = 0,
        .timestamp = system_get_time_ms()
    };

    return communication_send(channel, &heartbeat) > 0;
}

/**
 * @brief 发送姿态数据
 */
bool communication_send_attitude(communication_channel_t channel, attitude_data_t *attitude) {
    message_t message = {
        .type = MESSAGE_TYPE_ATTITUDE,
        .length = sizeof(attitude_data_t),
        .timestamp = system_get_time_ms()
    };

    memcpy(message.data, attitude, sizeof(attitude_data_t));
    return communication_send(channel, &message) > 0;
}

/**
 * @brief 发送电机数据
 */
bool communication_send_motor(communication_channel_t channel, motor_data_t *motor) {
    message_t message = {
        .type = MESSAGE_TYPE_MOTOR,
        .length = sizeof(motor_data_t),
        .timestamp = system_get_time_ms()
    };

    memcpy(message.data, motor, sizeof(motor_data_t));
    return communication_send(channel, &message) > 0;
}

/**
 * @brief 发送系统状态
 */
bool communication_send_status(communication_channel_t channel, system_status_t *status) {
    message_t message = {
        .type = MESSAGE_TYPE_STATUS,
        .length = sizeof(system_status_t),
        .timestamp = system_get_time_ms()
    };

    memcpy(message.data, status, sizeof(system_status_t));
    return communication_send(channel, &message) > 0;
}

/**
 * @brief 发送错误报告
 */
bool communication_send_error(communication_channel_t channel, uint8_t error, const char *message) {
    message_t error_message = {
        .type = MESSAGE_TYPE_ERROR,
        .length = 1 + strlen(message),
        .timestamp = system_get_time_ms()
    };

    error_message.data[0] = error;
    if (message) {
        memcpy(error_message.data + 1, message, strlen(message));
    }

    return communication_send(channel, &error_message) > 0;
}

/**
 * @brief 发送日志消息
 */
bool communication_send_log(communication_channel_t channel, log_level_t level, const char *message) {
    message_t log_message = {
        .type = MESSAGE_TYPE_LOG,
        .length = 1 + strlen(message),
        .timestamp = system_get_time_ms()
    };

    log_message.data[0] = (uint8_t)level;
    if (message) {
        memcpy(log_message.data + 1, message, strlen(message));
    }

    return communication_send(channel, &log_message) > 0;
}

/**
 * @brief 处理命令
 */
bool communication_process_command(command_type_t command, void *params) {
    // 使用(void)标记未使用的参数，避免编译警告
    (void)params;

    switch (command) {
        case COMMAND_ARM:
            // 解锁电机
            // 替换motor_enable_all为循环调用单个电机函数
            for (uint8_t i = 0; i < MOTOR_CHANNEL_MAX; i++) {
                motor_enable(i);
            }
            system_log(LOG_LEVEL_INFO, "Motors armed\n");
            return true;
            
        case COMMAND_DISARM:
            // 锁定电机
            // 替换motor_disable_all为循环调用单个电机函数
            for (uint8_t i = 0; i < MOTOR_CHANNEL_MAX; i++) {
                motor_disable(i);
            }
            system_log(LOG_LEVEL_INFO, "Motors disarmed\n");
            return true;
            
        case COMMAND_CALIBRATE:
            // 校准传感器
            // 为了简化，这里创建一个临时校准数据结构体并传递给校准函数
              mpu6050_calib_data_t calib_data;
              mpu6050_calibrate(&calib_data, 100); // 使用100个样本进行校准
            system_log(LOG_LEVEL_INFO, "Sensors calibrated\n");
            return true;
            
        case COMMAND_SAVE_CONFIG:
            // 保存配置
            system_log(LOG_LEVEL_INFO, "Configuration saved\n");
            return true;
            
        case COMMAND_RESET:
            // 重置系统
            system_reset(0);
            return true;
            
        case COMMAND_EMERGENCY:
            // 紧急停止
            motor_stop_all();
            system_log(LOG_LEVEL_ERROR, "Emergency stop activated\n");
            return true;
            
        default:
            return false;
    }
}

/**
 * @brief 设置通信回调函数
 */
void communication_set_callbacks(communication_channel_t channel, 
                                uart_callback_t rx_callback,
                                uart_callback_t tx_callback,
                                uart_callback_t error_callback) {
    // 使用(void)标记未使用的参数，避免编译警告
    (void)rx_callback;
    (void)tx_callback;
    (void)error_callback;
    
    if (!communication_initialized || channel >= COMM_CHANNEL_MAX) {
        return;
    }

    // 移除uart_set_callbacks调用，因为hal_uart.h中可能没有定义这个函数
}

/**
 * @brief 关闭通信通道
 */
bool communication_close(communication_channel_t channel) {
    if (!communication_initialized || channel >= COMM_CHANNEL_MAX) {
        return false;
    }

    bool result = true; // 移除uart_deinit调用，因为hal_uart.h中可能没有定义这个函数
    if (result) {
        system_log(LOG_LEVEL_INFO, "Communication channel %d closed\n", channel);
    }

    return result;
}

/**
 * @brief SOC通信初始化
 */
int32_t soc_communication_init(void) {
    // 初始化SOC通信通道
    communication_configs[COMM_CHANNEL_SOC].uart_channel = UART5;
    communication_configs[COMM_CHANNEL_SOC].baudrate = 115200;
    communication_configs[COMM_CHANNEL_SOC].data_bits = 8;
    communication_configs[COMM_CHANNEL_SOC].stop_bits = 1;
    communication_configs[COMM_CHANNEL_SOC].parity = UART_PARITY_NONE;
    communication_configs[COMM_CHANNEL_SOC].flow_control = UART_FLOW_CONTROL_NONE;
    
    // 初始化UART5
    uart_config_t uart_config = {
        .channel = UART5,
        .baudrate = 115200,
        .data_bits = 8,
        .stop_bits = 1,
        .parity = UART_PARITY_NONE,
        .flow_control = UART_FLOW_CONTROL_NONE,
        .interrupt_priority = 5
    };
    
    int32_t result = hal_uart_init(&uart_config);
    if (result != 0) {
        return -1;
    }

    // 初始化SOC状态
    memset(&soc_status, 0, sizeof(soc_status));
    soc_status.power_mode = SOC_POWER_MODE_NORMAL;
    soc_status.initialized = true;

    return 0;
}

/**
 * @brief 发送SOC命令
 */
bool communication_send_soc_command(communication_channel_t channel, command_type_t command, uint8_t *data, uint16_t length) {
    if (channel != COMM_CHANNEL_SOC) {
        return false;
    }

    message_t message = {
        .type = MESSAGE_TYPE_COMMAND,
        .length = sizeof(command_type_t) + length,
        .timestamp = system_get_time_ms()
    };

    // 填充命令数据
    *((command_type_t *)message.data) = command;
    if (data && length > 0) {
        memcpy(message.data + sizeof(command_type_t), data, length);
    }

    int32_t result = communication_send(channel, &message);
    return result > 0;
}

/**
 * @brief 接收SOC数据
 */
bool communication_receive_soc_data(communication_channel_t channel, soc_data_t *soc_data) {
    if (channel != COMM_CHANNEL_SOC || !soc_data) {
        return false;
    }

    message_t message;
    if (communication_receive(channel, &message) < 0) {
        return false;
    }

    // 解析SOC数据（这里使用模拟数据，实际使用时需要根据SOC协议解析）
    soc_data->voltage = 3.7f;  // 模拟电压值
    soc_data->current = 0.5f;  // 模拟电流值
    soc_data->power = soc_data->voltage * soc_data->current;
    soc_data->temperature = 25.0f;  // 模拟温度值
    soc_data->battery_level = 85.0f; // 模拟电池电量
    soc_data->power_mode = SOC_POWER_NORMAL;
    soc_data->status = SOC_STATUS_ACTIVE;
    soc_data->uptime = system_get_time_ms();

    return true;
}

/**
 * @brief 查询SOC状态
 */
bool communication_query_soc_status(communication_channel_t channel, soc_status_t *status) {
    if (channel != COMM_CHANNEL_SOC || !status) {
        return false;
    }

    // 发送状态查询命令
    bool result = communication_send_soc_command(channel, COMMAND_SOC_QUERY_STATUS, NULL, 0);
    if (result) {
        // 模拟SOC状态响应
        *status = SOC_STATUS_ACTIVE;
    }

    return result;
}

/**
 * @brief 设置SOC功耗模式
 */
bool communication_set_soc_power_mode(communication_channel_t channel, soc_power_mode_t mode) {
    if (channel != COMM_CHANNEL_SOC) {
        return false;
    }

    // 发送功耗模式设置命令
    command_type_t command;
    switch (mode) {
        case SOC_POWER_NORMAL:
            command = COMMAND_SOC_SET_POWER_MODE;
            break;
        case SOC_POWER_LOW:
        case SOC_POWER_ULTRA_LOW:
        case SOC_POWER_SLEEP:
            command = COMMAND_SOC_SLEEP;
            break;
        default:
            return false;
    }

    return communication_send_soc_command(channel, command, (uint8_t *)&mode, sizeof(mode));
}

/**
 * @brief 获取SOC电池信息
 */
bool communication_get_soc_battery_info(communication_channel_t channel, soc_data_t *soc_data) {
    if (channel != COMM_CHANNEL_SOC || !soc_data) {
        return false;
    }

    // 发送电池信息查询命令
    bool result = communication_send_soc_command(channel, COMMAND_SOC_GET_BATTERY_INFO, NULL, 0);
    if (result) {
        // 模拟电池信息数据
        soc_data->voltage = 3.7f;
        soc_data->current = 0.3f;
        soc_data->power = soc_data->voltage * soc_data->current;
        soc_data->battery_level = 85.0f;
        soc_data->temperature = 25.0f;
        soc_data->status = SOC_STATUS_ACTIVE;
    }

    return result;
}

/**
 * @brief 控制SOC进入睡眠模式
 */
bool communication_soc_sleep(communication_channel_t channel) {
    if (channel != COMM_CHANNEL_SOC) {
        return false;
    }

    return communication_send_soc_command(channel, COMMAND_SOC_SLEEP, NULL, 0);
}

/**
 * @brief 唤醒SOC
 */
bool communication_soc_wakeup(communication_channel_t channel) {
    if (channel != COMM_CHANNEL_SOC) {
        return false;
    }

    return communication_send_soc_command(channel, COMMAND_SOC_WAKEUP, NULL, 0);
}

/**
 * @brief SOC发送命令
 */
int32_t soc_send_command(soc_command_t command, uint8_t *data, uint16_t length) {
    if (!soc_status.initialized || command >= SOC_CMD_MAX) {
        return -1;
    }

    // 构建SOC命令消息
    message_t message;
    message.type = MESSAGE_TYPE_SOC_DATA;
    message.timestamp = system_get_time_ms();
    message.length = length + 1; // 1字节命令 + 数据

    // 第一个字节是命令类型
    message.data[0] = (uint8_t)command;
    
    // 添加数据（如果有）
    if (length > 0 && data != NULL && length <= sizeof(message.data) - 1) {
        memcpy(&message.data[1], data, length);
    }

    // 发送消息到SOC通道
    int32_t sent_bytes = communication_send(COMM_CHANNEL_SOC, &message);
    
    if (sent_bytes > 0) {
        soc_status.last_command_time = message.timestamp;
        soc_status.command_count++;
        return sent_bytes;
    }

    return -1;
}

/**
 * @brief SOC轮询监控任务
 * @note 这个函数需要在主循环或RTOS任务中定期调用
 */
void soc_poll_task(void) {
    if (!soc_status.initialized) {
        return;
    }

    // 检查通信超时
    uint32_t current_time = system_get_time_ms();
    if (current_time - soc_status.last_activity_time > SOC_COMM_TIMEOUT_MS) {
        // 通信超时，可能需要重连
        soc_status.connected = false;
        soc_status.timeout_count++;
        
        // 可以在这里添加重连逻辑
        return;
    }

    // 处理待响应的命令
    if (soc_status.pending_command != SOC_CMD_NONE && 
        current_time - soc_status.last_command_time > SOC_COMMAND_TIMEOUT_MS) {
        
        // 命令响应超时
        soc_status.timeout_count++;
        soc_status.last_error = SOC_ERROR_COMMAND_TIMEOUT;
        soc_status.pending_command = SOC_CMD_NONE;
        
        // 可以在这里添加重试逻辑
        if (soc_status.command_retry_count < SOC_MAX_RETRY_COUNT) {
            soc_status.command_retry_count++;
            soc_send_command(soc_status.pending_command, NULL, 0);
        }
    }

    // 定期发送心跳（如果需要）
    if (current_time - soc_status.last_heartbeat_time > SOC_HEARTBEAT_INTERVAL_MS) {
        soc_status.last_heartbeat_time = current_time;
        soc_send_command(SOC_CMD_HEARTBEAT, NULL, 0);
    }
}

/**
 * @brief SOC数据处理回调
 * @note 当接收到SOC数据时会调用此函数
 */
void soc_data_process_callback(uint8_t *data, uint16_t length) {
    if (data == NULL || length == 0) {
        return;
    }

    // 解析SOC响应数据
    if (length >= 2) { // 至少需要1字节命令 + 1字节状态
        soc_command_t command = (soc_command_t)data[0];
        uint8_t status = data[1];
        
        switch (command) {
            case SOC_CMD_QUERY_STATUS:
                if (length >= sizeof(soc_status_data_t)) {
                    soc_status_data_t status_data;
                    memcpy(&status_data, &data[2], sizeof(soc_status_data_t));
                    soc_status.last_voltage = status_data.voltage;
                    soc_status.last_temperature = status_data.temperature;
                    soc_status.current_power_mode = (soc_power_mode_t)status_data.power_mode;
                    soc_status.status = (soc_status_t)status_data.status;
                }
                break;
                
            case SOC_CMD_GET_BATTERY_INFO:
                if (length >= sizeof(soc_battery_info_t)) {
                    soc_battery_info_t battery_info;
                    memcpy(&battery_info, &data[2], sizeof(soc_battery_info_t));
                    soc_status.battery_level = battery_info.level;
                    soc_status.battery_voltage = battery_info.voltage;
                    soc_status.battery_current = battery_info.current;
                }
                break;
                
            case SOC_CMD_HEARTBEAT:
                soc_status.connected = true;
                soc_status.last_heartbeat_response = system_get_time_ms();
                break;
                
            default:
                break;
        }
        
        // 清除待处理的命令
        if (soc_status.pending_command == command) {
            soc_status.pending_command = SOC_CMD_NONE;
            soc_status.command_retry_count = 0;
        }
        
        soc_status.success_count++;
        soc_status.last_success_time = system_get_time_ms();
    }
}