/*
 * 四轴无人机系统通用数据类型定义
 */

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>

// 浮点类型别名
typedef float float32_t;

// 飞控系统常量定义
#define PI              3.14159265358979323846f
#define GRAVITY         9.80665f          // 重力加速度 (m/s^2)
#define DEG_TO_RAD      0.01745329251994329576f  // 角度转弧度
#define RAD_TO_DEG      57.2957795130823208767f  // 弧度转角度

// I2C通道定义
#define I2C_CHANNEL_1      0
#define I2C_CHANNEL_2      1
#define I2C_CHANNEL_3      2
#define I2C_CHANNEL_4      3
#define I2C_CHANNEL_MAX    4

// I2C速度定义
#define I2C_SPEED_100K     0   // 标准模式
#define I2C_SPEED_400K     1   // 快速模式
#define I2C_SPEED_1M       2   // 快速模式+
#define I2C_SPEED_3_4M     3   // 高速模式

// 基础数学类型定义
typedef struct {
    float x;
    float y;
    float z;
} vector3f_t;

typedef struct {
    float roll;
    float pitch;
    float yaw;
} euler_angle_t;

typedef struct {
    float w;
    float x;
    float y;
    float z;
} quaternion_t;

// PID控制器基础结构体 - flight_control.h需要使用
typedef struct {
    float kp;
    float ki;
    float kd;
    float error;
    float last_error;
    float integral;
    float derivative;
    float output;
    float integral_max;
    float output_max;
} pid_controller_t;

// 姿态数据结构体 - communication.h需要使用
typedef struct {
    float roll;
    float pitch;
    float yaw;
    float rates[3];
} attitude_data_t;

// 电机数据结构体 - communication.h需要使用
typedef struct {
    float outputs[4];
    bool armed;
} motor_data_t;

// UART回调函数类型 - communication.h需要使用
typedef void (*uart_callback_t)(uint8_t data);

// PID参数结构体 - flight_control.h需要使用
typedef struct {
    float kp;
    float ki;
    float kd;
    float integral_max;
    float output_max;
} pid_params_t;

// 错误信息结构体
typedef struct {
    uint8_t code;        // 错误代码
    const char *message; // 错误消息
    const char *file;    // 错误发生的文件
    uint32_t line;       // 错误发生的行号
} error_info_t;

// 日志级别枚举
typedef enum {
    LOG_LEVEL_DEBUG = 0,        // 调试信息
    LOG_LEVEL_INFO,             // 一般信息
    LOG_LEVEL_WARNING,          // 警告信息
    LOG_LEVEL_ERROR,            // 错误信息
    LOG_LEVEL_FATAL             // 致命错误
} log_level_t;

// 系统状态枚举
typedef enum {
    SYSTEM_STATE_INIT = 0,      // 系统初始化状态
    SYSTEM_STATE_CALIBRATING,   // 系统校准状态
    SYSTEM_STATE_READY,         // 系统准备就绪
    SYSTEM_STATE_FLYING,        // 飞行状态
    SYSTEM_STATE_ERROR          // 错误状态
} system_state_t;

// 飞行模式枚举
typedef enum {
    FLIGHT_MODE_MANUAL = 0,     // 手动模式
    FLIGHT_MODE_ATTITUDE,       // 姿态模式
    FLIGHT_MODE_GPS,            // GPS模式
    FLIGHT_MODE_HEADLESS        // 无头模式
} flight_mode_t;

// IMU校准类型枚举
typedef enum {
    IMU_CALIB_GYRO = 0,       // 陀螺仪校准
    IMU_CALIB_ACCEL,          // 加速度计校准
    IMU_CALIB_MAG,            // 磁力计校准
    IMU_CALIB_BOTH            // 全部校准(同时校准陀螺仪和加速度计)
} imu_calib_type_t;

// 控制通道枚举
typedef enum {
    CONTROL_CHANNEL_THROTTLE = 0,    // 油门通道
    CONTROL_CHANNEL_ROLL,            // 横滚通道
    CONTROL_CHANNEL_PITCH,           // 俯仰通道
    CONTROL_CHANNEL_YAW,             // 偏航通道
    CONTROL_CHANNEL_MODE,            // 模式通道
    CONTROL_CHANNEL_AUX1,            // 辅助通道1
    CONTROL_CHANNEL_AUX2,            // 辅助通道2
    CONTROL_CHANNEL_AUX3,            // 辅助通道3
    CONTROL_CHANNEL_AUX4,            // 辅助通道4
    CONTROL_CHANNEL_MAX              // 通道总数
} control_channel_t;

// 遥控器数据结构体
typedef struct {
    float throttle;             // 油门 (0.0-1.0)
    float roll;                 // 横滚 (-1.0-1.0)
    float pitch;                // 俯仰 (-1.0-1.0)
    float yaw;                  // 偏航 (-1.0-1.0)
    uint8_t mode_switch;        // 模式切换通道值
    uint8_t aux1;               // 辅助通道1
    uint8_t aux2;               // 辅助通道2
    bool valid;                 // 信号有效性标志
} rc_data_t;

