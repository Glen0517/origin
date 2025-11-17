/*
 * MPU6050陀螺仪/加速度计驱动实现
 */

#include "mpu6050.h"
#include "../config/config.h"
#include "../service/system.h"
#include <math.h>

// 静态变量定义
static mpu6050_config_t mpu_config;
static mpu6050_calib_data_t calib_data;
static float gyro_scale_factor = 0.0f;
static float accel_scale_factor = 0.0f;
static bool mpu_initialized = false;

// 陀螺仪量程对应的灵敏度因子 (LSB/°/s)
static const float GYRO_SCALE_FACTOR[4] = {
    131.0f,  // ±250°/s
    65.5f,   // ±500°/s
    32.8f,   // ±1000°/s
    16.4f    // ±2000°/s
};

// 加速度计量程对应的灵敏度因子 (LSB/g)
static const float ACCEL_SCALE_FACTOR[4] = {
    16384.0f, // ±2g
    8192.0f,  // ±4g
    4096.0f,  // ±8g
    2048.0f   // ±16g
};

/**
 * @brief 初始化MPU6050
 */
bool mpu6050_init(mpu6050_config_t *config) {
    uint8_t device_id;
    
    if (config == NULL) {
        return false;
    }
    
    // 保存配置
    mpu_config = *config;
    
    // 初始化I2C
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .speed_mode = I2C_FAST_MODE, // 400kHz
        .address_mode = I2C_ADDRESS_7BIT,
        .own_address = 0,
        .dma_enable = false,
        .interrupt_enable = false
    };
    
    if (!hal_i2c_init(mpu_config.i2c_channel, &i2c_config)) {
        return false;
    }
    
    hal_i2c_enable(mpu_config.i2c_channel);
    
    // 检查设备连接
    if (!hal_i2c_is_device_connected(mpu_config.i2c_channel, mpu_config.device_address)) {
        return false;
    }
    
    // 读取设备ID
    if (!mpu6050_get_device_id(&device_id)) {
        return false;
    }
    
    // 验证设备ID
    if (device_id != 0x68) {
        return false;
    }
    
    // 复位MPU6050
    if (!mpu6050_reset()) {
        return false;
    }
    
    // 唤醒MPU6050
    if (!mpu6050_wake_up()) {
        return false;
    }
    
    // 配置陀螺仪量程
    if (!mpu6050_set_gyro_range(mpu_config.gyro_range)) {
        return false;
    }
    
    // 配置加速度计量程
    if (!mpu6050_set_accel_range(mpu_config.accel_range)) {
        return false;
    }
    
    // 配置低通滤波器
    if (!mpu6050_set_dlpf_bandwidth(mpu_config.dlpf_bandwidth)) {
        return false;
    }
    
    // 配置采样率
    if (!mpu6050_set_sample_rate_divider(mpu_config.sample_rate_divider)) {
        return false;
    }
    
    // 初始化校准数据
    for (uint8_t i = 0; i < 3; i++) {
        calib_data.gyro_offset[i] = 0.0f;
        calib_data.accel_offset[i] = 0.0f;
        calib_data.accel_scale[i] = 1.0f;
    }
    
    // 标记为已初始化
    mpu_initialized = true;
    
    return true;
}

/**
 * @brief 复位MPU6050
 */
bool mpu6050_reset(void) {
    // 设置复位位
    if (!hal_i2c_write_byte(mpu_config.i2c_channel, mpu_config.device_address,
                           MPU6050_REG_PWR_MGMT_1, 0x80)) {
        return false;
    }
    
    // 等待复位完成
    system_delay_ms(100);
    
    return true;
}

/**
 * @brief 获取MPU6050设备ID
 */
bool mpu6050_get_device_id(uint8_t *device_id) {
    if (device_id == NULL) {
        return false;
    }
    
    return hal_i2c_read_byte(mpu_config.i2c_channel, mpu_config.device_address,
                           MPU6050_REG_WHO_AM_I, device_id);
}

/**
 * @brief 设置陀螺仪量程
 */
bool mpu6050_set_gyro_range(mpu6050_gyro_range_t range) {
    uint8_t reg_value;
    
    // 读取当前配置
    if (!hal_i2c_read_byte(mpu_config.i2c_channel, mpu_config.device_address,
                           MPU6050_REG_GYRO_CONFIG, &reg_value)) {
        return false;
    }
    
    // 清除量程位
    reg_value &= 0xE7; // 清除bit3和bit4
    
    // 设置新量程
    reg_value |= (range << 3);
    
    // 写入配置
    if (!hal_i2c_write_byte(mpu_config.i2c_channel, mpu_config.device_address,
                           MPU6050_REG_GYRO_CONFIG, reg_value)) {
        return false;
    }
    
    // 更新灵敏度因子
    gyro_scale_factor = 1.0f / GYRO_SCALE_FACTOR[range];
    
    return true;
}

