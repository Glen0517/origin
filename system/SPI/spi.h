#ifndef SPI_LIBRARY_H
#ifndef SPI_LIBRARY_H
#define SPI_LIBRARY_H

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_spi.h"

/* SPI设备结构体 */
typedef struct {
    SPI_HandleTypeDef *hspi;    // SPI句柄
    GPIO_TypeDef *cs_port;      // CS引脚端口
    uint16_t cs_pin;            // CS引脚编号
} SPIDevice_Struct;

/* 函数声明 */
HAL_StatusTypeDef SPI_InitDevice(SPIDevice_Struct *device, SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin);
HAL_StatusTypeDef SPI_DeInitDevice(SPIDevice_Struct *device);
HAL_StatusTypeDef SPI_TransmitByte(SPIDevice_Struct *device, uint8_t data);
HAL_StatusTypeDef SPI_ReceiveByte(SPIDevice_Struct *device, uint8_t *data);
HAL_StatusTypeDef SPI_TransmitReceiveByte(SPIDevice_Struct *device, uint8_t tx_data, uint8_t *rx_data);
HAL_StatusTypeDef SPI_Transmit(SPIDevice_Struct *device, uint8_t *pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef SPI_Receive(SPIDevice_Struct *device, uint8_t *pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef SPI_TransmitReceive(SPIDevice_Struct *device, uint8_t *pTxData, uint8_t *pRxData, uint16_t Size, uint32_t Timeout);
void SPI_SelectDevice(SPIDevice_Struct *device);
void SPI_DeselectDevice(SPIDevice_Struct *device);

#endif /* SPI_LIBRARY_H */


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