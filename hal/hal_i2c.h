/*
 * I2C硬件抽象层接口定义
 */

#ifndef HAL_I2C_H
#define HAL_I2C_H

#include "../include/types.h"

// I2C通道定义
typedef enum {
    I2C_1 = 0,
    I2C_2,
    I2C_3,
    I2C_4,
    I2C_MAX
} i2c_channel_t;

// I2C模式定义
typedef enum {
    I2C_MODE_MASTER = 0,
    I2C_MODE_SLAVE,
    I2C_MODE_MAX
} i2c_mode_t;

// I2C传输速度定义
typedef enum {
    I2C_STANDARD_MODE = 0,     // 100kHz
    I2C_FAST_MODE,             // 400kHz
    I2C_FAST_MODE_PLUS,        // 1MHz
    I2C_HIGH_SPEED_MODE,       // 3.4MHz
    I2C_SPEED_MAX
} i2c_speed_mode_t;

// I2C地址模式定义
typedef enum {
    I2C_ADDRESS_7BIT = 0,
    I2C_ADDRESS_10BIT,
    I2C_ADDRESS_MAX
} i2c_address_mode_t;

// I2C配置结构体
typedef struct {
    i2c_mode_t mode;           // I2C模式 (主机/从机)
    i2c_speed_mode_t speed_mode; // 传输速度模式
    i2c_address_mode_t address_mode; // 地址模式
    uint16_t own_address;      // 自身地址 (从机模式)
    bool dma_enable;           // DMA使能
    bool interrupt_enable;     // 中断使能
} i2c_config_t;

// 函数声明

/**
 * @brief 初始化I2C
 * @param channel I2C通道
 * @param config I2C配置结构体
 * @return 是否初始化成功
 */
bool hal_i2c_init(i2c_channel_t channel, i2c_config_t *config);

/**
 * @brief 向I2C从设备写入单个字节
 * @param channel I2C通道
 * @param dev_address 从设备地址
 * @param reg_address 寄存器地址
 * @param data 要写入的数据
 * @return 是否写入成功
 */
bool hal_i2c_write_byte(i2c_channel_t channel, uint16_t dev_address, uint8_t reg_address, uint8_t data);

/**
 * @brief 从I2C从设备读取单个字节
 * @param channel I2C通道
 * @param dev_address 从设备地址
 * @param reg_address 寄存器地址
 * @param data 接收数据缓冲区
 * @return 是否读取成功
 */
bool hal_i2c_read_byte(i2c_channel_t channel, uint16_t dev_address, uint8_t reg_address, uint8_t *data);

/**
 * @brief 向I2C从设备写入多个字节
 * @param channel I2C通道
 * @param dev_address 从设备地址
 * @param reg_address 寄存器地址
 * @param data 要写入的数据缓冲区
 * @param length 数据长度
 * @return 是否写入成功
 */
bool hal_i2c_write_data(i2c_channel_t channel, uint16_t dev_address, uint8_t reg_address, uint8_t *data, uint16_t length);

/**
 * @brief 从I2C从设备读取多个字节
 * @param channel I2C通道
 * @param dev_address 从设备地址
 * @param reg_address 寄存器地址
 * @param data 接收数据缓冲区
 * @param length 数据长度
 * @return 是否读取成功
 */
bool hal_i2c_read_data(i2c_channel_t channel, uint16_t dev_address, uint8_t reg_address, uint8_t *data, uint16_t length);

/**
 * @brief 发送I2C开始条件
 * @param channel I2C通道
 * @return 是否发送成功
 */
bool hal_i2c_start(i2c_channel_t channel);

/**
 * @brief 发送I2C停止条件
 * @param channel I2C通道
 */
void hal_i2c_stop(i2c_channel_t channel);

/**
 * @brief 检查I2C设备是否连接
 * @param channel I2C通道
 * @param dev_address 从设备地址
 * @return 设备是否连接
 */
bool hal_i2c_is_device_connected(i2c_channel_t channel, uint16_t dev_address);

/**
 * @brief 使能I2C
 * @param channel I2C通道
 */
void hal_i2c_enable(i2c_channel_t channel);

/**
 * @brief 禁用I2C
 * @param channel I2C通道
 */
void hal_i2c_disable(i2c_channel_t channel);

#endif /* HAL_I2C_H */