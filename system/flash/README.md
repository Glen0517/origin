# Flash功能模块说明

## 概述
本模块提供了STM32F4系列微控制器的Flash存储器操作功能，包括扇区擦除、字节/半字/字/双字写入、缓冲区读写等操作。

## 文件结构
- `flash.h`：头文件，包含宏定义、数据结构和函数声明
- `flash.c`：源文件，实现Flash操作功能
- `flash_demo.h`：演示函数声明
- `flash_demo.c`：演示程序，展示Flash功能的使用方法
- `README.md`：本说明文件

## 功能特点
1. 支持扇区擦除
2. 支持字节(8位)、半字(16位)、字(32位)和双字(64位)写入
3. 支持字节(8位)、半字(16位)、字(32位)和双字(64位)读取
4. 支持缓冲区批量读写
5. 包含扇区检查、扇区编号获取、扇区地址计算等辅助功能
6. 提供错误处理机制

## 使用方法
1. 初始化Flash：
   ```c
   Flash_Init();
   ```

2. 擦除扇区：
   ```c
   uint32_t sector = FLASH_SECTOR_5;  // 选择扇区5
   Flash_EraseSector(sector);
   ```

3. 写入数据：
   ```c
   uint32_t address = FLASH_SECTOR_5;  // 写入地址
   uint8_t data = 0xAA;  // 要写入的数据
   Flash_WriteByte(address, data);  // 写入字节
   Flash_WriteHalfWord(address + 2, 0x55AA);  // 写入半字
   Flash_WriteWord(address + 4, 0x12345678);  // 写入字
   Flash_WriteDoubleWord(address + 8, 0x1122334455667788ULL);  // 写入双字
   ```

4. 读取数据：
   ```c
   uint8_t byte_data = Flash_ReadByte(address);  // 读取字节
   uint16_t half_word_data = Flash_ReadHalfWord(address + 2);  // 读取半字
   uint32_t word_data = Flash_ReadWord(address + 4);  // 读取字
   uint64_t double_word_data = Flash_ReadDoubleWord(address + 8);  // 读取双字
   ```

5. 缓冲区读写：
   ```c
   uint8_t buffer[10] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
   uint8_t read_buffer[10];
   Flash_WriteBuffer(address + 16, buffer, 10);  // 写入缓冲区
   Flash_ReadBuffer(address + 16, read_buffer, 10);  // 读取缓冲区
   ```

## 演示程序
运行`Flash_Demo()`函数可以进行Flash功能的完整演示，包括擦除、写入、读取和验证操作。

## 注意事项
1. Flash写入前必须先擦除对应的扇区
2. 写入地址必须对齐：字节(任意地址)、半字(2字节对齐)、字(4字节对齐)、双字(8字节对齐)
3. 操作Flash前需要解锁，操作完成后需要锁定
4. 避免频繁擦写Flash，会影响Flash寿命
5. 演示程序使用了UART功能，需要确保UART已经初始化
6. 本模块依赖STM32F4xx_HAL_Driver库

## 错误码说明
- `FLASH_OK` (0)：操作成功
- `FLASH_ERROR` (1)：操作失败
- `FLASH_TIMEOUT` (2)：操作超时
- `FLASH_PROTECTED` (3)：Flash被保护

## 版本信息
- 版本：V1.0
- 日期：2025-07-31