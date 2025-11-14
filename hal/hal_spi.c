/*
 * SPI硬件抽象层实现 - STM32F4系列风格
 */

#include "hal_spi.h"
#include "../include/types.h"
#include "../platform/platform.h"

// SPI波特率分频定义
#define SPI_BAUDRATE_DIV_2     0x0000
#define SPI_BAUDRATE_DIV_4     0x0008
#define SPI_BAUDRATE_DIV_8     0x0010
#define SPI_BAUDRATE_DIV_16    0x0018
#define SPI_BAUDRATE_DIV_32    0x0020
#define SPI_BAUDRATE_DIV_64    0x0028
#define SPI_BAUDRATE_DIV_128   0x0030
#define SPI_BAUDRATE_DIV_256   0x0038

// 时钟极性别名
#define SPI_CLOCK_POLARITY_HIGH  SPI_CPOL_HIGH

// 时钟相位别名
#define SPI_CLOCK_PHASE_2EDGE    SPI_CPHA_2EDGE

// 首位移位别名
#define SPI_FIRST_BIT_LSB        1
#define SPI_FIRST_BIT_MSB        0

// STM32F4系列SPI寄存器定义
#define SPI1_BASE            0x40013000UL
#define SPI2_BASE            0x40003800UL
#define SPI3_BASE            0x40003C00UL

// SPI端口基地址查找表
static const uint32_t spi_base_addresses[] = {
    [SPI_CHANNEL_1] = SPI1_BASE,
    [SPI_CHANNEL_2] = SPI2_BASE,
    [SPI_CHANNEL_3] = SPI3_BASE
};

// SPI寄存器结构体
typedef struct {
    uint32_t CR1;        /*!< SPI control register 1,                     Address offset: 0x00 */
    uint32_t CR2;        /*!< SPI control register 2,                     Address offset: 0x04 */
    uint32_t SR;         /*!< SPI status register,                        Address offset: 0x08 */
    uint32_t DR;         /*!< SPI data register,                          Address offset: 0x0C */
    uint32_t CRCPR;      /*!< SPI CRC polynomial register,                Address offset: 0x10 */
    uint32_t RXCRCR;     /*!< SPI Rx CRC register,                        Address offset: 0x14 */
    uint32_t TXCRCR;     /*!< SPI Tx CRC register,                        Address offset: 0x18 */
    uint32_t I2SCFGR;    /*!< SPI_I2S configuration register,             Address offset: 0x1C */
    uint32_t I2SPR;      /*!< SPI_I2S prescaler register,                 Address offset: 0x20 */
} SPI_TypeDef;

// 获取SPI实例的宏 - 更安全的实现
#define SPI_GET_INSTANCE(ch)    (((ch) >= SPI_CHANNEL_1 && (ch) <= SPI_CHANNEL_3) ? \
                                ((SPI_TypeDef *)spi_base_addresses[(ch)]) : NULL)

// SPI实例数组
static SPI_TypeDef* spi_instances[SPI_MAX] = {
    NULL,   // SPI_CHANNEL_0 (未使用)
    SPI_GET_INSTANCE(SPI_CHANNEL_1),   // SPI_CHANNEL_1
    SPI_GET_INSTANCE(SPI_CHANNEL_2),   // SPI_CHANNEL_2
    SPI_GET_INSTANCE(SPI_CHANNEL_3)    // SPI_CHANNEL_3
};

// SPI状态结构体
typedef struct {
    spi_config_t config;
    bool initialized;
    bool enabled;
} spi_state_t;

// SPI状态数组
static spi_state_t spi_states[SPI_MAX] = {0};

