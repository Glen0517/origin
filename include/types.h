/**
 * @file types.h
 * @brief 四轴无人机系统通用数据类型定义
 * @details 该文件定义了系统通用的数据类型、常量和结构体，
 *          提供平台无关的抽象接口，增强代码的可移植性和可维护性
 * @author 系统开发团队
 * @version 2.0.0
 * @date 2024-11-10
 */

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>

// ========================== 基础类型别名 ==========================

// 标准浮点类型别名 - 提高代码可移植性
typedef float       float32_t;           ///< 32位浮点数
typedef double      float64_t;           ///< 64位浮点数
typedef int8_t      int8;                ///< 8位有符号整数
typedef uint8_t     uint8;               ///< 8位无符号整数
typedef int16_t     int16;               ///< 16位有符号整数
typedef uint16_t    uint16;              ///< 16位无符号整数
typedef int32_t     int32;               ///< 32位有符号整数
typedef uint32_t    uint32;              ///< 32位无符号整数
typedef int64_t     int64;               ///< 64位有符号整数
typedef uint64_t    uint64;              ///< 64位无符号整数

// ========================== 数学常量定义 ==========================

// 数学常数 - 统一管理，避免重复定义
#ifndef PI
    #define PI              3.14159265358979323846f   ///< 圆周率
#endif
#ifndef GRAVITY
    #define GRAVITY         9.80665f                  ///< 重力加速度 (m/s²)
#endif
#ifndef DEG_TO_RAD
    #define DEG_TO_RAD      0.01745329251994329576f   ///< 角度转弧度系数
#endif
#ifndef RAD_TO_DEG
    #define RAD_TO_DEG      57.2957795130823208767f   ///< 弧度转角度系数
#endif

// ========================== 通用状态枚举 ==========================

/// 系统状态枚举 - 描述系统运行状态
typedef enum {
    SYSTEM_STATE_INIT = 0,       ///< 系统初始化状态
    SYSTEM_STATE_CALIBRATING,    ///< 系统校准状态  
    SYSTEM_STATE_READY,          ///< 系统准备就绪
    SYSTEM_STATE_FLYING,         ///< 飞行状态
    SYSTEM_STATE_ERROR           ///< 错误状态
} system_state_t;

/// 飞行模式枚举 - 定义不同飞行控制模式
typedef enum {
    FLIGHT_MODE_MANUAL = 0,      ///< 手动模式
    FLIGHT_MODE_ATTITUDE,        ///< 姿态模式
    FLIGHT_MODE_GPS,             ///< GPS模式
    FLIGHT_MODE_HEADLESS         ///< 无头模式
} flight_mode_t;

/// 日志级别枚举 - 定义日志输出级别
typedef enum {
    LOG_LEVEL_DEBUG = 0,         ///< 调试信息
    LOG_LEVEL_INFO,              ///< 一般信息
    LOG_LEVEL_WARNING,           ///< 警告信息
    LOG_LEVEL_ERROR,             ///< 错误信息
    LOG_LEVEL_FATAL              ///< 致命错误
} log_level_t;

// ========================== 基础数学类型定义 ==========================

/// 三维向量结构体 - 用于表示3D空间中的向量
typedef struct {
    float x;                     ///< X轴分量
    float y;                     ///< Y轴分量  
    float z;                     ///< Z轴分量
} vector3f_t;

/// 欧拉角结构体 - 表示飞行器姿态（单位：度）
typedef struct {
    float roll;                  ///< 横滚角 (°)
    float pitch;                 ///< 俯仰角 (°)
    float yaw;                   ///< 偏航角 (°)
} euler_angle_t;

/// 四元数结构体 - 表示飞行器姿态（无奇异性）
typedef struct {
    float w;                     ///< 标量分量
    float x;                     ///< 向量X分量
    float y;                     ///< 向量Y分量
    float z;                     ///< 向量Z分量
} quaternion_t;

// ========================== 宏辅助函数 ==========================

/// 向量操作辅助宏 - 提高代码可读性
#define VECTOR3_SET(v, x_val, y_val, z_val)  \
    do {                                     \
        (v)->x = (x_val);                    \
        (v)->y = (y_val);                    \
        (v)->z = (z_val);                    \
    } while(0)

