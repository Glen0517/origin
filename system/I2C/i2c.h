#ifndef _I2C_H
#define _I2C_H
#include "stdint.h"
#include "stdbool.h"

/** 最大支持的I2C设备数量 */
#define MAX_I2C_DEVICES 10

// 无符号整数（8位到64位）
typedef unsigned char       uint8_t;    // 8位无符号（范围：0~255）
typedef unsigned short      uint16_t;   // 16位无符号（范围：0~65535）
typedef unsigned int        uint32_t;   // 32位无符号（范围：0~4294967295）
typedef unsigned long long  uint64_t;   // 64位无符号

// 有符号整数（8位到64位）
typedef signed char         int8_t;     // 8位有符号（范围：-128~127）
typedef short               int16_t;    // 16位有符号（范围：-32768~32767）
typedef int                 int32_t;    // 32位有符号
typedef long long           int64_t;    // 64位有符号

// 布尔类型（通常用8位无符号）
typedef uint8_t             bool_t;     // 0=false, 非0=true

typedef uint32_t I2C_HandleTypeDef ;
typedef struct I2CManager_Struct I2CManager;
typedef struct I2CDevice_Struct I2CDevice;
typedef bool_t HAL_StatusTypeDef;

#define HAL_OK 0
#define HAL_ERROR 1

/** I2C设备结构体 */
typedef struct {
    I2C_HandleTypeDef* hi2c;    // I2C句柄
    uint16_t device_address;   // I2C设备地址（7位地址左移1位，包含读写位）
    uint32_t timeout;          // 超时时间（毫秒）

    // 函数指针已移至I2CManager_Struct中定义
    // 此处仅保留设备特定参数
    I2C_HandleTypeDef* hi2c;    // I2C句柄
    uint16_t device_address;   // I2C设备地址（7位地址左移1位，包含读写位）
    uint32_t timeout;          // 超时时间（毫秒）
} I2CDevice_Struct;

/** I2C设备管理器 */
typedef struct {
    I2CDevice devices[MAX_I2C_DEVICES]; // 设备数组
    int device_count;                   // 当前设备数量

    I2CManager_Struct* (*i2c_manager_create)(I2C_HandleTypeDef* hi2c) ;
    void (*i2c_manager_destroy)(I2CManager_Struct* manager) ;
    int (*i2c_add_device)(I2CManager_Struct* manager, uint8_t address) ;
} I2CManager_Struct;

I2CManager_Struct* i2c_manager_create(I2C_HandleTypeDef* hi2c) ;
void i2c_manager_destroy(I2CManager_Struct* manager) ;
int i2c_add_device(I2CManager_Struct* manager, uint8_t address) ;
void i2c_set_timeout(I2CManager_Struct* manager, int device_index, uint32_t timeout) ;
HAL_StatusTypeDef i2c_send(I2CManager_Struct* manager, int device_index, uint8_t* pData, uint16_t Size);
HAL_StatusTypeDef i2c_receive(I2CManager_Struct* manager, int device_index, uint8_t* pData, uint16_t Size);
HAL_StatusTypeDef i2c_write_register(I2CManager_Struct* manager, int device_index, uint16_t RegisterAddress, uint8_t* pData, uint16_t Size) ;
HAL_StatusTypeDef i2c_read_register(I2CManager_Struct* manager, int device_index, uint16_t RegisterAddress, uint8_t* pData, uint16_t Size) ;
void i2c_multiple_devices_example(I2C_HandleTypeDef* hi2c);

#endif