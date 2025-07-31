#ifndef __FLASH_H
#define __FLASH_H

#include "stm32f4xx_hal.h"
#include "spi.h"

/* SPI Flash 硬件配置 */
#define SPI_FLASH_CS_PORT      GPIOA                 /* 片选端口 */
#define SPI_FLASH_CS_PIN       GPIO_PIN_4            /* 片选引脚 */
#define SPI_FLASH_SPEED        SPI_BAUDRATEPRESCALER_2  /* SPI波特率预分频器 */
#define SPI_FLASH_MODE         SPI_MODE_3             /* SPI模式(CPOL=1, CPHA=1) */

/* SPI Flash 容量定义 */
#define FLASH_BASE             0x00000000            /* Flash起始地址 */
#define FLASH_SIZE             0x1000000             /* Flash容量(16MB) */
#define FLASH_MAX_ADDRESS      (FLASH_BASE + FLASH_SIZE - 1) /* Flash最大地址 */
#define FLASH_PAGE_SIZE        0x100                 /* 页大小(256字节) */
#define FLASH_SECTOR_SIZE      0x1000                /* 扇区大小(4KB) */
#define FLASH_BLOCK_SIZE_32K   0x8000                /* 32KB块大小 */
#define FLASH_BLOCK_SIZE_64K   0x10000               /* 64KB块大小 */
#define FLASH_MAX_SECTOR       (FLASH_SIZE / FLASH_SECTOR_SIZE) /* 最大扇区数 */

/* SPI Flash 命令定义 */
#define W25Q_WRITE_ENABLE     0x06    /* 写使能 */
#define W25Q_WRITE_DISABLE    0x04    /* 写禁止 */
#define W25Q_READ_STATUS1     0x05    /* 读状态寄存器1 */
#define W25Q_READ_STATUS2     0x35    /* 读状态寄存器2 */
#define W25Q_WRITE_STATUS     0x01    /* 写状态寄存器 */
#define W25Q_PAGE_PROGRAM     0x02    /* 页编程 */
#define W25Q_SECTOR_ERASE     0x20    /* 扇区擦除(4KB) */
#define W25Q_BLOCK_ERASE_32K  0x52    /* 块擦除(32KB) */
#define W25Q_BLOCK_ERASE_64K  0xD8    /* 块擦除(64KB) */
#define W25Q_CHIP_ERASE       0xC7    /* 整片擦除 */
#define W25Q_READ_DATA        0x03    /* 读数据 */
#define W25Q_FAST_READ        0x0B    /* 快速读 */
#define W25Q_JEDEC_ID         0x9F    /* 读JEDEC ID */

/* SPI Flash 状态位定义 */
#define W25Q_BUSY_BIT         0x01    /* 忙标志位 */
#define W25Q_WEL_BIT          0x02    /* 写使能锁存位 */

/* Flash 操作状态 */
#define FLASH_OK           0
#define FLASH_ERROR        1
#define FLASH_TIMEOUT      2
#define FLASH_PROTECTED    3

/* Flash 写保护状态 */
#define FLASH_WRP_DISABLE  0
#define FLASH_WRP_ENABLE   1

/* 函数声明 */
uint8_t Flash_Init(void);
uint8_t Flash_EraseSector(uint32_t sector);
uint8_t Flash_EraseBlock32K(uint32_t address);
uint8_t Flash_EraseBlock64K(uint32_t address);
uint8_t Flash_EraseChip(void);
uint8_t Flash_WriteByte(uint32_t address, uint8_t data);
uint8_t Flash_WriteHalfWord(uint32_t address, uint16_t data);
uint8_t Flash_WriteWord(uint32_t address, uint32_t data);
uint8_t Flash_WriteDoubleWord(uint32_t address, uint64_t data);
uint8_t Flash_WriteBuffer(uint32_t address, uint8_t *buffer, uint32_t length);
uint8_t Flash_ReadByte(uint32_t address);
uint16_t Flash_ReadHalfWord(uint32_t address);
uint32_t Flash_ReadWord(uint32_t address);
uint64_t Flash_ReadDoubleWord(uint32_t address);
uint8_t Flash_ReadBuffer(uint32_t address, uint8_t *buffer, uint32_t length);
uint8_t Flash_CheckSector(uint32_t sector);
uint8_t Flash_GetSectorNumber(uint32_t address);
uint32_t Flash_GetSectorStartAddress(uint32_t sector);
uint32_t Flash_GetSectorEndAddress(uint32_t sector);
uint32_t Flash_ReadID(void);
void Flash_CS_Enable(void);
void Flash_CS_Disable(void);
void Flash_ErrorHandler(void);

/* 内部函数声明 (不对外公开) */
static uint8_t Flash_WaitBusy(uint32_t timeout);
static void Flash_WriteEnable(void);

/* 内部函数声明 (不对外公开) */
static uint8_t Flash_WaitBusy(uint32_t timeout);
static void Flash_WriteEnable(void);

#endif /* __FLASH_H */