#define VECTOR3_COPY(dest, src)              \
    do {                                     \
        (dest)->x = (src)->x;                \
        (dest)->y = (src)->y;                \
        (dest)->z = (src)->z;                \
    } while(0)

#define QUATERNION_SET(q, w_val, x_val, y_val, z_val) \
    do {                                            \
        (q)->w = (w_val);                           \
        (q)->x = (x_val);                           \
        (q)->y = (y_val);                           \
        (q)->z = (z_val);                           \
    } while(0)

// ========================== 控制器类型定义 ==========================

/// PID控制器配置结构体 - 仅包含参数配置
typedef struct {
    float kp;                     ///< 比例增益
    float ki;                     ///< 积分增益
    float kd;                     ///< 微分增益
    float integral_max;           ///< 积分项最大值（抗饱和）
    float output_max;             ///< 输出最大值（限幅）
} pid_config_t;

/// PID控制器状态结构体 - 包含运行时状态
typedef struct {
    float error;                  ///< 当前误差
    float last_error;             ///< 上次误差
    float integral;               ///< 积分累积值
    float derivative;             ///< 微分项
    float output;                 ///< 当前输出
    uint32_t update_count;        ///< 更新计数
} pid_state_t;

/// PID控制器完整结构体 - 配置+状态组合
typedef struct {
    pid_config_t config;          ///< PID配置参数
    pid_state_t state;            ///< PID运行状态
} pid_controller_t;

/// 控制器输出结构体 - 统一控制器输出接口
typedef struct {
    float output;                 ///< 控制器输出值
    float error;                  ///< 当前误差
    bool limited;                 ///< 是否达到限幅
    uint32_t timestamp;           ///< 时间戳
} control_output_t;

// ========================== 错误处理类型定义 ==========================

/// 错误严重级别枚举
typedef enum {
    ERROR_LEVEL_INFO = 0,         ///< 信息级别
    ERROR_LEVEL_WARNING,          ///< 警告级别
    ERROR_LEVEL_ERROR,            ///< 错误级别
    ERROR_LEVEL_CRITICAL          ///< 严重错误级别
} error_level_t;

/// 错误信息结构体 - 统一错误报告格式
typedef struct {
    uint32_t code;                ///< 错误代码
    error_level_t level;          ///< 错误级别
    const char *module;           ///< 发生错误的模块
    const char *message;          ///< 错误消息
    const char *file;             ///< 错误发生的文件
    uint32_t line;                ///< 错误发生的行号
    uint32_t timestamp;           ///< 错误发生时间
} error_info_t;

/// 错误统计结构体 - 用于错误监控和分析
typedef struct {
    uint32_t total_errors;        ///< 总错误数
    uint32_t error_by_level[4];   ///< 按级别分类的错误数
    uint32_t last_error_time;     ///< 最后一次错误时间
    const char *last_error_module;///< 最后一次错误的模块
} error_statistics_t;

// 日志级别枚举
// 注意：上述枚举类型已在文件开头统一定义
typedef enum {
    LOG_LEVEL_DEBUG = 0,        // 调试信息
    LOG_LEVEL_INFO,             // 一般信息
    LOG_LEVEL_WARNING,          // 警告信息
    LOG_LEVEL_ERROR,            // 错误信息
    LOG_LEVEL_FATAL             // 致命错误
} log_level_t;

// ========================== 校准和通道类型定义 ==========================

/// IMU校准类型枚举
typedef enum {
    IMU_CALIB_GYRO = 0,         ///< 陀螺仪校准
    IMU_CALIB_ACCEL,            ///< 加速度计校准
    IMU_CALIB_MAG,              ///< 磁力计校准
    IMU_CALIB_BOTH              ///< 全部校准（同时校准陀螺仪和加速度计）
} imu_calib_type_t;

