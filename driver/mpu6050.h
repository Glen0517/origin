/*
 * MPU6050陀螺仪/加速度计驱动接口定义
 */

#ifndef MPU6050_H
#define MPU6050_H

#include "../include/types.h"
#include "../hal/hal_i2c.h"

// MPU6050 I2C地址定义
#define MPU6050_ADDR_AD0_LOW      0x68    // AD0引脚接低电平
#define MPU6050_ADDR_AD0_HIGH     0x69    // AD0引脚接高电平

// MPU6050寄存器定义
#define MPU6050_REG_SELF_TEST_X    0x0D
#define MPU6050_REG_SELF_TEST_Y    0x0E
#define MPU6050_REG_SELF_TEST_Z    0x0F
#define MPU6050_REG_SELF_TEST_A    0x10
#define MPU6050_REG_SMPLRT_DIV     0x19
#define MPU6050_REG_CONFIG         0x1A
#define MPU6050_REG_GYRO_CONFIG    0x1B
#define MPU6050_REG_ACCEL_CONFIG   0x1C
#define MPU6050_REG_MOTION_THRESH  0x1F
#define MPU6050_REG_INT_PIN_CFG    0x37
#define MPU6050_REG_INT_ENABLE     0x38
#define MPU6050_REG_INT_STATUS     0x3A
#define MPU6050_REG_ACCEL_XOUT_H   0x3B
#define MPU6050_REG_ACCEL_XOUT_L   0x3C
#define MPU6050_REG_ACCEL_YOUT_H   0x3D
#define MPU6050_REG_ACCEL_YOUT_L   0x3E
#define MPU6050_REG_ACCEL_ZOUT_H   0x3F
#define MPU6050_REG_ACCEL_ZOUT_L   0x40
#define MPU6050_REG_TEMP_OUT_H     0x41
#define MPU6050_REG_TEMP_OUT_L     0x42
#define MPU6050_REG_GYRO_XOUT_H    0x43
#define MPU6050_REG_GYRO_XOUT_L    0x44
#define MPU6050_REG_GYRO_YOUT_H    0x45
#define MPU6050_REG_GYRO_YOUT_L    0x46
#define MPU6050_REG_GYRO_ZOUT_H    0x47
#define MPU6050_REG_GYRO_ZOUT_L    0x48
#define MPU6050_REG_PWR_MGMT_1     0x6B
#define MPU6050_REG_PWR_MGMT_2     0x6C
#define MPU6050_REG_WHO_AM_I       0x75

// 陀螺仪量程定义
typedef enum {
    MPU6050_GYRO_RANGE_250DPS  = 0, // ±250°/s
    MPU6050_GYRO_RANGE_500DPS,      // ±500°/s
    MPU6050_GYRO_RANGE_1000DPS,     // ±1000°/s
    MPU6050_GYRO_RANGE_2000DPS      // ±2000°/s
} mpu6050_gyro_range_t;

// 加速度计量程定义
typedef enum {
    MPU6050_ACCEL_RANGE_2G   = 0,    // ±2g
    MPU6050_ACCEL_RANGE_4G,          // ±4g
    MPU6050_ACCEL_RANGE_8G,          // ±8g
    MPU6050_ACCEL_RANGE_16G          // ±16g
} mpu6050_accel_range_t;

// 低通滤波器截止频率定义
typedef enum {
    MPU6050_DLPF_BW_250HZ   = 0,     // 250Hz
    MPU6050_DLPF_BW_184HZ,           // 184Hz
    MPU6050_DLPF_BW_92HZ,            // 92Hz
    MPU6050_DLPF_BW_41HZ,            // 41Hz
    MPU6050_DLPF_BW_20HZ,            // 20Hz
    MPU6050_DLPF_BW_10HZ,            // 10Hz
    MPU6050_DLPF_BW_5HZ              // 5Hz
} mpu6050_dlpf_bandwidth_t;

