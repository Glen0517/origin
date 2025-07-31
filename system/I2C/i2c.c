#include "i2c.h"
#include <stdbool.h>

I2CMANAGER_Struct I2CManagerSTRUCT = {
    .i2c_manager_create = stm32_i2c_manager_create,
    .i2c_manager_destroy = stm32_i2c_manager_destroy,
    .device_count = 0,
    .i2c_add_device = 0,
    .devices = 0
};

I2CDevice_Struct I2CDeviceSTRUCT = {
    .device_address = 0x35,
    //.hi2c = 1,
    .i2c_read_register = stm32_i2c_read_register,
    .i2c_write_register = stm32_i2c_write_register,
    .i2c_receive =stm32_i2c_receive,
    .i2c_send = stm32_i2c_send
};

/**
 * 创建I2C管理器
 * @param hi2c I2C句柄指针
 * @return I2C管理器指针
 */
I2CManager_Struct* stm32_i2c_manager_create(I2C_HandleTypeDef* hi2c) {
    I2CManager_Struct* manager = (I2CManager_Struct*)malloc(sizeof(I2CManager_Struct));
    if (!manager) {
        return NULL;
    }
    
    memset(manager, 0, sizeof(I2CManager_Struct));
    
    // 初始化所有设备的I2C句柄
    for (int i = 0; i < MAX_I2C_DEVICES; i++) {
        manager->devices[i].hi2c = hi2c;
        manager->devices[i].timeout = 1000; // 默认超时时间1秒
    }
    
    return manager;
}

/**
 * 销毁I2C管理器
 * @param manager I2C管理器指针
 */
void stm32_i2c_manager_destroy(I2CManager_Struct* manager) {
    if (manager) {
        free(manager);
    }
}

/**
 * 添加I2C设备
 * @param manager I2C管理器指针
 * @param address I2C设备7位地址
 * @return 设备索引，失败返回-1
 */
int stm32_i2c_add_device(I2CManager_Struct* manager, uint8_t address) {
    if (!manager || manager->device_count >= MAX_I2C_DEVICES) {
        return -1;
    }
    
    // 检查地址是否已存在
    for (int i = 0; i < manager->device_count; i++) {
        if (manager->devices[i].device_address == (address << 1)) {
            return i; // 设备已存在，返回索引
        }
    }
    
    // 添加新设备
    int index = manager->device_count++;
    manager->devices[index].device_address = address << 1; // 转换为STM32 HAL库使用的格式
    return index;
}

/**
 * 设置设备超时时间
 * @param manager I2C管理器指针
 * @param device_index 设备索引
 * @param timeout 超时时间（毫秒）
 */
void stm32_i2c_set_timeout(I2CManager_Struct* manager, int device_index, uint32_t timeout) {
    if (manager && device_index >= 0 && device_index < manager->device_count) {
        manager->devices[device_index].timeout = timeout;
    }
}

/**
 * 向I2C设备发送数据
 * @param manager I2C管理器指针
 * @param device_index 设备索引
 * @param pData 数据缓冲区指针
 * @param Size 数据大小（字节）
 * @return HAL状态码
 */
HAL_StatusTypeDef stm32_i2c_send(I2CManager_Struct* manager, int device_index, uint8_t* pData, uint16_t Size) {
    if (!manager || device_index < 0 || device_index >= manager->device_count) {
        return HAL_ERROR;
    }
    
    I2CDevice_Struct* device = &manager->devices[device_index];
    return HAL_I2C_Master_Transmit(device->hi2c, device->device_address, pData, Size, device->timeout);
}

/**
 * 从I2C设备接收数据
 * @param manager I2C管理器指针
 * @param device_index 设备索引
 * @param pData 数据缓冲区指针
 * @param Size 数据大小（字节）
 * @return HAL状态码
 */