/// 控制通道枚举 - 定义遥控器通道
typedef enum {
    CONTROL_CHANNEL_THROTTLE = 0,   ///< 油门通道
    CONTROL_CHANNEL_ROLL,           ///< 横滚通道
    CONTROL_CHANNEL_PITCH,          ///< 俯仰通道
    CONTROL_CHANNEL_YAW,            ///< 偏航通道
    CONTROL_CHANNEL_MODE,           ///< 模式通道
    CONTROL_CHANNEL_AUX1,           ///< 辅助通道1
    CONTROL_CHANNEL_AUX2,           ///< 辅助通道2
    CONTROL_CHANNEL_AUX3,           ///< 辅助通道3
    CONTROL_CHANNEL_AUX4,           ///< 辅助通道4
    CONTROL_CHANNEL_MAX             ///< 通道总数
} control_channel_t;

/// 通道数据联合体 - 支持不同数据类型
typedef union {
    float raw[8];               ///< 原始浮点数据
    struct {
        float throttle;         ///< 油门值 (0.0-1.0)
        float roll;             ///< 横滚值 (-1.0-1.0)
        float pitch;            ///< 俯仰值 (-1.0-1.0)
        float yaw;              ///< 偏航值 (-1.0-1.0)
        float aux1;             ///< 辅助通道1
        float aux2;             ///< 辅助通道2
        float aux3;             ///< 辅助通道3
        float aux4;             ///< 辅助通道4
    } scaled;                   ///< 缩放后的数据
    struct {
        uint16_t throttle;      ///< 油门原始值 (PWM)
        uint16_t roll;          ///< 横滚原始值 (PWM)
        uint16_t pitch;         ///< 俯仰原始值 (PWM)
        uint16_t yaw;           ///< 偏航原始值 (PWM)
        uint16_t mode;          ///< 模式通道值
        uint16_t aux1;          ///< 辅助通道1
        uint16_t aux2;          ///< 辅助通道2
        uint16_t aux3;          ///< 辅助通道3
    } pwm;                      ///< PWM原始数据
} control_data_t;

/// 遥控器数据结构体 - 统一遥控器数据接口
typedef struct {
    control_data_t data;        ///< 通道数据
    bool valid;                 ///< 数据有效性标志
    uint32_t timestamp;         ///< 数据时间戳
    uint8_t link_quality;       ///< 遥控信号质量 (0-100%)
    bool failsafe_active;       ///< 失控保护是否激活
} rc_data_t;

/// 电机输出结构体 - 统一电机控制接口
typedef struct {
    float outputs[4];           ///< 四个电机输出值 (0.0-1.0)
    uint16_t pwm_outputs[4];    ///< PWM输出值 (1000-2000us)
    bool armed;                 ///< 电机解锁标志
    uint32_t timestamp;         ///< 更新时间戳
} motor_output_t;

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

// ========================== 电源管理数据类型 ==========================

// 电池数据类型
typedef struct {
    float voltage;        // 电压 (V)
    float current;        // 电流 (A) 
    float temperature;    // 温度 (°C)
    float remaining;      // 剩余电量 (%)
    uint32_t capacity;    // 电池容量 (mAh)
    bool low_power;       // 低电量标志
} battery_data_t;

// 电源状态枚举
typedef enum {
    POWER_STATE_ON,       // 电源开启
    POWER_STATE_STANDBY,  // 待机状态
    POWER_STATE_SLEEP,    // 睡眠状态
    POWER_STATE_OFF,      // 电源关闭
    POWER_STATE_ERROR     // 电源错误
} power_state_t;

// 电源模式枚举
typedef enum {
    POWER_MODE_NORMAL,    // 正常模式
    POWER_MODE_ECO,       // 经济模式
    POWER_MODE_LOW,       // 低功耗模式
    POWER_MODE_SLEEP,     // 睡眠模式
    POWER_MODE_BOOST      // 增强模式
} power_mode_t;

// 电源管理配置结构体
typedef struct {
    power_mode_t mode;                // 当前电源模式
    float voltage_threshold;          // 电压阈值 (V)
    float current_limit;              // 电流限制 (A)
    float temperature_limit;          // 温度限制 (°C)
    uint32_t sleep_timeout;           // 睡眠超时时间 (ms)
    bool auto_power_off;              // 自动关机使能
    bool low_power_alert;             // 低电量警报使能
    float low_power_threshold;        // 低电量阈值 (%)
} power_config_t;