// MPU6050配置结构体
typedef struct {
    i2c_channel_t i2c_channel;        // I2C通道
    uint8_t device_address;           // 设备地址
    mpu6050_gyro_range_t gyro_range;  // 陀螺仪量程
    mpu6050_accel_range_t accel_range;// 加速度计量程
    mpu6050_dlpf_bandwidth_t dlpf_bandwidth; // 低通滤波器带宽
    uint8_t sample_rate_divider;      // 采样率分频器
} mpu6050_config_t;

// MPU6050传感器数据结构体
typedef struct {
    // 陀螺仪原始数据
    int16_t gyro_raw[3];              // [x, y, z]
    
    // 加速度计原始数据
    int16_t accel_raw[3];             // [x, y, z]
    
    // 陀螺仪数据 (°/s)
    float gyro[3];                    // [x, y, z]
    
    // 加速度计数据 (g)
    float accel[3];                   // [x, y, z]
    
    // 温度数据 (°C)
    float temperature;
    
    // 数据有效性标志
    bool data_valid;
} mpu6050_data_t;

// 校准参数结构体
typedef struct {
    // 陀螺仪零偏
    float gyro_offset[3];             // [x, y, z]
    
    // 加速度计零偏
    float accel_offset[3];            // [x, y, z]
    
    // 加速度计刻度因子
    float accel_scale[3];             // [x, y, z]
} mpu6050_calib_data_t;

// 函数声明

/**
 * @brief 初始化MPU6050
 * @param config MPU6050配置结构体
 * @return 是否初始化成功
 */
bool mpu6050_init(mpu6050_config_t *config);

/**
 * @brief 复位MPU6050
 * @return 是否复位成功
 */
bool mpu6050_reset(void);

/**
 * @brief 获取MPU6050设备ID
 * @param device_id 设备ID缓冲区
 * @return 是否获取成功
 */
bool mpu6050_get_device_id(uint8_t *device_id);

/**
 * @brief 设置陀螺仪量程
 * @param range 陀螺仪量程
 * @return 是否设置成功
 */
bool mpu6050_set_gyro_range(mpu6050_gyro_range_t range);

/**
 * @brief 设置加速度计量程
 * @param range 加速度计量程
 * @return 是否设置成功
 */
bool mpu6050_set_accel_range(mpu6050_accel_range_t range);

/**
 * @brief 设置低通滤波器带宽
 * @param bandwidth 带宽设置
 * @return 是否设置成功
 */
bool mpu6050_set_dlpf_bandwidth(mpu6050_dlpf_bandwidth_t bandwidth);

/**
 * @brief 设置采样率分频器
 * @param divider 分频器值
 * @return 是否设置成功
 */
bool mpu6050_set_sample_rate_divider(uint8_t divider);

/**
 * @brief 唤醒MPU6050
 * @return 是否唤醒成功
 */
bool mpu6050_wake_up(void);

/**
 * @brief 让MPU6050进入睡眠模式
 * @return 是否设置成功
 */
bool mpu6050_sleep(void);

/**
 * @brief 读取MPU6050原始数据
 * @param data 数据缓冲区
 * @return 是否读取成功
 */
bool mpu6050_read_raw_data(mpu6050_data_t *data);

/**
 * @brief 读取MPU6050传感器数据（转换为物理量）
 * @param data 数据缓冲区
 * @return 是否读取成功
 */
bool mpu6050_read_data(mpu6050_data_t *data);

/**
 * @brief 校准MPU6050
 * @param calib_data 校准数据缓冲区
 * @param samples 采样次数
 * @return 是否校准成功
 */
bool mpu6050_calibrate(mpu6050_calib_data_t *calib_data, uint16_t samples);

/**
 * @brief 设置MPU6050校准参数
 * @param calib_data 校准数据
 */
void mpu6050_set_calibration_data(mpu6050_calib_data_t *calib_data);

/**
 * @brief 获取MPU6050校准参数
 * @param calib_data 校准数据缓冲区
 */
void mpu6050_get_calibration_data(mpu6050_calib_data_t *calib_data);

/**
 * @brief 检查MPU6050是否就绪
 * @return 是否已初始化并就绪
 */
bool mpu6050_is_ready(void);

#endif /* MPU6050_H */