// 控制通道结构体
typedef struct {
    float throttle;             // 油门 (0.0-1.0)
    float roll;                 // 横滚 (-1.0-1.0)
    float pitch;                // 俯仰 (-1.0-1.0)
    float yaw;                  // 偏航 (-1.0-1.0)
    uint8_t mode_switch;        // 模式切换通道值
    uint8_t aux1;               // 辅助通道1
    uint8_t aux2;               // 辅助通道2
    bool valid;                 // 信号有效性标志
} control_channels_t;

// 传感器数据结构体
typedef struct {
    // 陀螺仪数据 (rad/s)
    float gyro[3];              // [x, y, z]
    
    // 加速度计数据 (m/s^2)
    float accel[3];             // [x, y, z]
    
    // 气压计数据
    float pressure;             // 气压 (hPa)
    float temperature;          // 温度 (°C)
    
    // 磁力计数据 (uT)
    float mag[3];               // [x, y, z]
    
    // GPS数据
    float latitude;             // 纬度 (度)
    float longitude;            // 经度 (度)
    float altitude;             // 高度 (m)
    float groundspeed;          // 地速 (m/s)
    float heading;              // 航向角 (度)
    uint8_t num_satellites;     // 卫星数量
    bool gps_fix;               // GPS定位标志
    
    // 时间戳 (ms)
    uint32_t timestamp;
    
    // 数据有效性标志
    bool gyro_valid;
    bool accel_valid;
    bool pressure_valid;
    bool mag_valid;
    bool gps_valid;
} sensor_data_t;

// 姿态数据结构体
typedef struct {
    // 四元数表示姿态
    float quaternion[4];        // [w, x, y, z]
    
    // 欧拉角表示姿态 (度)
    float roll;                 // 横滚角
    float pitch;                // 俯仰角
    float yaw;                  // 偏航角
    
    // 角速度 (度/s)
    float rates[3];             // [x, y, z]
    
    // 时间戳 (ms)
    uint32_t timestamp;
} attitude_t;

// 位置数据结构体
typedef struct {
    // NED坐标系位置 (m)
    float position[3];          // [north, east, down]
    
    // 速度 (m/s)
    float velocity[3];          // [north, east, down]
    
    // 时间戳 (ms)
    uint32_t timestamp;
    
    // 数据有效性标志
    bool valid;
} position_t;

// 电机输出结构体
typedef struct {
    uint16_t motor[4];          // 4个电机PWM值 (1000-2000us)
    bool armed;                 // 电机解锁标志
} motor_output_t;

// 系统状态结构体
typedef struct {
    system_state_t state;       // 系统状态
    flight_mode_t flight_mode;  // 飞行模式
    
    // 电池信息
    float battery_voltage;      // 电池电压 (V)
    float battery_current;      // 电池电流 (A)
    float battery_percentage;   // 电池剩余电量百分比
    
    // 系统时间 (ms)
    uint32_t system_time;
    uint32_t uptime;            // 系统运行时间(ms)
    float cpu_usage;            // CPU使用率
    uint32_t free_heap;         // 空闲内存
    uint32_t used_heap;         // 已用堆内存
    uint32_t total_heap;        // 总堆内存
    float temperature;          // 温度
    uint32_t last_error;        // 最后一次错误
    uint32_t error_count;       // 错误计数
    
    // 错误标志
    uint16_t error_flags;
} system_status_t;

// 飞行状态结构体
typedef struct {
    flight_mode_t current_mode;         // 当前飞行模式
    bool armed;                         // 电机是否解锁
    bool failsafe_active;               // 故障保护是否激活
    uint32_t failsafe_start_time;       // 故障保护开始时间
    float rc_input[4];                  // 遥控器输入 [roll, pitch, yaw, throttle]
    bool stabilized;                    // 是否已稳定
    float motor_outputs[4];             // 电机输出值
    bool calibration_in_progress;       // 是否正在校准
    imu_calib_type_t current_calib_type; // 当前校准类型
} flight_status_t;

// 系统错误代码枚举
typedef enum {
    SYSTEM_ERROR_NONE = 0,              // 无错误
    SYSTEM_ERROR_INIT_FAILED,           // 初始化失败
    SYSTEM_ERROR_MEMORY_ALLOCATION,     // 内存分配错误
    SYSTEM_ERROR_TIMER_INIT_FAILED,     // 定时器初始化失败
    SYSTEM_ERROR_TASK_SCHEDULER_ERROR,  // 任务调度器错误
    SYSTEM_ERROR_SENSOR_ERROR,          // 传感器错误
    SYSTEM_ERROR_MOTOR_ERROR,           // 电机错误
    SYSTEM_ERROR_COMMUNICATION_ERROR    // 通信错误
} system_error_t;

#endif /* TYPES_H */