// 电源状态数据结构体
typedef struct {
    power_state_t state;              // 电源状态
    float input_voltage;              // 输入电压 (V)
    float output_voltage;             // 输出电压 (V)
    float input_current;              // 输入电流 (A)
    float output_current;             // 输出电流 (A)
    float power_consumption;          // 功耗 (W)
    float efficiency;                 // 电源效率 (%)
    float temperature;                // 温度 (°C)
    uint32_t uptime;                  // 运行时间 (ms)
    uint32_t sleep_count;             // 睡眠次数
    bool charging;                    // 充电状态
    bool low_power_warning;           // 低电量警告
} power_status_t;

// 电池管理系统结构体
typedef struct {
    battery_data_t main_battery;      // 主电池
    battery_data_t backup_battery;    // 备用电池
    bool battery_ok;                  // 电池状态OK
    bool charging_complete;           // 充电完成
    float battery_health;             // 电池健康度 (%)
    uint32_t charge_cycles;           // 充电循环次数
    float estimated_flight_time;      // 预计飞行时间 (分钟)
} battery_management_t;

// 电源保护结构体
typedef struct {
    bool over_voltage_protection;     // 过压保护
    bool under_voltage_protection;    // 欠压保护
    bool over_current_protection;     // 过流保护
    bool over_temperature_protection; // 过温保护
    bool short_circuit_protection;    // 短路保护
    bool reverse_polarity_protection; // 反接保护
} power_protection_t;

// ========================== 扩展算法数据类型 ==========================

// 传感器融合状态
typedef struct {
    vector3f_t gyro_bias;             // 陀螺仪偏置
    vector3f_t accel_bias;            // 加速度计偏置
    vector3f_t mag_bias;              // 磁力计偏置
    float alignment_matrix[3][3];     // 对齐矩阵
    float temperature_compensation;   // 温度补偿
    bool calibrated;                  // 校准状态
} sensor_fusion_state_t;

// 滤波器配置结构体
typedef struct {
    float lowpass_cutoff_freq;        // 低通滤波器截止频率 (Hz)
    float highpass_cutoff_freq;       // 高通滤波器截止频率 (Hz)
    float kalman_process_noise;       // 卡尔曼滤波过程噪声
    float kalman_measurement_noise;   // 卡尔曼滤波测量噪声
    float complementary_alpha;        // 互补滤波系数
} filter_config_t;

// 控制器状态结构体
typedef struct {
    float position_error[3];          // 位置误差 [x, y, z]
    float velocity_error[3];          // 速度误差 [vx, vy, vz]
    float attitude_error[3];          // 姿态误差 [roll, pitch, yaw]
    float angular_velocity_error[3];  // 角速度误差
    float control_output[4];          // 控制输出 [throttle, roll, pitch, yaw]
    uint32_t control_loop_count;      // 控制循环计数
    bool stability_achieved;          // 稳定性达成标志
} control_state_t;

// 故障严重程度枚举
typedef enum {
    FAULT_SEVERITY_LOW,               // 低严重程度
    FAULT_SEVERITY_MEDIUM,            // 中严重程度
    FAULT_SEVERITY_HIGH,              // 高严重程度
    FAULT_SEVERITY_CRITICAL           // 关键严重程度
} fault_severity_t;

// 故障检测结果
typedef struct {
    bool sensor_failure[3];           // 传感器故障 [gyro, accel, mag]
    bool motor_failure[4];            // 电机故障
    bool communication_failure;       // 通信故障
    bool battery_failure;             // 电池故障
    float failure_confidence;         // 故障置信度
    fault_severity_t severity;        // 故障严重程度
} fault_detection_t;

// 性能监控结构体
typedef struct {
    float cpu_usage;                  // CPU使用率 (%)
    float memory_usage;               // 内存使用率 (%)
    float algorithm_latency;          // 算法延迟 (ms)
    float control_loop_time;          // 控制循环时间 (ms)
    uint32_t packet_loss_count;       // 数据包丢失计数
    float jitter;                     // 时钟抖动 (us)
} performance_monitor_t;

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