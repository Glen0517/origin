/*
 * I2C硬件抽象层实现 - STM32F4系列风格
 */

#include "hal_i2c.h"
#include <string.h>

// STM32F4系列I2C寄存器定义
#define I2C1_BASE            0x40005400UL
#define I2C2_BASE            0x40005800UL
#define I2C3_BASE            0x40005C00UL

// I2C端口基地址查找表
static const uint32_t i2c_base_addresses[] = {
    [I2C_CHANNEL_1] = I2C1_BASE,
    [I2C_CHANNEL_2] = I2C2_BASE,
    [I2C_CHANNEL_3] = I2C3_BASE
};

// 获取I2C实例的宏 - 更安全的实现
#define I2C_GET_INSTANCE(ch)    (((ch) >= I2C_CHANNEL_1 && (ch) <= I2C_CHANNEL_3) ? \
                                ((I2C_TypeDef *)i2c_base_addresses[(ch)]) : NULL)

// I2C寄存器结构体
typedef struct {
    uint32_t CR1;        /*!< I2C Control register 1,                     Address offset: 0x00 */
    uint32_t CR2;        /*!< I2C Control register 2,                     Address offset: 0x04 */
    uint32_t OAR1;       /*!< I2C Own address register 1,                 Address offset: 0x08 */
    uint32_t OAR2;       /*!< I2C Own address register 2,                 Address offset: 0x0C */
    uint32_t DR;         /*!< I2C Data register,                          Address offset: 0x10 */
    uint32_t SR1;        /*!< I2C Status register 1,                      Address offset: 0x14 */
    uint32_t SR2;        /*!< I2C Status register 2,                      Address offset: 0x18 */
    uint32_t CCR;        /*!< I2C Clock control register,                 Address offset: 0x1C */
    uint32_t TRISE;      /*!< I2C TRISE register,                         Address offset: 0x20 */
    uint32_t FLTR;       /*!< I2C FLTR register,                          Address offset: 0x24 */
} I2C_TypeDef;

// I2C实例
#define I2C1        I2C_GET_INSTANCE(I2C_CHANNEL_1)
#define I2C2        I2C_GET_INSTANCE(I2C_CHANNEL_2)
#define I2C3        I2C_GET_INSTANCE(I2C_CHANNEL_3)

// I2C实例数组
static I2C_TypeDef* i2c_instances[I2C_MAX] = {
    NULL,   // I2C_CHANNEL_0 (未使用)
    I2C_GET_INSTANCE(I2C_CHANNEL_1),   // I2C_CHANNEL_1
    I2C_GET_INSTANCE(I2C_CHANNEL_2),   // I2C_CHANNEL_2
    I2C_GET_INSTANCE(I2C_CHANNEL_3)    // I2C_CHANNEL_3
};

// 默认I2C配置
const i2c_config_t I2C_DEFAULT_CONFIG = {
    .mode = I2C_MODE_MASTER,
    .speed_mode = I2C_STANDARD_MODE,
    .address_mode = I2C_ADDRESS_7BIT,
    .own_address = 0,
    .dma_enable = false,
    .interrupt_enable = false
};

// I2C状态结构体
typedef struct {
    i2c_config_t config;
    bool initialized;
    bool enabled;
} i2c_state_t;

// I2C状态数组
static i2c_state_t i2c_states[I2C_MAX] = {0};

// 位定义
#define I2C_CR1_PE           (1U << 0)   /*!< Peripheral Enable */
#define I2C_CR1_SMBUS        (1U << 1)   /*!< SMBus Mode */
#define I2C_CR1_SMBTYPE      (1U << 3)   /*!< SMBus Type */
#define I2C_CR1_ENARP        (1U << 4)   /*!< ARP Enable */
#define I2C_CR1_ENPEC        (1U << 5)   /*!< PEC Enable */
#define I2C_CR1_ENGC         (1U << 6)   /*!< General Call Enable */
#define I2C_CR1_NOSTRETCH    (1U << 7)   /*!< Clock Stretching Disable */
#define I2C_CR1_START        (1U << 8)   /*!< Start Generation */
#define I2C_CR1_STOP         (1U << 9)   /*!< Stop Generation */
#define I2C_CR1_ACK          (1U << 10)  /*!< Acknowledge Enable */
#define I2C_CR1_POS          (1U << 11)  /*!< Acknowledge/PEC Position */
#define I2C_CR1_PEC          (1U << 12)  /*!< Packet Error Checking */
#define I2C_CR1_ALERT        (1U << 13)  /*!< SMBus Alert */
#define I2C_CR1_SWRST        (1U << 15)  /*!< Software Reset */

