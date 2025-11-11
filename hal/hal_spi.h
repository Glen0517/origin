/*
 * SPI硬件抽象层接口定义
 */

#ifndef HAL_SPI_H
#define HAL_SPI_H

#include "../include/types.h"

// SPI通道定义
typedef enum {
    SPI_1 = 0,
    SPI_2,
    SPI_3,
    SPI_4,
    SPI_5,
    SPI_6,
    SPI_MAX
} spi_channel_t;

// SPI模式定义
typedef enum {
    SPI_MODE_MASTER = 0,
    SPI_MODE_SLAVE,
    SPI_MODE_MAX
} spi_mode_t;

// SPI时钟极性定义
typedef enum {
    SPI_CPOL_LOW = 0,         // 时钟空闲状态为低电平
    SPI_CPOL_HIGH,            // 时钟空闲状态为高电平
    SPI_CPOL_MAX
} spi_cpol_t;

// SPI时钟相位定义
typedef enum {
    SPI_CPHA_1EDGE = 0,       // 数据在第一个时钟沿采样
    SPI_CPHA_2EDGE,           // 数据在第二个时钟沿采样
    SPI_CPHA_MAX
} spi_cpha_t;

// SPI数据大小定义
typedef enum {
    SPI_DATA_SIZE_8BIT = 0,
    SPI_DATA_SIZE_16BIT,
    SPI_DATA_SIZE_MAX
} spi_data_size_t;

// SPI数据传输方向定义
typedef enum {
    SPI_DIRECTION_2LINES = 0,  // 双线双向全双工
    SPI_DIRECTION_2LINES_RXONLY, // 双线单向接收
    SPI_DIRECTION_1LINE,       // 单线双向
    SPI_DIRECTION_MAX
} spi_direction_t;

// SPI NSS管理定义
typedef enum {
    SPI_NSS_SOFT = 0,          // 软件控制
    SPI_NSS_HARD_INPUT,        // 硬件输入
    SPI_NSS_HARD_OUTPUT,       // 硬件输出
    SPI_NSS_MAX
} spi_nss_t;

// SPI配置结构体
typedef struct {
    uint32_t clock_speed;      // SPI时钟频率 (Hz)
    spi_mode_t mode;           // SPI模式 (主机/从机)
    spi_cpol_t cpol;           // 时钟极性
    spi_cpha_t cpha;           // 时钟相位
    spi_data_size_t data_size; // 数据大小 (8/16位)
    spi_direction_t direction; // 传输方向
    spi_nss_t nss;             // NSS管理
    bool dma_enable;           // DMA使能
    uint32_t fifo_threshold;   // FIFO阈值 (用于某些平台)
} spi_config_t;

// 函数声明

/**
 * @brief 初始化SPI
 * @param channel SPI通道
 * @param config SPI配置结构体
 * @return 是否初始化成功
 */
bool hal_spi_init(spi_channel_t channel, spi_config_t *config);

/**
 * @brief 发送单个数据
 * @param channel SPI通道
 * @param data 要发送的数据
 * @return 是否发送成功
 */
bool hal_spi_transmit_byte(spi_channel_t channel, uint8_t data);

/**
 * @brief 接收单个数据
 * @param channel SPI通道
 * @param data 接收数据缓冲区
 * @return 是否接收成功
 */
bool hal_spi_receive_byte(spi_channel_t channel, uint8_t *data);

/**
 * @brief 发送数据
 * @param channel SPI通道
 * @param data 要发送的数据缓冲区
 * @param length 数据长度
 * @return 是否发送成功
 */
bool hal_spi_transmit(spi_channel_t channel, uint8_t *data, uint16_t length);

/**
 * @brief 接收数据
 * @param channel SPI通道
 * @param data 接收数据缓冲区
 * @param length 数据长度
 * @return 是否接收成功
 */
bool hal_spi_receive(spi_channel_t channel, uint8_t *data, uint16_t length);

/**
 * @brief 全双工传输
 * @param channel SPI通道
 * @param tx_data 发送数据缓冲区
 * @param rx_data 接收数据缓冲区
 * @param length 数据长度
 * @return 是否传输成功
 */
bool hal_spi_transmit_receive(spi_channel_t channel, uint8_t *tx_data, uint8_t *rx_data, uint16_t length);

/**
 * @brief 使能SPI
 * @param channel SPI通道
 */
void hal_spi_enable(spi_channel_t channel);

/**
 * @brief 禁用SPI
 * @param channel SPI通道
 */
void hal_spi_disable(spi_channel_t channel);

/**
 * @brief 配置SPI为收发模式
 * @param channel SPI通道
 */
void hal_spi_config_full_duplex(spi_channel_t channel);

/**
 * @brief 配置SPI为仅接收模式
 * @param channel SPI通道
 */
void hal_spi_config_receive_only(spi_channel_t channel);

#endif /* HAL_SPI_H */