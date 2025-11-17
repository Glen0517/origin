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
    COMM_CHANNEL_SOC,        // SOC交互通道
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
    MESSAGE_TYPE_LOG,                   // 日志信息
    MESSAGE_TYPE_SOC_DATA,              // SOC数据交互
    MESSAGE_TYPE_SOC_STATUS,            // SOC状态查询
    MESSAGE_TYPE_SOC_CONFIG,            // SOC配置命令
    MESSAGE_TYPE_SOC_RESPONSE           // SOC响应消息
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
    COMMAND_EMERGENCY,                  // 紧急停止
    COMMAND_SOC_QUERY_STATUS,           // SOC状态查询
    COMMAND_SOC_SET_POWER_MODE,         // SOC设置功耗模式
    COMMAND_SOC_GET_VOLTAGE,            // SOC获取电压
    COMMAND_SOC_GET_TEMPERATURE,        // SOC获取温度
    COMMAND_SOC_GET_BATTERY_INFO,       // SOC获取电池信息
    COMMAND_SOC_SLEEP,                  // SOC进入低功耗模式
    COMMAND_SOC_WAKEUP                  // SOC唤醒
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

// SOC状态枚举
typedef enum {
    SOC_STATUS_ACTIVE,                  // SOC活跃状态
    SOC_STATUS_LOW_POWER,               // SOC低功耗状态
    SOC_STATUS_SLEEP,                   // SOC睡眠状态
    SOC_STATUS_ERROR,                   // SOC错误状态
    SOC_STATUS_UNKNOWN                  // SOC未知状态
} soc_status_t;

// SOC功耗模式枚举
typedef enum {
    SOC_POWER_NORMAL,                   // 正常功耗模式
    SOC_POWER_LOW,                      // 低功耗模式
    SOC_POWER_ULTRA_LOW,                // 超低功耗模式
    SOC_POWER_SLEEP                     // 睡眠模式
} soc_power_mode_t;

// SOC数据信息结构体
typedef struct {
    float voltage;                      // 电压(V)
    float current;                      // 电流(A)
    float power;                        // 功耗(W)
    float temperature;                  // 温度(°C)
    float battery_level;                // 电池电量(%)
    soc_power_mode_t power_mode;        // 功耗模式
    soc_status_t status;                // SOC状态
    uint32_t uptime;                    // 运行时间(ms)
} soc_data_t;

// SOC配置结构体
typedef struct {
    soc_power_mode_t power_mode;        // 功耗模式
    bool auto_sleep_enabled;            // 自动睡眠使能
    uint32_t sleep_threshold;           // 睡眠阈值(秒)
    bool temperature_protection;        // 温度保护使能
    float max_temperature;              // 最高工作温度
} soc_config_t;

// SOC状态数据结构体
typedef struct {
    float voltage;                      // 电压(V)
    float temperature;                  // 温度(°C)
    uint8_t power_mode;                 // 功耗模式
    uint8_t status;                     // 状态
} soc_status_data_t;

// SOC电池信息结构体
typedef struct {
    float level;                        // 电池电量(%)
    float voltage;                      // 电池电压(V)
    float current;                      // 电池电流(A)
    uint32_t capacity;                  // 电池容量(mAh)
    uint32_t remaining_capacity;        // 剩余容量(mAh)
} soc_battery_info_t;

// SOC通信状态管理结构体
typedef struct {
    bool initialized;                   // 初始化标志
    bool connected;                     // 连接状态
    bool data_available;                // 数据可用标志
    soc_command_t pending_command;      // 待处理命令
    uint32_t command_count;             // 命令发送计数
    uint32_t receive_count;             // 数据接收计数
    uint32_t error_count;               // 错误计数
    uint32_t timeout_count;             // 超时计数
    uint32_t success_count;             // 成功计数
    uint32_t last_activity_time;        // 最后活动时间
    uint32_t last_command_time;         // 最后命令时间
    uint32_t last_receive_time;         // 最后接收时间
    uint32_t last_heartbeat_time;       // 最后心跳时间
    uint32_t last_heartbeat_response;   // 最后心跳响应时间
    uint32_t last_success_time;         // 最后成功时间
    uint8_t command_retry_count;        // 命令重试计数
    soc_error_t last_error;             // 最后错误码
    soc_power_mode_t power_mode;        // 当前功耗模式
    soc_power_mode_t current_power_mode;// 当前功耗模式
    float last_voltage;                 // 最后电压值
    float last_temperature;             // 最后温度值
    float battery_level;                // 电池电量
    float battery_voltage;              // 电池电压
    float battery_current;              // 电池电流
} soc_comm_status_t;