#define I2C_SR1_SB           (1U << 0)   /*!< Start Bit */
#define I2C_SR1_ADDR         (1U << 1)   /*!< Address Sent */
#define I2C_SR1_BTF          (1U << 2)   /*!< Byte Transfer Finished */
#define I2C_SR1_ADD10        (1U << 3)   /*!< 10-bit Header Sent */
#define I2C_SR1_STOPF        (1U << 4)   /*!< Stop Detection */
#define I2C_SR1_RxNE         (1U << 6)   /*!< Data Register not Empty */
#define I2C_SR1_TxE          (1U << 7)   /*!< Data Register Empty */
#define I2C_SR1_BERR         (1U << 8)   /*!< Bus Error */
#define I2C_SR1_ARLO         (1U << 9)   /*!< Arbitration Lost */
#define I2C_SR1_AF           (1U << 10)  /*!< Acknowledge Failure */
#define I2C_SR1_OVR          (1U << 11)  /*!< Overrun/Underrun */
#define I2C_SR1_PECERR       (1U << 12)  /*!< PEC Error in Reception */
#define I2C_SR1_TIMEOUT      (1U << 14)  /*!< Timeout or Tlow Error */
#define I2C_SR1_SMBALERT     (1U << 15)  /*!< SMBus Alert */

#define I2C_SR2_MSL          (1U << 0)   /*!< Master/Slave */
#define I2C_SR2_BUSY         (1U << 1)   /*!< Bus Busy */
#define I2C_SR2_TRA          (1U << 2)   /*!< Transmitter/Receiver */
#define I2C_SR2_GENCALL      (1U << 4)   /*!< General Call Address (Slave mode) */
#define I2C_SR2_SMBDEFAULT   (1U << 5)   /*!< SMBus Device Default Address (Slave mode) */
#define I2C_SR2_SMBHOST      (1U << 6)   /*!< SMBus Host Header (Slave mode) */
#define I2C_SR2_DUALF        (1U << 7)   /*!< Dual Flag (Slave mode) */
#define I2C_SR2_PEC          (1U << 8)   /*!< Packet Error Checking Register */

#define I2C_CCR_FS           (1U << 15)  /*!< Fast Mode Selection */
#define I2C_CCR_DUTY         (1U << 14)  /*!< Fast Mode Duty Cycle */

/**
 * @brief 初始化I2C接口
 * @param channel I2C通道
 * @param speed I2C速度
 * @return 成功返回true，失败返回false
 */
bool i2c_init(i2c_channel_t channel, i2c_speed_mode_t speed_mode) {
    i2c_config_t config = {
        .speed_mode = speed_mode,
        .mode = I2C_MODE_MASTER
    };
    
    return hal_i2c_init(channel, &config);
}

/**
 * @brief 初始化I2C通道
 */
bool hal_i2c_init(i2c_channel_t channel, i2c_config_t *config) {
    if (channel >= I2C_MAX || config == NULL || i2c_instances[channel] == NULL) {
        return false;
    }
    
    // 保存配置
    i2c_states[channel].config = *config;
    
    // 使能I2C时钟（在实际实现中需要根据平台设置）
    // RCC->APB1ENR |= (1 << 21); // 使能I2C1时钟为例
    
    I2C_TypeDef *i2c = i2c_instances[channel];
    
    // 软件复位
    i2c->CR1 |= I2C_CR1_SWRST;
    i2c->CR1 &= ~I2C_CR1_SWRST;
    
    // 禁用I2C，以便配置
    i2c->CR1 &= ~I2C_CR1_PE;
    
    // 配置I2C时钟频率（假设APB1时钟为42MHz）
    i2c->CR2 = 42; // FREQ[5:0] = 42，表示APB1时钟频率为42MHz
    
    // 配置CCR寄存器
    uint32_t ccr_value = 0;
    
    if (config->speed_mode == I2C_STANDARD_MODE) {
        // 标准模式 (100kHz)
        ccr_value = 210; // 42MHz / (2 * 100kHz) = 210
    } else if (config->speed_mode == I2C_FAST_MODE) {
        // 快速模式 (400kHz)
        ccr_value |= I2C_CCR_FS; // 设置快速模式
        ccr_value |= 52; // 42MHz / (3 * 400kHz) = 35, 但这里用52作为示例
    }
    
    i2c->CCR = ccr_value;
    
    // 配置TRISE寄存器
    if (config->speed_mode == I2C_STANDARD_MODE) {
        i2c->TRISE = 43; // 标准模式：42 + 1
    } else {
        i2c->TRISE = 11; // 快速模式：(42 * 0.0000003) / 0.000000002 + 1 ≈ 11
    }
    
    // 默认启用ACK
    i2c->CR1 |= I2C_CR1_ACK;
    
    // 使能I2C
    i2c->CR1 |= I2C_CR1_PE;
    
    // 标记为已初始化
    i2c_states[channel].initialized = true;
    i2c_states[channel].enabled = true;
    
    return true;
}

