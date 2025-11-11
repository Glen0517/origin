/*
 * 四轴无人机系统配置参数
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "../include/types.h"

// ========================== 系统配置 ==========================

// 系统时钟频率 (Hz)
#define SYSTEM_CLOCK_FREQ         72000000U

// 任务调度频率 (Hz)
#define SCHEDULER_FREQ            1000U

// 任务优先级定义
// 任务优先级已在service/system.h中定义为枚举类型task_priority_t

// 故障保护配置
#define FAILSAFE_LANDING_DELAY_MS   5000    // 故障保护着陆延迟时间(毫秒)

// ========================== 传感器配置 ==========================

// 传感器采样频率 (Hz)
#define GYRO_SAMPLE_FREQ          200U
#define ACCEL_SAMPLE_FREQ         200U
#define BARO_SAMPLE_FREQ          50U
#define MAG_SAMPLE_FREQ           50U
#define GPS_SAMPLE_FREQ           10U

// 陀螺仪配置
#define GYRO_RANGE                2000.0f    // 陀螺仪量程 (°/s)
#define GYRO_FILTER_CUTOFF        20.0f      // 低通滤波器截止频率 (Hz)

// 加速度计配置
#define ACCEL_RANGE               8.0f       // 加速度计量程 (g)
#define ACCEL_FILTER_CUTOFF       20.0f      // 低通滤波器截止频率 (Hz)

// 磁力计配置
#define MAG_RANGE                 4.8f       // 磁力计量程 (Ga)

// 气压计配置
#define BARO_FILTER_ALPHA         0.05f      // 气压数据低通滤波系数

// GPS配置
#define GPS_MIN_SATELLITES        6          // 最小有效卫星数量
#define GPS_MAX_HDOP              2.0f       // 最大水平精度因子

// ========================== 控制参数配置 ==========================

// 姿态控制参数 (角速度环)
#define RATE_PID_ROLL_KP          0.08f
#define RATE_PID_ROLL_KI          0.2f
#define RATE_PID_ROLL_KD          0.001f

#define RATE_PID_PITCH_KP         0.08f
#define RATE_PID_PITCH_KI         0.2f
#define RATE_PID_PITCH_KD         0.001f

#define RATE_PID_YAW_KP           0.05f
#define RATE_PID_YAW_KI           0.1f
#define RATE_PID_YAW_KD           0.0f

// 姿态控制参数 (角度环)
#define ATTITUDE_PID_ROLL_KP      4.0f
#define ATTITUDE_PID_ROLL_KI      0.0f
#define ATTITUDE_PID_ROLL_KD      0.0f

#define ATTITUDE_PID_PITCH_KP     4.0f
#define ATTITUDE_PID_PITCH_KI     0.0f
#define ATTITUDE_PID_PITCH_KD     0.0f

// 位置控制参数
#define POSITION_PID_X_KP         0.5f
#define POSITION_PID_X_KI         0.0f
#define POSITION_PID_X_KD         0.1f

#define POSITION_PID_Y_KP         0.5f
#define POSITION_PID_Y_KI         0.0f
#define POSITION_PID_Y_KD         0.1f

#define POSITION_PID_Z_KP         2.0f
#define POSITION_PID_Z_KI         0.5f
#define POSITION_PID_Z_KD         0.1f

// 速度控制参数
#define VELOCITY_PID_X_KP         2.0f
#define VELOCITY_PID_X_KI         0.1f
#define VELOCITY_PID_X_KD         0.0f

#define VELOCITY_PID_Y_KP         2.0f
#define VELOCITY_PID_Y_KI         0.1f
#define VELOCITY_PID_Y_KD         0.0f

#define VELOCITY_PID_Z_KP         1.0f
#define VELOCITY_PID_Z_KI         0.2f
#define VELOCITY_PID_Z_KD         0.0f

// 电机输出限制
#define MOTOR_MIN_PWM             1000U      // 最小PWM值 (us)
#define MOTOR_MAX_PWM             2000U      // 最大PWM值 (us)
#define MOTOR_IDLE_PWM            1100U      // 怠速PWM值 (us)
#define MOTOR_PWM_FREQUENCY       500U       // 电机PWM频率 (Hz)

// 控制输出限幅
#define MAX_ROLL_ANGLE            45.0f      // 最大横滚角 (度)
#define MAX_PITCH_ANGLE           45.0f      // 最大俯仰角 (度)
#define MAX_YAW_RATE              180.0f     // 最大偏航角速度 (度/s)
#define MAX_VERTICAL_VELOCITY     3.0f       // 最大垂直速度 (m/s)

// ========================== 安全配置 ==========================

// 电池电压报警阈值 (V)
#define BATTERY_WARNING_THRESHOLD 10.5f      // 警告阈值
#define BATTERY_CRITICAL_THRESHOLD 9.9f      // 严重阈值
#define BATTERY_LOW_THRESHOLD     9.6f       // 低电压阈值 (触发降落)

// 失控保护设置
#define FAILSAFE_DETECT_TIMEOUT   500U       // 失控检测超时时间 (ms)
#define FAILSAFE_ACTION           1          // 1: 降落, 2: 返航

// 地理围栏设置
#define GEOFENCE_RADIUS           50.0f      // 围栏半径 (m)
#define GEOFENCE_MAX_ALTITUDE     100.0f     // 最大飞行高度 (m)

// ========================== 通信配置 ==========================

// 串口配置
#define UART_BAUDRATE             115200U    // 串口波特率
#define UART_TX_BUFFER_SIZE       128U       // 发送缓冲区大小
#define UART_RX_BUFFER_SIZE       128U       // 接收缓冲区大小

// 地面站通信
#define MAVLINK_SEND_INTERVAL     100U       // Mavlink消息发送间隔 (ms)

// ========================== 调试配置 ==========================

// 日志级别
#define LOG_LEVEL_DEBUG           0
#define LOG_LEVEL_INFO            1
#define LOG_LEVEL_WARNING         2
#define LOG_LEVEL_ERROR           3
#define LOG_LEVEL_FATAL           4

#define CURRENT_LOG_LEVEL         LOG_LEVEL_INFO

// 调试输出配置
#define DEBUG_ENABLED             1          // 调试输出使能
#define DEBUG_UART_ENABLED        1          // 串口调试输出使能

#endif /* CONFIG_H */