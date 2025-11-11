/*
 * 通信模块接口定义
 */

#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include "../include/types.h"
#include "../config/config.h"
#include "../hal/hal_uart.h"

// 通信协议类型
typedef enum {
    PROTOCOL_MAVLINK,        // MAVLink协议
    PROTOCOL_SBUS,           // SBUS协议
    PROTOCOL_PPM,            // PPM协议
    PROTOCOL_SERIAL,         // 自定义串行协议
    PROTOCOL_UDP,            // UDP协议
    PROTOCOL_TCP             // TCP协议
} communication_protocol_t;

// 通信通道类型
typedef enum {
    COMM_CHANNEL_RC,         // 遥控器通道
    COMM_CHANNEL_GCS,        // 地面站通道
    COMM_CHANNEL_DEBUG,      // 调试通道
    COMM_CHANNEL_LOGGING,    // 日志通道
    COMM_CHANNEL_MAX         // 最大通道数
} communication_channel_t;

// 通信配置结构体
typedef struct {
    communication_channel_t channel;    // 通信通道
    communication_protocol_t protocol;  // 通信协议
    uart_channel_t uart_channel;        // UART通道
    uint32_t baud_rate;                 // 波特率
    uint16_t buffer_size;               // 缓冲区大小
    bool parity_enabled;                // 是否启用校验
    uint8_t data_bits;                  // 数据位
    uint8_t stop_bits;                  // 停止位
} communication_config_t;

// 通信状态结构体
typedef struct {
    communication_channel_t channel;    // 通信通道
    communication_protocol_t protocol;  // 通信协议
    uint32_t baud_rate;                 // 当前波特率
    uint32_t rx_count;                  // 接收字节数
    uint32_t tx_count;                  // 发送字节数
    uint32_t error_count;               // 错误计数
    uint32_t last_activity_time;        // 最后活动时间
    bool connected;                     // 是否连接
    bool data_available;                // 是否有数据可用
} communication_status_t;

// 消息类型
typedef enum {
    MESSAGE_TYPE_HEARTBEAT,             // 心跳包
    MESSAGE_TYPE_ATTITUDE,              // 姿态数据
    MESSAGE_TYPE_RC,                    // 遥控器数据
    MESSAGE_TYPE_MOTOR,                 // 电机数据
    MESSAGE_TYPE_PID,                   // PID参数
    MESSAGE_TYPE_CONFIG,                // 配置参数
    MESSAGE_TYPE_COMMAND,               // 命令
    MESSAGE_TYPE_STATUS,                // 状态报告
    MESSAGE_TYPE_ERROR,                 // 错误报告
    MESSAGE_TYPE_LOG                    // 日志信息
} message_type_t;

// 消息结构体
typedef struct {
    message_type_t type;                // 消息类型
    uint8_t data[256];                  // 消息数据
    uint16_t length;                    // 数据长度
    uint32_t timestamp;                 // 时间戳
} message_t;

// 命令类型
typedef enum {
    COMMAND_ARM,                        // 解锁电机
    COMMAND_DISARM,                     // 锁定电机
    COMMAND_CALIBRATE,                  // 校准传感器
    COMMAND_SET_MODE,                   // 设置飞行模式
    COMMAND_SET_PID,                    // 设置PID参数
    COMMAND_SAVE_CONFIG,                // 保存配置
    COMMAND_LOAD_CONFIG,                // 加载配置
    COMMAND_RESET,                      // 重置系统
    COMMAND_EMERGENCY                   // 紧急停止
} command_type_t;

/**
 * @brief 初始化通信模块
 * @param config 通信配置结构体
 * @return 是否初始化成功
 */
bool communication_init(communication_config_t *config);

/**
 * @brief 初始化所有通信通道
 * @return 是否全部初始化成功
 */
bool communication_init_all(void);

/**
 * @brief 发送消息
 * @param channel 通信通道
 * @param message 消息结构体
 * @return 发送的字节数
 */
int32_t communication_send(communication_channel_t channel, message_t *message);

/**
 * @brief 接收消息
 * @param channel 通信通道
 * @param message 消息结构体
 * @return 接收的字节数
 */
int32_t communication_receive(communication_channel_t channel, message_t *message);

/**
 * @brief 处理接收到的消息
 * @param channel 通信通道
 * @param message 消息结构体
 */
void communication_process_message(communication_channel_t channel, message_t *message);

/**
 * @brief 获取通信状态
 * @param channel 通信通道
 * @param status 通信状态结构体
 * @return 是否获取成功
 */
bool communication_get_status(communication_channel_t channel, communication_status_t *status);

/**
 * @brief 检查通信连接
 * @param channel 通信通道
 * @return 是否已连接
 */
bool communication_is_connected(communication_channel_t channel);

/**
 * @brief 检查是否有数据可用
 * @param channel 通信通道
 * @return 是否有数据可用
 */
bool communication_has_data(communication_channel_t channel);

/**
 * @brief 发送心跳包
 * @param channel 通信通道
 * @return 是否发送成功
 */
bool communication_send_heartbeat(communication_channel_t channel);

/**
 * @brief 发送姿态数据
 * @param channel 通信通道
 * @param attitude 姿态数据
 * @return 是否发送成功
 */
bool communication_send_attitude(communication_channel_t channel, attitude_data_t *attitude);

/**
 * @brief 发送电机数据
 * @param channel 通信通道
 * @param motor 电机数据
 * @return 是否发送成功
 */
bool communication_send_motor(communication_channel_t channel, motor_data_t *motor);

/**
 * @brief 发送系统状态
 * @param channel 通信通道
 * @param status 系统状态
 * @return 是否发送成功
 */
bool communication_send_status(communication_channel_t channel, system_status_t *status);

/**
 * @brief 发送错误报告
 * @param channel 通信通道
 * @param error 错误代码
 * @param message 错误消息
 * @return 是否发送成功
 */
bool communication_send_error(communication_channel_t channel, uint8_t error, const char *message);

/**
 * @brief 发送日志消息
 * @param channel 通信通道
 * @param level 日志级别
 * @param message 日志消息
 * @return 是否发送成功
 */
bool communication_send_log(communication_channel_t channel, log_level_t level, const char *message);

/**
 * @brief 处理命令
 * @param command 命令类型
 * @param params 命令参数
 * @return 命令执行结果
 */
bool communication_process_command(command_type_t command, void *params);

/**
 * @brief 设置通信回调函数
 * @param channel 通信通道
 * @param rx_callback 接收回调函数
 * @param tx_callback 发送回调函数
 * @param error_callback 错误回调函数
 */
void communication_set_callbacks(communication_channel_t channel, 
                                uart_callback_t rx_callback,
                                uart_callback_t tx_callback,
                                uart_callback_t error_callback);

/**
 * @brief 关闭通信通道
 * @param channel 通信通道
 * @return 是否关闭成功
 */
bool communication_close(communication_channel_t channel);

#endif // COMMUNICATION_H