/**
 * @brief 使能I2C
 */
void hal_i2c_enable(i2c_channel_t channel) {
    if (channel >= I2C_MAX || !i2c_states[channel].initialized || i2c_instances[channel] == NULL) {
        return;
    }
    
    // 使能I2C
    I2C_TypeDef *i2c = i2c_instances[channel];
    i2c->CR1 |= I2C_CR1_PE;
    
    i2c_states[channel].enabled = true;
}

/**
 * @brief 检查I2C设备是否连接
 */
bool hal_i2c_is_device_connected(i2c_channel_t channel, uint16_t address) {
    if (channel >= I2C_MAX || !i2c_states[channel].initialized || !i2c_states[channel].enabled || i2c_instances[channel] == NULL) {
        return false;
    }
    
    I2C_TypeDef *i2c = i2c_instances[channel];
    uint32_t timeout = 10000;
    
    // 生成起始条件
    i2c->CR1 |= I2C_CR1_START;
    
    // 等待起始条件生成
    while (!(i2c->SR1 & I2C_SR1_SB) && timeout--);
    if (timeout == 0) {
        return false; // 超时
    }
    
    // 发送设备地址
    i2c->DR = (address & 0xFE); // 写入模式
    
    // 等待地址发送完成
    timeout = 10000;
    while (!(i2c->SR1 & I2C_SR1_ADDR) && timeout--);
    
    // 检查是否收到ACK
    bool ack_received = (timeout != 0);
    
    // 清除地址标志
    (void)i2c->SR2;
    
    // 生成停止条件
    i2c->CR1 |= I2C_CR1_STOP;
    
    return ack_received;
}

/**
 * @brief I2C发送数据
 */
bool hal_i2c_transmit(i2c_channel_t channel, uint8_t address, uint8_t *data, uint16_t length, uint32_t timeout) {
    if (channel >= I2C_MAX || !i2c_states[channel].initialized || !i2c_states[channel].enabled || 
        data == NULL || i2c_instances[channel] == NULL) {
        return false;
    }
    
    I2C_TypeDef *i2c = i2c_instances[channel];
    uint32_t start_timeout = timeout;
    
    // 生成起始条件
    i2c->CR1 |= I2C_CR1_START;
    
    // 等待起始条件生成
    while (!(i2c->SR1 & I2C_SR1_SB) && start_timeout--);
    if (start_timeout == 0) {
        return false; // 超时
    }
    
    // 发送设备地址
    i2c->DR = (address & 0xFE); // 写入模式
    
    // 等待地址发送完成
    uint32_t addr_timeout = timeout;
    while (!(i2c->SR1 & I2C_SR1_ADDR) && addr_timeout--);
    if (addr_timeout == 0) {
        return false; // 超时
    }
    
    // 清除地址标志
    (void)i2c->SR2;
    
    // 发送数据
    for (uint16_t i = 0; i < length; i++) {
        // 等待发送缓冲区为空
        uint32_t tx_timeout = timeout;
        while (!(i2c->SR1 & I2C_SR1_TxE) && tx_timeout--);
        if (tx_timeout == 0) {
            return false; // 超时
        }
        
        // 发送数据
        i2c->DR = data[i];
    }
    
    // 等待传输完成
    uint32_t btf_timeout = timeout;
    while (!(i2c->SR1 & I2C_SR1_BTF) && btf_timeout--);
    if (btf_timeout == 0) {
        return false; // 超时
    }
    
    // 生成停止条件
    i2c->CR1 |= I2C_CR1_STOP;
    
    return true;
}

/**
 * @brief I2C接收数据
 */