// 位定义
#define SPI_CR1_CPHA         (1U << 0)   /*!< Clock Phase */
#define SPI_CR1_CPOL         (1U << 1)   /*!< Clock Polarity */
#define SPI_CR1_MSTR         (1U << 2)   /*!< Master Selection */
#define SPI_CR1_BR_0         (1U << 3)   /*!< Baud Rate Control Bit 0 */
#define SPI_CR1_BR_1         (1U << 4)   /*!< Baud Rate Control Bit 1 */
#define SPI_CR1_BR_2         (1U << 5)   /*!< Baud Rate Control Bit 2 */
#define SPI_CR1_SPE          (1U << 6)   /*!< SPI Enable */
#define SPI_CR1_LSBFIRST     (1U << 7)   /*!< Frame Format */
#define SPI_CR1_SSI          (1U << 8)   /*!< Internal Slave Select */
#define SPI_CR1_SSM          (1U << 9)   /*!< Software Slave Management */
#define SPI_CR1_RXONLY       (1U << 10)  /*!< Receive only */
#define SPI_CR1_DFF          (1U << 11)  /*!< Data Frame Format */
#define SPI_CR1_CRCNEXT      (1U << 12)  /*!< CRC Transfer Next */
#define SPI_CR1_CRCEN        (1U << 13)  /*!< Hardware CRC calculation enable */
#define SPI_CR1_BIDIOE       (1U << 14)  /*!< Output enable in bidirectional mode */
#define SPI_CR1_BIDIMODE     (1U << 15)  /*!< Bidirectional data mode enable */

#define SPI_SR_RXNE          (1U << 0)   /*!< Receive buffer not empty */
#define SPI_SR_TXE           (1U << 1)   /*!< Transmit buffer empty */
#define SPI_SR_CHSIDE        (1U << 2)   /*!< Channel side */
#define SPI_SR_UDR           (1U << 3)   /*!< Underrun flag */
#define SPI_SR_CRCERR        (1U << 4)   /*!< CRC error flag */
#define SPI_SR_MODF          (1U << 5)   /*!< Mode fault */
#define SPI_SR_OVR           (1U << 6)   /*!< Overrun flag */
#define SPI_SR_BSY           (1U << 7)   /*!< Busy flag */
#define SPI_SR_FRE           (1U << 8)   /*!< Frame format error */

/**
 * @brief 初始化SPI
 */
bool hal_spi_init(spi_channel_t channel, spi_config_t *config) {
    if (channel >= SPI_MAX || config == NULL || spi_instances[channel] == NULL) {
        return false;
    }
    
    // 保存配置
    spi_states[channel].config = *config;
    
    // 使能SPI时钟（在实际实现中需要根据平台设置）
    // RCC->APB2ENR |= (1 << 12); // 使能SPI1时钟为例
    
    // 禁用SPI，以便配置
    SPI_TypeDef *spi = spi_instances[channel];
    spi->CR1 &= ~SPI_CR1_SPE;
    
    // 配置SPI模式
    uint32_t cr1_value = 0;
    
    // 配置主/从模式
    if (config->mode == SPI_MODE_MASTER) {
        cr1_value |= SPI_CR1_MSTR; // 主模式
        
        // 根据clock_speed计算波特率分频
    // 假设系统时钟为168MHz (STM32F4典型值)
    uint32_t sys_clock = 168000000;
    uint32_t divider = sys_clock / (2 * config->clock_speed);
    
    // 选择最接近的分频值
    if (divider <= 2) {
        cr1_value |= SPI_BAUDRATE_DIV_2;
    } else if (divider <= 4) {
        cr1_value |= SPI_BAUDRATE_DIV_4;
    } else if (divider <= 8) {
        cr1_value |= SPI_BAUDRATE_DIV_8;
    } else if (divider <= 16) {
        cr1_value |= SPI_BAUDRATE_DIV_16;
    } else if (divider <= 32) {
        cr1_value |= SPI_BAUDRATE_DIV_32;
    } else if (divider <= 64) {
        cr1_value |= SPI_BAUDRATE_DIV_64;
    } else if (divider <= 128) {
        cr1_value |= SPI_BAUDRATE_DIV_128;
    } else {
        cr1_value |= SPI_BAUDRATE_DIV_256;
    }
    }
    
    // 配置时钟极性和相位
    if (config->cpol == SPI_CPOL_HIGH) {
        cr1_value |= SPI_CR1_CPOL;
    }
    
    if (config->cpha == SPI_CPHA_2EDGE) {
        cr1_value |= SPI_CR1_CPHA;
    }
    
    // 配置数据位长度
    if (config->data_size == SPI_DATA_SIZE_16BIT) {
        cr1_value |= SPI_CR1_DFF;
    }
    
    // STM32默认是MSB先行，符合大多数SPI设备的要求
    // 此处不需要额外配置
    
    // 配置软件从设备管理
    cr1_value |= SPI_CR1_SSM | SPI_CR1_SSI; // 使用软件NSS
    
    // 配置数据方向
    if (config->direction == SPI_DIRECTION_2LINES_RXONLY) {
        cr1_value |= SPI_CR1_RXONLY;
    }
    
    // 写入配置
    spi->CR1 = cr1_value;
    
    // 标记为已初始化
    spi_states[channel].initialized = true;
    
    // 默认禁用
    spi_states[channel].enabled = false;
    
    return true;
}