// SOC错误类型枚举
typedef enum {
    SOC_ERROR_NONE = 0,                 // 无错误
    SOC_ERROR_INIT_FAILED,              // 初始化失败
    SOC_ERROR_SEND_FAILED,              // 发送失败
    SOC_ERROR_RECEIVE_FAILED,           // 接收失败
    SOC_ERROR_COMMAND_TIMEOUT,          // 命令超时
    SOC_ERROR_INVALID_COMMAND,          // 无效命令
    SOC_ERROR_CHECKSUM_ERROR,           // 校验和错误
    SOC_ERROR_BUFFER_OVERFLOW,          // 缓冲区溢出
    SOC_ERROR_UNKNOWN                   // 未知错误
} soc_error_t;

/**
 * @brief 初始化SOC通信通道
 * @param config SOC配置结构体
 * @return 是否初始化成功
 */
bool communication_init_soc(communication_config_t *config);

/**
 * @brief 发送SOC命令
 * @param channel SOC通信通道
 * @param command 命令类型
 * @param data 命令数据
 * @param length 数据长度
 * @return 是否发送成功
 */
bool communication_send_soc_command(communication_channel_t channel, command_type_t command, uint8_t *data, uint16_t length);

/**
 * @brief 接收SOC数据
 * @param channel SOC通信通道
 * @param soc_data SOC数据结构体
 * @return 是否接收成功
 */
bool communication_receive_soc_data(communication_channel_t channel, soc_data_t *soc_data);

/**
 * @brief 查询SOC状态
 * @param channel SOC通信通道
 * @param status SOC状态
 * @return 是否查询成功
 */
bool communication_query_soc_status(communication_channel_t channel, soc_status_t *status);

/**
 * @brief 设置SOC功耗模式
 * @param channel SOC通信通道
 * @param mode 功耗模式
 * @return 是否设置成功
 */
bool communication_set_soc_power_mode(communication_channel_t channel, soc_power_mode_t mode);

/**
 * @brief 获取SOC电池信息
 * @param channel SOC通信通道
 * @param soc_data SOC数据
 * @return 是否获取成功
 */
bool communication_get_soc_battery_info(communication_channel_t channel, soc_data_t *soc_data);

/**
 * @brief 控制SOC进入睡眠模式
 * @param channel SOC通信通道
 * @return 是否发送成功
 */
bool communication_soc_sleep(communication_channel_t channel);

/**
 * @brief 唤醒SOC
 * @param channel SOC通信通道
 * @return 是否发送成功
 */
bool communication_soc_wakeup(communication_channel_t channel);

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
 * @brief SOC通信状态管理结构体
 * @note 这个变量在communication.c中定义并使用
 */
extern soc_comm_status_t soc_status;

// SOC监控任务函数声明
/**
 * @brief SOC轮询监控任务
 * @note 这个函数需要在主循环或RTOS任务中定期调用
 */
void soc_poll_task(void);

/**
 * @brief SOC数据处理回调
 * @note 当接收到SOC数据时会调用此函数
 */
void soc_data_process_callback(uint8_t *data, uint16_t length);

// 高级SOC功能函数声明
/**
 * @brief SOC通信初始化
 * @return 初始化结果
 */
int32_t soc_communication_init(void);

/**
 * @brief SOC发送命令
 * @param command 命令类型
 * @param data 命令数据
 * @param length 数据长度
 * @return 发送字节数
 */
int32_t soc_send_command(soc_command_t command, uint8_t *data, uint16_t length);

/**
 * @brief SOC接收数据
 * @param buffer 接收缓冲区
 * @param length 缓冲区长度
 * @return 接收字节数
 */
int32_t soc_receive_data(uint8_t *buffer, uint16_t length);

/**
 * @brief 查询SOC状态
 * @return SOC状态结构体
 */
soc_status_t soc_query_status(void);

/**
 * @brief 设置SOC功耗模式
 * @param mode 功耗模式
 * @return 设置结果
 */
bool soc_set_power_mode(soc_power_mode_t mode);

/**
 * @brief 获取SOC电池信息
 * @param info 电池信息结构体
 * @return 获取结果
 */
bool soc_get_battery_info(soc_battery_info_t *info);

/**
 * @brief 控制SOC进入睡眠模式
 * @return 操作结果
 */
bool soc_sleep(void);

/**
 * @brief 唤醒SOC
 * @return 操作结果
 */
bool soc_wakeup(void);

/**
 * @brief 关闭通信通道
 * @param channel 通信通道
 * @return 是否关闭成功
 */
bool communication_close(communication_channel_t channel);

#endif // COMMUNICATION_H