/**
 * @brief 设置加速度计量程
 */
bool mpu6050_set_accel_range(mpu6050_accel_range_t range) {
    uint8_t reg_value;
    
    // 读取当前配置
    if (!hal_i2c_read_byte(mpu_config.i2c_channel, mpu_config.device_address,
                           MPU6050_REG_ACCEL_CONFIG, &reg_value)) {
        return false;
    }
    
    // 清除量程位
    reg_value &= 0xE7; // 清除bit3和bit4
    
    // 设置新量程
    reg_value |= (range << 3);
    
    // 写入配置
    if (!hal_i2c_write_byte(mpu_config.i2c_channel, mpu_config.device_address,
                           MPU6050_REG_ACCEL_CONFIG, reg_value)) {
        return false;
    }
    
    // 更新灵敏度因子
    accel_scale_factor = 1.0f / ACCEL_SCALE_FACTOR[range];
    
    return true;
}

/**
 * @brief 设置低通滤波器带宽
 */
bool mpu6050_set_dlpf_bandwidth(mpu6050_dlpf_bandwidth_t bandwidth) {
    uint8_t reg_value;
    
    // 读取当前配置
    if (!hal_i2c_read_byte(mpu_config.i2c_channel, mpu_config.device_address,
                           MPU6050_REG_CONFIG, &reg_value)) {
        return false;
    }
    
    // 清除带宽位
    reg_value &= 0xF8; // 清除bit0-bit2
    
    // 设置新带宽
    reg_value |= bandwidth;
    
    // 写入配置
    if (!hal_i2c_write_byte(mpu_config.i2c_channel, mpu_config.device_address,
                           MPU6050_REG_CONFIG, reg_value)) {
        return false;
    }
    
    return true;
}

/**
 * @brief 设置采样率分频器
 */
bool mpu6050_set_sample_rate_divider(uint8_t divider) {
    return hal_i2c_write_byte(mpu_config.i2c_channel, mpu_config.device_address,
                           MPU6050_REG_SMPLRT_DIV, divider);
}

/**
 * @brief 唤醒MPU6050
 */
bool mpu6050_wake_up(void) {
    return hal_i2c_write_byte(mpu_config.i2c_channel, mpu_config.device_address,
                           MPU6050_REG_PWR_MGMT_1, 0x00);
}

/**
 * @brief 让MPU6050进入睡眠模式
 */
bool mpu6050_sleep(void) {
    return hal_i2c_write_byte(mpu_config.i2c_channel, mpu_config.device_address,
                           MPU6050_REG_PWR_MGMT_1, 0x40);
}

/**
 * @brief 读取MPU6050原始数据
 */
bool mpu6050_read_raw_data(mpu6050_data_t *data) {
    uint8_t buffer[14];
    
    if (data == NULL || !mpu_initialized) {
        return false;
    }
    
    // 读取14个寄存器 (ACCEL_XOUT_H到GYRO_ZOUT_L)
    if (!hal_i2c_read_data(mpu_config.i2c_channel, mpu_config.device_address,
                          MPU6050_REG_ACCEL_XOUT_H, buffer, 14)) {
        return false;
    }
    
    // 加速度计原始数据 (高字节在前)
    data->accel_raw[0] = (int16_t)((buffer[0] << 8) | buffer[1]);
    data->accel_raw[1] = (int16_t)((buffer[2] << 8) | buffer[3]);
    data->accel_raw[2] = (int16_t)((buffer[4] << 8) | buffer[5]);
    
    // 温度数据
    int16_t temp_raw = (int16_t)((buffer[6] << 8) | buffer[7]);
    data->temperature = (temp_raw / 340.0f) + 36.53f;
    
    // 陀螺仪原始数据 (高字节在前)
    data->gyro_raw[0] = (int16_t)((buffer[8] << 8) | buffer[9]);
    data->gyro_raw[1] = (int16_t)((buffer[10] << 8) | buffer[11]);
    data->gyro_raw[2] = (int16_t)((buffer[12] << 8) | buffer[13]);
    
    // 标记数据有效
    data->data_valid = true;
    
    return true;
}

/**
 * @brief 读取MPU6050传感器数据（转换为物理量）
 */