bool hal_i2c_receive(i2c_channel_t channel, uint8_t address, uint8_t *data, uint16_t length, uint32_t timeout) {
    if (channel >= I2C_MAX || !i2c_states[channel].initialized || !i2c_states[channel].enabled || 
        data == NULL || i2c_instances[channel] == NULL) {
        return false;
    }
    
    I2C_TypeDef *i2c = i2c_instances[channel];
    uint32_t start_timeout = timeout;
    
    // 生成起始条件
    i2c->CR1 |= I2C_CR1_START;
    
    // 等待起始条件生成
    while (!(i2c->SR1 & I2C_SR1_SB) && start_timeout--);
    if (start_timeout == 0) {
        return false; // 超时
    }
    
    // 发送设备地址
    i2c->DR = (address | 0x01); // 读取模式
    
    // 等待地址发送完成
    uint32_t addr_timeout = timeout;
    while (!(i2c->SR1 & I2C_SR1_ADDR) && addr_timeout--);
    if (addr_timeout == 0) {
        return false; // 超时
    }
    
    // 清除地址标志
    (void)i2c->SR2;
    
    // 单个字节接收特殊处理
    if (length == 1) {
        // 禁用ACK
        i2c->CR1 &= ~I2C_CR1_ACK;
        
        // 生成停止条件
        i2c->CR1 |= I2C_CR1_STOP;
        
        // 等待接收数据
        uint32_t rx_timeout = timeout;
        while (!(i2c->SR1 & I2C_SR1_RxNE) && rx_timeout--);
        if (rx_timeout == 0) {
            return false; // 超时
        }
        
        // 读取数据
        data[0] = i2c->DR;
    } 
    // 多个字节接收
    else if (length > 1) {
        for (uint16_t i = 0; i < length; i++) {
            // 等待接收数据
            uint32_t rx_timeout = timeout;
            while (!(i2c->SR1 & I2C_SR1_RxNE) && rx_timeout--);
            if (rx_timeout == 0) {
                return false; // 超时
            }
            
            // 读取数据
            data[i] = i2c->DR;
            
            // 最后一个字节前，生成停止条件并禁用ACK
            if (i == length - 2) {
                // 禁用ACK
                i2c->CR1 &= ~I2C_CR1_ACK;
                
                // 生成停止条件
                i2c->CR1 |= I2C_CR1_STOP;
            }
        }
    }
    
    // 恢复ACK
    i2c->CR1 |= I2C_CR1_ACK;
    
    return true;
}

/**
 * @brief I2C写入寄存器
 */
bool hal_i2c_write_register(i2c_channel_t channel, uint8_t address, uint8_t reg, uint8_t data) {
    uint8_t tx_data[2] = {reg, data};
    return hal_i2c_transmit(channel, address, tx_data, 2, 1000);
}

/**
 * @brief I2C读取寄存器
 */
bool hal_i2c_read_register(i2c_channel_t channel, uint8_t address, uint8_t reg, uint8_t *data) {
    if (!hal_i2c_transmit(channel, address, &reg, 1, 1000)) {
        return false;
    }
    return hal_i2c_receive(channel, address, data, 1, 1000);
}

// 添加缺失的函数实现
bool hal_i2c_write_byte(i2c_channel_t channel, uint16_t dev_address, uint8_t reg_address, uint8_t data) {
    return hal_i2c_write_register(channel, (uint8_t)(dev_address & 0xFF), reg_address, data);
}

bool hal_i2c_read_byte(i2c_channel_t channel, uint16_t dev_address, uint8_t reg_address, uint8_t *data) {
    return hal_i2c_read_register(channel, (uint8_t)(dev_address & 0xFF), reg_address, data);
}

bool hal_i2c_write_data(i2c_channel_t channel, uint16_t dev_address, uint8_t reg_address, uint8_t *data, uint16_t length) {
    if (channel >= I2C_MAX || !i2c_states[channel].initialized || !i2c_states[channel].enabled || 
        data == NULL || i2c_instances[channel] == NULL) {
        return false;
    }
    
    // 先发送寄存器地址
    if (!hal_i2c_transmit(channel, (uint8_t)(dev_address & 0xFF), &reg_address, 1, 1000)) {
        return false;
    }
    
    // 再发送数据
    return hal_i2c_transmit(channel, (uint8_t)(dev_address & 0xFF), data, length, 1000);
}

bool hal_i2c_read_data(i2c_channel_t channel, uint16_t dev_address, uint8_t reg_address, uint8_t *data, uint16_t length) {
    if (channel >= I2C_MAX || !i2c_states[channel].initialized || !i2c_states[channel].enabled || 
        data == NULL || i2c_instances[channel] == NULL) {
        return false;
    }
    
    // 先发送寄存器地址
    if (!hal_i2c_transmit(channel, (uint8_t)(dev_address & 0xFF), &reg_address, 1, 1000)) {
        return false;
    }
    
    // 再读取数据
    return hal_i2c_receive(channel, (uint8_t)(dev_address & 0xFF), data, length, 1000);
}