HAL_StatusTypeDef stm32_i2c_receive(I2CManager_Struct* manager, int device_index, uint8_t* pData, uint16_t Size) {
    if (!manager || device_index < 0 || device_index >= manager->device_count) {
        return HAL_ERROR;
    }
    
    I2CDevice_Struct* device = &manager->devices[device_index];
    return HAL_I2C_Master_Receive(device->hi2c, device->device_address, pData, Size, device->timeout);
}

/**
 * 向I2C设备的指定寄存器写入数据
 * @param manager I2C管理器指针
 * @param device_index 设备索引
 * @param RegisterAddress 寄存器地址
 * @param pData 数据缓冲区指针
 * @param Size 数据大小（字节）
 * @return HAL状态码
 */
HAL_StatusTypeDef stm32_i2c_write_register(I2CManager_Struct* manager, int device_index, uint16_t RegisterAddress, uint8_t* pData, uint16_t Size) {
    if (!manager || device_index < 0 || device_index >= manager->device_count) {
        return HAL_ERROR;
    }
    
    I2CDevice_Struct* device = &manager->devices[device_index];
    
    // 创建包含寄存器地址和数据的缓冲区
    uint8_t* buffer = (uint8_t*)malloc(Size + 1);
    if (!buffer) {
        return HAL_ERROR;
    }
    
    buffer[0] = (uint8_t)RegisterAddress;
    memcpy(buffer + 1, pData, Size);
    
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(device->hi2c, device->device_address, buffer, Size + 1, device->timeout);
    free(buffer);
    return status;
}

/**
 * 从I2C设备的指定寄存器读取数据
 * @param manager I2C管理器指针
 * @param device_index 设备索引
 * @param RegisterAddress 寄存器地址
 * @param pData 数据缓冲区指针
 * @param Size 数据大小（字节）
 * @return HAL状态码
 */
HAL_StatusTypeDef stm32_i2c_read_register(I2CManager_Struct* manager, int device_index, uint16_t RegisterAddress, uint8_t* pData, uint16_t Size) {
    if (!manager || device_index < 0 || device_index >= manager->device_count) {
        return HAL_ERROR;
    }
    
    I2CDevice* device = &manager->devices[device_index];
    
    // 发送寄存器地址
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(device->hi2c, device->device_address, (uint8_t*)&RegisterAddress, 1, device->timeout);
    if (status != HAL_OK) {
        return status;
    }
    
    // 接收数据
    return HAL_I2C_Master_Receive(device->hi2c, device->device_address, pData, Size, device->timeout);
}

/**
 * 示例：如何使用多地址I2C通信
 */
void stm32_i2c_multiple_devices_example(I2C_HandleTypeDef* hi2c) {
    // 创建I2C管理器
    I2CManager* manager = I2CManager_Struct.i2c_manager_create(hi2c);
    
    // 添加两个设备（地址0x48和0x49）
    int device1 = I2CDeviceSTRUCT.i2c_add_device(manager, 0x48);
    int device2 = I2CDeviceSTRUCT.i2c_add_device(manager, 0x49);
    
    // 设置不同的超时时间
    I2CDeviceSTRUCT.i2c_set_timeout(manager, device1, 500);  // 500ms超时
    I2CDeviceSTRUCT.i2c_set_timeout(manager, device2, 1000); // 1000ms超时
    
    // 向设备1的寄存器0x01写入数据
    uint8_t write_data = 0x55;
    I2CDeviceSTRUCT.i2c_write_register(manager, device1, 0x01, &write_data, 1);
    
    // 从设备1的寄存器0x01读取数据
    uint8_t read_data;
    I2CDeviceSTRUCT.i2c_read_register(manager, device1, 0x01, &read_data, 1);
    
    // 向设备2发送命令
    uint8_t command[] = {0x02, 0xAA};
    I2CDeviceSTRUCT.i2c_send(manager, device2, command, sizeof(command));
    
    // 从设备2接收数据
    uint8_t response[2];
    I2CDeviceSTRUCT.i2c_receive(manager, device2, response, sizeof(response));
    
    // 销毁管理器
    I2CMANAGER_Struct.i2c_manager_destroy(manager);
}