bool mpu6050_read_data(mpu6050_data_t *data) {
    if (data == NULL || !mpu_initialized) {
        return false;
    }
    
    // 读取原始数据
    if (!mpu6050_read_raw_data(data)) {
        return false;
    }
    
    // 应用校准参数并转换为物理量
    // 陀螺仪数据 (°/s)
    data->gyro[0] = ((float)data->gyro_raw[0] * gyro_scale_factor) - calib_data.gyro_offset[0];
    data->gyro[1] = ((float)data->gyro_raw[1] * gyro_scale_factor) - calib_data.gyro_offset[1];
    data->gyro[2] = ((float)data->gyro_raw[2] * gyro_scale_factor) - calib_data.gyro_offset[2];
    
    // 加速度计数据 (g)
    data->accel[0] = (((float)data->accel_raw[0] * accel_scale_factor) - calib_data.accel_offset[0]) * calib_data.accel_scale[0];
    data->accel[1] = (((float)data->accel_raw[1] * accel_scale_factor) - calib_data.accel_offset[1]) * calib_data.accel_scale[1];
    data->accel[2] = (((float)data->accel_raw[2] * accel_scale_factor) - calib_data.accel_offset[2]) * calib_data.accel_scale[2];
    
    return true;
}

/**
 * @brief 校准MPU6050
 */
bool mpu6050_calibrate(mpu6050_calib_data_t *calib_data_out, uint16_t samples) {
    mpu6050_data_t data;
    int32_t gyro_sum[3] = {0};
    int32_t accel_sum[3] = {0};
    
    if (calib_data_out == NULL || !mpu_initialized) {
        return false;
    }
    
    // 确保无人机静止
    system_delay_ms(1000);
    
    // 采样多次计算平均值作为零偏
    for (uint16_t i = 0; i < samples; i++) {
        if (!mpu6050_read_raw_data(&data)) {
            return false;
        }
        
        // 累加原始数据
        gyro_sum[0] += data.gyro_raw[0];
        gyro_sum[1] += data.gyro_raw[1];
        gyro_sum[2] += data.gyro_raw[2];
        
        accel_sum[0] += data.accel_raw[0];
        accel_sum[1] += data.accel_raw[1];
        accel_sum[2] += data.accel_raw[2];
        
        system_delay_ms(5);
    }
    
    // 计算陀螺仪零偏 (°/s)
    calib_data_out->gyro_offset[0] = (float)gyro_sum[0] / samples * gyro_scale_factor;
    calib_data_out->gyro_offset[1] = (float)gyro_sum[1] / samples * gyro_scale_factor;
    calib_data_out->gyro_offset[2] = (float)gyro_sum[2] / samples * gyro_scale_factor;
    
    // 计算加速度计零偏和刻度因子
    // 假设Z轴指向重力方向
    float accel_avg[3];
    accel_avg[0] = (float)accel_sum[0] / samples * accel_scale_factor;
    accel_avg[1] = (float)accel_sum[1] / samples * accel_scale_factor;
    accel_avg[2] = (float)accel_sum[2] / samples * accel_scale_factor;
    
    // 计算Z轴刻度因子 (理想情况下应该等于1g)
    float accel_z_mag = fabs(accel_avg[2]);
    
    calib_data_out->accel_offset[0] = accel_avg[0];
    calib_data_out->accel_offset[1] = accel_avg[1];
    calib_data_out->accel_offset[2] = accel_avg[2] - 1.0f; // 假设Z轴指向下方，应该为-1g
    
    calib_data_out->accel_scale[0] = 1.0f;
    calib_data_out->accel_scale[1] = 1.0f;
    calib_data_out->accel_scale[2] = 1.0f / accel_z_mag; // 刻度因子
    
    // 更新校准数据
    calib_data = *calib_data_out;
    
    return true;
}

/**
 * @brief 设置MPU6050校准参数
 */
void mpu6050_set_calibration_data(mpu6050_calib_data_t *calib_data_in) {
    if (calib_data_in != NULL) {
        calib_data = *calib_data_in;
    }
}

/**
 * @brief 获取MPU6050校准参数
 */
void mpu6050_get_calibration_data(mpu6050_calib_data_t *calib_data_out) {
    if (calib_data_out != NULL) {
        *calib_data_out = calib_data;
    }
}

/**
 * @brief 重新初始化MPU6050
 * @return 是否重新初始化成功
 */
bool mpu6050_reinit(void) {
    if (!mpu_initialized) {
        return false;
    }
    
    // 标记为未初始化
    mpu_initialized = false;
    
    // 重新初始化
    return mpu6050_init(&mpu_config);
}

/**
 * @brief 检查MPU6050是否就绪
 * @return 是否已初始化并就绪
 */
bool mpu6050_is_ready(void) {
    return mpu_initialized;
}