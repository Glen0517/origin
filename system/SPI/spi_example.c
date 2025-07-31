/**
 * SPI模块示例代码
 * 展示如何使用HAL库模式的SPI模块
 */
#include "spi.h"
#include "stm32f4xx_hal.h"

// SPI句柄定义
SPI_HandleTypeDef hspi1;

// SPI设备定义
SPIDevice_Struct spi_device;

/**
 * SPI1初始化函数
 */
void SPI1_Init(void) {
    // SPI1参数配置
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES_FULLDUPLEX;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;

    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        // 初始化错误处理
        Error_Handler();
    }

    // 初始化SPI设备
    SPI_InitDevice(&spi_device, &hspi1, GPIOA, GPIO_PIN_4);
}

/**
 * SPI MSP初始化回调函数
 * 由HAL_SPI_Init函数调用
 */
void HAL_SPI_MspInit(SPI_HandleTypeDef* spiHandle) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(spiHandle->Instance==SPI1) {
        // 使能SPI1时钟
        __HAL_RCC_SPI1_CLK_ENABLE();

        // 使能GPIOA时钟
        __HAL_RCC_GPIOA_CLK_ENABLE();

        // 配置SPI1引脚: SCK(PA5), MISO(PA6), MOSI(PA7)
        GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

/**
 * SPI示例函数 - 发送和接收数据
 */
void SPI_Example(void) {
    uint8_t tx_data[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t rx_data[4] = {0};

    // 发送数据
    SPI_Transmit(&spi_device, tx_data, 4, 1000);

    // 接收数据
    SPI_Receive(&spi_device, rx_data, 4, 1000);

    // 发送并接收数据
    SPI_TransmitReceive(&spi_device, tx_data, rx_data, 4, 1000);
}

/**
 * 错误处理函数
 */
void Error_Handler(void) {
    // 可以在这里添加错误处理代码
    while(1) {
        // 错误循环
    }
}