/**
 * @brief 发送单个数据
 */
bool hal_spi_transmit_byte(spi_channel_t channel, uint8_t data) {
    if (channel >= SPI_MAX || !spi_states[channel].initialized || !spi_states[channel].enabled || spi_instances[channel] == NULL) {
        return false;
    }
    
    SPI_TypeDef *spi = spi_instances[channel];
    
    // 等待发送缓冲区为空
    while (!(spi->SR & SPI_SR_TXE));
    
    // 发送数据
    if (spi_states[channel].config.data_size == SPI_DATA_SIZE_16BIT) {
        spi->DR = ((uint16_t)data) & 0xFF;
    } else {
        spi->DR = data;
    }
    
    // 等待传输完成
    while (spi->SR & SPI_SR_BSY);
    
    return true;
}

/**
 * @brief 接收单个数据
 */
bool hal_spi_receive_byte(spi_channel_t channel, uint8_t *data) {
    if (channel >= SPI_MAX || !spi_states[channel].initialized || !spi_states[channel].enabled || data == NULL || spi_instances[channel] == NULL) {
        return false;
    }
    
    SPI_TypeDef *spi = spi_instances[channel];
    
    // 发送一个dummy数据以启动接收
    if (spi_states[channel].config.mode == SPI_MODE_MASTER) {
        while (!(spi->SR & SPI_SR_TXE));
        spi->DR = 0xFF; // dummy数据
    }
    
    // 等待接收完成
    while (!(spi->SR & SPI_SR_RXNE));
    
    // 读取数据
    if (spi_states[channel].config.data_size == SPI_DATA_SIZE_16BIT) {
        *data = (uint8_t)(spi->DR & 0xFF);
    } else {
        *data = (uint8_t)spi->DR;
    }
    
    return true;
}

/**
 * @brief 发送数据
 */
bool hal_spi_transmit(spi_channel_t channel, uint8_t *data, uint16_t length) {
    if (channel >= SPI_MAX || !spi_states[channel].initialized || !spi_states[channel].enabled || data == NULL || spi_instances[channel] == NULL) {
        return false;
    }
    
    SPI_TypeDef *spi = spi_instances[channel];
    
    for (uint16_t i = 0; i < length; i++) {
        // 等待发送缓冲区为空
        while (!(spi->SR & SPI_SR_TXE));
        
        // 发送数据
        if (spi_states[channel].config.data_size == SPI_DATA_SIZE_16BIT) {
            spi->DR = ((uint16_t)data[i]) & 0xFF;
        } else {
            spi->DR = data[i];
        }
    }
    
    // 等待传输完成
    while (spi->SR & SPI_SR_BSY);
    
    return true;
}

/**
 * @brief 接收数据
 */
bool hal_spi_receive(spi_channel_t channel, uint8_t *data, uint16_t length) {
    if (channel >= SPI_MAX || !spi_states[channel].initialized || !spi_states[channel].enabled || data == NULL || spi_instances[channel] == NULL) {
        return false;
    }
    
    SPI_TypeDef *spi = spi_instances[channel];
    
    for (uint16_t i = 0; i < length; i++) {
        // 发送一个dummy数据以启动接收（主模式）
        if (spi_states[channel].config.mode == SPI_MODE_MASTER) {
            while (!(spi->SR & SPI_SR_TXE));
            spi->DR = 0xFF; // dummy数据
        }
        
        // 等待接收完成
        while (!(spi->SR & SPI_SR_RXNE));
        
        // 读取数据
        if (spi_states[channel].config.data_size == SPI_DATA_SIZE_16BIT) {
            data[i] = (uint8_t)(spi->DR & 0xFF);
        } else {
            data[i] = (uint8_t)spi->DR;
        }
    }
    
    return true;
}

