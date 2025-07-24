#ifndef SPI_LIBRARY_H
#define SPI_LIBRARY_H

#include <stdint.h>

/* SPI模式定义 */
typedef enum {
    SPI_MODE0 = 0,  // CPOL=0, CPHA=0
    SPI_MODE1 = 1,  // CPOL=0, CPHA=1
    SPI_MODE2 = 2,  // CPOL=1, CPHA=0
    SPI_MODE3 = 3   // CPOL=1, CPHA=1
} SPIMode;

/* SPI数据位宽 */
typedef enum {
    SPI_DATA_8BIT = 8,
    SPI_DATA_16BIT = 16
} SPIDataSize;

/* SPI波特率预分频器 */
typedef enum {
    SPI_BAUD_PRESCALER_2 = 0,
    SPI_BAUD_PRESCALER_4 = 1,
    SPI_BAUD_PRESCALER_8 = 2,
    SPI_BAUD_PRESCALER_16 = 3,
    SPI_BAUD_PRESCALER_32 = 4,
    SPI_BAUD_PRESCALER_64 = 5,
    SPI_BAUD_PRESCALER_128 = 6,
    SPI_BAUD_PRESCALER_256 = 7
} SPIPrescaler;

/* SPI配置结构体 */
typedef struct {
    uint32_t baseAddress;     // SPI外设基地址
    SPIMode mode;             // SPI模式
    SPIDataSize dataSize;     // 数据位宽
    SPIPrescaler prescaler;   // 波特率预分频器
    uint8_t masterMode;       // 主从模式(1=主模式,0=从模式)
    uint8_t lsbFirst;         // 低位在前(1=LSB优先,0=MSB优先)
} SPIConfig_Struct;

/* SPI寄存器定义 */
typedef struct {
    volatile uint32_t CR1;    // 控制寄存器1
    volatile uint32_t CR2;    // 控制寄存器2
    volatile uint32_t SR;     // 状态寄存器
    volatile uint32_t DR;     // 数据寄存器
    volatile uint32_t CRCPR;  // CRC多项式寄存器
    volatile uint32_t RXCRCR; // 接收CRC寄存器
    volatile uint32_t TXCRCR; // 发送CRC寄存器
    volatile uint32_t I2SCFGR; // I2S配置寄存器
    volatile uint32_t I2SPR;  // I2S预分频器寄存器
} SPIRegisters_Struct;

/* SPI状态寄存器标志位 */
#define SPI_SR_RXNE     (1 << 0)  // 接收缓冲区非空
#define SPI_SR_TXE      (1 << 1)  // 发送缓冲区为空
#define SPI_SR_BSY      (1 << 7)  // 忙标志

/* SPI控制寄存器1位定义 */
#define SPI_CR1_CPHA    (1 << 0)  // 时钟相位
#define SPI_CR1_CPOL    (1 << 1)  // 时钟极性
#define SPI_CR1_MSTR    (1 << 2)  // 主模式
#define SPI_CR1_BR_POS  3         // 波特率控制位位置
#define SPI_CR1_BR_MASK (7 << 3)  // 波特率控制位掩码
#define SPI_CR1_SPE     (1 << 6)  // SPI使能
#define SPI_CR1_LSBFIRST (1 << 7) // 帧格式(LSB/MSB优先)
#define SPI_CR1_SSI     (1 << 8)  // 内部从设备选择
#define SPI_CR1_SSM     (1 << 9)  // 软件从设备管理
#define SPI_CR1_DFF     (1 << 11) // 数据帧格式(8/16位)

#define NULL ((void *)0)

/**
 * 初始化SPI外设
 * @param config SPI配置结构体指针
 */
void SPI_Init(SPIConfig_Struct *config);

/**
 * 使能SPI外设
 * @param baseAddress SPI外设基地址
 */
void SPI_Enable(uint32_t baseAddress);

/**
 * 禁用SPI外设
 * @param baseAddress SPI外设基地址
 */
void SPI_Disable(uint32_t baseAddress);

/**
 * 通过SPI发送单个字节并接收响应
 * @param baseAddress SPI外设基地址
 * @param data 要发送的数据
 * @return 接收到的数据
 */
uint8_t SPI_TransferByte(uint32_t baseAddress, uint8_t data);

/**
 * 通过SPI发送多个字节
 * @param baseAddress SPI外设基地址
 * @param txBuffer 发送缓冲区指针
 * @param rxBuffer 接收缓冲区指针
 * @param length 要发送的字节数
 */
void SPI_Transfer(uint32_t baseAddress, uint8_t *txBuffer, uint8_t *rxBuffer, uint32_t length);

/**
 * 检查SPI是否忙
 * @param baseAddress SPI外设基地址
 * @return 1=忙, 0=空闲
 */
uint8_t SPI_IsBusy(uint32_t baseAddress);

#endif