/**
 * @brief 全双工传输
 */
bool hal_spi_transmit_receive(spi_channel_t channel, uint8_t *tx_data, uint8_t *rx_data, uint16_t length) {
    if (channel >= SPI_MAX || !spi_states[channel].initialized || !spi_states[channel].enabled || 
        tx_data == NULL || rx_data == NULL || spi_instances[channel] == NULL) {
        return false;
    }
    
    SPI_TypeDef *spi = spi_instances[channel];
    
    for (uint16_t i = 0; i < length; i++) {
        // 等待发送缓冲区为空
        while (!(spi->SR & SPI_SR_TXE));
        
        // 发送数据
        if (spi_states[channel].config.data_size == SPI_DATA_SIZE_16BIT) {
            spi->DR = ((uint16_t)tx_data[i]) & 0xFF;
        } else {
            spi->DR = tx_data[i];
        }
        
        // 等待接收完成
        while (!(spi->SR & SPI_SR_RXNE));
        
        // 读取数据
        if (spi_states[channel].config.data_size == SPI_DATA_SIZE_16BIT) {
            rx_data[i] = (uint8_t)(spi->DR & 0xFF);
        } else {
            rx_data[i] = (uint8_t)spi->DR;
        }
    }
    
    // 等待传输完成
    while (spi->SR & SPI_SR_BSY);
    
    return true;
}

/**
 * @brief 使能SPI
 */
void hal_spi_enable(spi_channel_t channel) {
    if (channel >= SPI_MAX || !spi_states[channel].initialized || spi_instances[channel] == NULL) {
        return;
    }
    
    // 使能SPI
    SPI_TypeDef *spi = spi_instances[channel];
    spi->CR1 |= SPI_CR1_SPE;
    
    spi_states[channel].enabled = true;
}

/**
 * @brief 禁用SPI
 */
void hal_spi_disable(spi_channel_t channel) {
    if (channel >= SPI_MAX || !spi_states[channel].initialized || spi_instances[channel] == NULL) {
        return;
    }
    
    // 禁用SPI
    SPI_TypeDef *spi = spi_instances[channel];
    spi->CR1 &= ~SPI_CR1_SPE;
    
    spi_states[channel].enabled = false;
}

/**
 * @brief 配置SPI为收发模式
 */
void hal_spi_config_full_duplex(spi_channel_t channel) {
    if (channel >= SPI_MAX || !spi_states[channel].initialized || spi_instances[channel] == NULL) {
        return;
    }
    
    // 禁用SPI以修改配置
    SPI_TypeDef *spi = spi_instances[channel];
    spi->CR1 &= ~SPI_CR1_SPE;
    
    // 配置为全双工模式
    spi->CR1 &= ~SPI_CR1_RXONLY;
    
    // 更新配置
    spi_states[channel].config.direction = SPI_DIRECTION_2LINES;
    
    // 如果之前是使能的，则重新使能
    if (spi_states[channel].enabled) {
        spi->CR1 |= SPI_CR1_SPE;
    }
}

/**
 * @brief 配置SPI为仅接收模式
 */
void hal_spi_config_receive_only(spi_channel_t channel) {
    if (channel >= SPI_MAX || !spi_states[channel].initialized || spi_instances[channel] == NULL) {
        return;
    }
    
    // 禁用SPI以修改配置
    SPI_TypeDef *spi = spi_instances[channel];
    spi->CR1 &= ~SPI_CR1_SPE;
    
    // 配置为仅接收模式
    spi->CR1 |= SPI_CR1_RXONLY;
    
    // 更新配置
    spi_states[channel].config.direction = SPI_DIRECTION_2LINES_RXONLY;
    
    // 如果之前是使能的，则重新使能
    if (spi_states[channel].enabled) {
        spi->CR1 |= SPI_CR1_SPE;
    }
}