/**
 * SPI库函数实现 - 支持配置和基本通信操作
 * 适用于嵌入式系统，使用寄存器操作方式
 */

#include "spi.h"

void SPI_Init(SPIConfig_Struct *config) {
    SPIRegisters_Struct *spi = (SPIRegisters_Struct *)config->baseAddress;
    
    // 禁用SPI进行配置
    spi->CR1 &= ~SPI_CR1_SPE;
    
    // 配置SPI模式(CPOL, CPHA)
    if (config->mode == SPI_MODE0) {
        spi->CR1 &= ~(SPI_CR1_CPOL | SPI_CR1_CPHA);
    } else if (config->mode == SPI_MODE1) {
        spi->CR1 &= ~SPI_CR1_CPOL;
        spi->CR1 |= SPI_CR1_CPHA;
    } else if (config->mode == SPI_MODE2) {
        spi->CR1 |= SPI_CR1_CPOL;
        spi->CR1 &= ~SPI_CR1_CPHA;
    } else if (config->mode == SPI_MODE3) {
        spi->CR1 |= SPI_CR1_CPOL | SPI_CR1_CPHA;
    }
    
    // 配置主从模式
    if (config->masterMode) {
        spi->CR1 |= SPI_CR1_MSTR;
        // 主模式需要软件管理NSS
        spi->CR1 |= SPI_CR1_SSM | SPI_CR1_SSI;
    } else {
        spi->CR1 &= ~SPI_CR1_MSTR;
    }
    
    // 配置波特率预分频器
    spi->CR1 = (spi->CR1 & ~SPI_CR1_BR_MASK) | 
              (config->prescaler << SPI_CR1_BR_POS);
    
    // 配置数据位宽
    if (config->dataSize == SPI_DATA_16BIT) {
        spi->CR1 |= SPI_CR1_DFF;
    } else {
        spi->CR1 &= ~SPI_CR1_DFF;
    }
    
    // 配置LSB/MSB优先
    if (config->lsbFirst) {
        spi->CR1 |= SPI_CR1_LSBFIRST;
    } else {
        spi->CR1 &= ~SPI_CR1_LSBFIRST;
    }
    
    // 其他配置(可以根据需要扩展)
}

void SPI_Enable(uint32_t baseAddress) {
    SPIRegisters_Struct *spi = (SPIRegisters_Struct *)baseAddress;
    spi->CR1 |= SPI_CR1_SPE;
}

void SPI_Disable(uint32_t baseAddress) {
    SPIRegisters_Struct *spi = (SPIRegisters_Struct *)baseAddress;
    spi->CR1 &= ~SPI_CR1_SPE;
}

uint8_t SPI_TransferByte(uint32_t baseAddress, uint8_t data) {
    SPIRegisters_Struct *spi = (SPIRegisters_Struct *)baseAddress;
    
    // 等待发送缓冲区为空
    while (!(spi->SR & SPI_SR_TXE));
    
    // 发送数据
    spi->DR = data;
    
    // 等待接收缓冲区非空
    while (!(spi->SR & SPI_SR_RXNE));
    
    // 返回接收到的数据
    return (uint8_t)spi->DR;
}

void SPI_Transfer(uint32_t baseAddress, uint8_t *txBuffer, uint8_t *rxBuffer, uint32_t length) {
    SPIRegisters_Struct *spi = (SPIRegisters_Struct *)baseAddress;
    uint32_t i;
    
    for (i = 0; i < length; i++) {
        // 等待发送缓冲区为空
        while (!(spi->SR & SPI_SR_TXE));
        
        // 发送数据
        if (txBuffer != NULL) {
            spi->DR = txBuffer[i];
        } else {
            spi->DR = 0xFF; // 发送默认值
        }
        
        // 等待接收缓冲区非空
        while (!(spi->SR & SPI_SR_RXNE));
        
        // 保存接收到的数据
        if (rxBuffer != NULL) {
            rxBuffer[i] = (uint8_t)spi->DR;
        } else {
            // 忽略接收到的数据
            (void)spi->DR;
        }
    }
}

uint8_t SPI_IsBusy(uint32_t baseAddress) {
    SPIRegisters_Struct *spi = (SPIRegisters_Struct *)baseAddress;
    return (spi->SR & SPI_SR_BSY) != 0;
}

//STM32库函数例程
#if 0
/**
 * STM32 SPI通信示例 - 基于标准外设库
 * 包含主模式和从模式初始化及数据传输
 */

#include "stm32f10x.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

/**
 * 配置SPI1为主模式
 */
void SPI1_Master_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef SPI_InitStructure;
    
    // 使能SPI1和GPIOA时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1 | RCC_APB2Periph_GPIOA, ENABLE);
    
    // 配置SPI1引脚: SCK(PA5), MISO(PA6), MOSI(PA7)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  // 复用推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;  // 浮空输入
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // 配置SPI1为全双工主模式
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;  // 软件管理NSS
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStructure);
    
    // 使能SPI1
    SPI_Cmd(SPI1, ENABLE);
}

/**
 * 配置SPI2为从模式
 */
void SPI2_Slave_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef SPI_InitStructure;
    
    // 使能SPI2和GPIOB时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    // 配置SPI2引脚: SCK(PB13), MISO(PB14), MOSI(PB15), NSS(PB12)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;  // 输入模式
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  // 复用推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // 配置SPI2为全双工从模式
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Slave;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Hard;  // 硬件管理NSS
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI2, &SPI_InitStructure);
    
    // 使能SPI2
    SPI_Cmd(SPI2, ENABLE);
}

/**
 * SPI发送并接收一个字节
 * @param SPIx SPI外设
 * @param data 要发送的数据
 * @return 接收到的数据
 */
uint8_t SPI_SendByte(SPI_TypeDef* SPIx, uint8_t data) {
    // 等待发送缓冲区为空
    while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_TXE) == RESET);
    
    // 发送数据
    SPI_I2S_SendData(SPIx, data);
    
    // 等待接收缓冲区非空
    while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_RXNE) == RESET);
    
    // 返回接收到的数据
    return SPI_I2S_ReceiveData(SPIx);
}

/**
 * SPI发送多个字节
 * @param SPIx SPI外设
 * @param txBuffer 发送缓冲区
 * @param length 发送长度
 */
void SPI_SendBuffer(SPI_TypeDef* SPIx, uint8_t* txBuffer, uint32_t length) {
    uint32_t i;
    for (i = 0; i < length; i++) {
        SPI_SendByte(SPIx, txBuffer[i]);
    }
}

/**
 * SPI接收多个字节
 * @param SPIx SPI外设
 * @param rxBuffer 接收缓冲区
 * @param length 接收长度
 */
void SPI_ReceiveBuffer(SPI_TypeDef* SPIx, uint8_t* rxBuffer, uint32_t length) {
    uint32_t i;
    for (i = 0; i < length; i++) {
        rxBuffer[i] = SPI_SendByte(SPIx, 0xFF);  // 发送虚拟数据以触发接收
    }
}

/**
 * 主函数示例
 */
int main(void) {
    uint8_t txData[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t rxData[4] = {0};
    
    // 初始化SPI1为主模式
    SPI1_Master_Init();
    
    // 初始化SPI2为从模式 (如果使用外部从设备，此步骤可省略)
    SPI2_Slave_Init();
    
    while (1) {
        // 主设备发送数据
        SPI_SendBuffer(SPI1, txData, 4);
        
        // 主设备接收数据
        SPI_ReceiveBuffer(SPI1, rxData, 4);
        
        // 延时
        for(volatile int i = 0; i < 1000000; i++);
    }
}    
#endif

//SPI烧录flash例程
#if 0
#include "spi_flash.h"
#include "delay.h"  // 需自行实现延时函数（如毫秒级延时）

/**
 * 初始化SPI和Flash引脚
 * - 配置SPI1为主模式，引脚：SCK(PA5)、MOSI(PA7)、MISO(PA6)、CS(PA4)
 */
void SPI_Flash_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef SPI_InitStructure;

    // 使能时钟
    RCC_APB2PeriphClockCmd(FLASH_SPI_CLK | FLASH_GPIO_CLK, ENABLE);

    // 配置CS引脚（推挽输出）
    GPIO_InitStructure.GPIO_Pin = FLASH_CS_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(FLASH_GPIO_PORT, &GPIO_InitStructure);
    FLASH_CS_HIGH();  // 初始拉高片选（不选中）

    // 配置SPI引脚：SCK、MOSI推挽复用；MISO浮空输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;  // SCK(PA5)、MOSI(PA7)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(FLASH_GPIO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;  // MISO(PA6)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(FLASH_GPIO_PORT, &GPIO_InitStructure);

    // 配置SPI参数
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;  // 全双工
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;                       // 主模式
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;                   // 8位数据
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;                         // 时钟极性（根据Flash手册，W25Q默认CPOL=1）
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;                        // 时钟相位（CPHA=1）
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;                           // 软件控制片选
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;   // 分频（APB2=72MHz，SPI=36MHz）
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;                   // 高位在前
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(FLASH_SPI, &SPI_InitStructure);

    // 使能SPI
    SPI_Cmd(FLASH_SPI, ENABLE);
}

/**
 * SPI单字节收发
 * @param data：发送的字节
 * @return 接收的字节
 */
static uint8_t SPI_Flash_TransferByte(uint8_t data) {
    // 等待发送缓冲区为空
    while (SPI_I2S_GetFlagStatus(FLASH_SPI, SPI_I2S_FLAG_TXE) == RESET);
    // 发送数据
    SPI_I2S_SendData(FLASH_SPI, data);
    // 等待接收缓冲区非空
    while (SPI_I2S_GetFlagStatus(FLASH_SPI, SPI_I2S_FLAG_RXNE) == RESET);
    // 返回接收数据
    return SPI_I2S_ReceiveData(FLASH_SPI);
}

/**
 * 读取Flash ID（厂商ID+设备ID）
 * @return 3字节ID（如W25Q128返回0XEF4018）
 */
uint32_t SPI_Flash_ReadID(void) {
    uint32_t id = 0;
    FLASH_CS_LOW();               // 选中Flash
    SPI_Flash_TransferByte(W25Q_JEDEC_ID);  // 发送读ID指令
    id |= (SPI_Flash_TransferByte(0xFF) << 16);  // 厂商ID（第1字节）
    id |= (SPI_Flash_TransferByte(0xFF) << 8);   // 设备ID高8位
    id |= SPI_Flash_TransferByte(0xFF);          // 设备ID低8位
    FLASH_CS_HIGH();              // 取消选中
    return id;
}

/**
 * 写使能（Flash写入/擦除前必须执行）
 */
void SPI_Flash_WriteEnable(void) {
    FLASH_CS_LOW();
    SPI_Flash_TransferByte(W25Q_WRITE_ENABLE);  // 发送写使能指令
    FLASH_CS_HIGH();
}

/**
 * 读状态寄存器1
 * @return 状态值（bit0为忙标志）
 */
uint8_t SPI_Flash_ReadStatus(void) {
    uint8_t status;
    FLASH_CS_LOW();
    SPI_Flash_TransferByte(W25Q_READ_STATUS1);  // 发送读状态指令
    status = SPI_Flash_TransferByte(0xFF);      // 读取状态
    FLASH_CS_HIGH();
    return status;
}

/**
 * 等待Flash空闲（忙标志位为0）
 */
void SPI_Flash_WaitBusy(void) {
    while ((SPI_Flash_ReadStatus() & W25Q_BUSY_BIT) != 0);  // 循环等待忙标志清零
}

/**
 * 扇区擦除（4KB）
 * @param addr：扇区起始地址（必须对齐4KB）
 */
void SPI_Flash_SectorErase(uint32_t addr) {
    SPI_Flash_WriteEnable();      // 写使能
    FLASH_CS_LOW();
    SPI_Flash_TransferByte(W25Q_SECTOR_ERASE);  // 发送扇区擦除指令
    // 发送3字节地址（高位在前）
    SPI_Flash_TransferByte((addr >> 16) & 0xFF);
    SPI_Flash_TransferByte((addr >> 8) & 0xFF);
    SPI_Flash_TransferByte(addr & 0xFF);
    FLASH_CS_HIGH();
    SPI_Flash_WaitBusy();         // 等待擦除完成（约40ms）
}

/**
 * 块擦除（64KB）
 * @param addr：块起始地址（必须对齐64KB）
 */
void SPI_Flash_BlockErase(uint32_t addr) {
    SPI_Flash_WriteEnable();
    FLASH_CS_LOW();
    SPI_Flash_TransferByte(W25Q_BLOCK_ERASE);  // 发送块擦除指令
    // 发送3字节地址
    SPI_Flash_TransferByte((addr >> 16) & 0xFF);
    SPI_Flash_TransferByte((addr >> 8) & 0xFF);
    SPI_Flash_TransferByte(addr & 0xFF);
    FLASH_CS_HIGH();
    SPI_Flash_WaitBusy();  // 等待擦除完成（约150ms）
}

/**
 * 整片擦除（谨慎使用！耗时较长，约20秒）
 */
void SPI_Flash_ChipErase(void) {
    SPI_Flash_WriteEnable();
    FLASH_CS_LOW();
    SPI_Flash_TransferByte(W25Q_CHIP_ERASE);  // 发送整片擦除指令
    FLASH_CS_HIGH();
    SPI_Flash_WaitBusy();  // 等待擦除完成
}

/**
 * 页写入（单次最多写256字节，地址需对齐页）
 * @param buf：待写入数据缓冲区
 * @param addr：写入起始地址（必须在同一页内）
 * @param len：写入长度（≤256）
 */
void SPI_Flash_WritePage(uint8_t *buf, uint32_t addr, uint16_t len) {
    if (len == 0 || len > PAGE_SIZE) return;  // 长度检查

    SPI_Flash_WriteEnable();  // 写使能
    FLASH_CS_LOW();
    SPI_Flash_TransferByte(W25Q_PAGE_PROGRAM);  // 发送页编程指令
    // 发送3字节地址
    SPI_Flash_TransferByte((addr >> 16) & 0xFF);
    SPI_Flash_TransferByte((addr >> 8) & 0xFF);
    SPI_Flash_TransferByte(addr & 0xFF);
    // 发送数据
    for (uint16_t i = 0; i < len; i++) {
        SPI_Flash_TransferByte(buf[i]);
    }
    FLASH_CS_HIGH();
    SPI_Flash_WaitBusy();  // 等待写入完成
}

/**
 * 连续写入（跨页自动分块）
 * @param buf：待写入数据缓冲区
 * @param addr：写入起始地址
 * @param len：写入总长度
 */
void SPI_Flash_Write(uint8_t *buf, uint32_t addr, uint32_t len) {
    uint16_t page_remain;  // 当前页剩余可写长度
    while (len > 0) {
        // 计算当前页剩余空间（地址到页尾的长度）
        page_remain = PAGE_SIZE - (addr % PAGE_SIZE);
        if (len <= page_remain) {
            page_remain = len;  // 剩余长度小于一页
        }
        // 写入当前页数据
        SPI_Flash_WritePage(buf, addr, page_remain);
        // 更新参数：地址后移，长度减少，缓冲区后移
        addr += page_remain;
        len -= page_remain;
        buf += page_remain;
    }
}

/**
 * 连续读取
 * @param buf：接收数据缓冲区
 * @param addr：读取起始地址
 * @param len：读取总长度
 */
void SPI_Flash_Read(uint8_t *buf, uint32_t addr, uint32_t len) {
    FLASH_CS_LOW();
    SPI_Flash_TransferByte(W25Q_READ_DATA);  // 发送读数据指令
    // 发送3字节地址
    SPI_Flash_TransferByte((addr >> 16) & 0xFF);
    SPI_Flash_TransferByte((addr >> 8) & 0xFF);
    SPI_Flash_TransferByte(addr & 0xFF);
    // 读取数据
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = SPI_Flash_TransferByte(0xFF);  // 发送 dummy 字节以读取数据
    }
    FLASH_CS_HIGH();
}
#endif

//使用实例
#if 0
#include "stm32f10x.h"
#include "spi_flash.h"
#include "delay.h"  // 需实现毫秒级延时函数（如SysTick）

int main(void) {
    uint32_t flash_id;
    uint8_t tx_buf[256], rx_buf[256];
    uint32_t addr = 0x000000;  // 写入起始地址

    // 初始化延时函数（需自行实现）
    Delay_Init();
    // 初始化SPI Flash
    SPI_Flash_Init();

    // 读取Flash ID并验证
    flash_id = SPI_Flash_ReadID();
    if (flash_id != W25Q128_ID) {
        // ID错误，可添加错误处理（如点灯提示）
        while (1);
    }

    // 准备测试数据（填充0x00~0xFF）
    for (uint16_t i = 0; i < 256; i++) {
        tx_buf[i] = i;
    }

    // 擦除目标扇区（必须先擦除再写入，Flash默认值为0xFF）
    SPI_Flash_SectorErase(addr);

    // 写入数据（256字节）
    SPI_Flash_Write(tx_buf, addr, 256);

    // 读取数据
    SPI_Flash_Read(rx_buf, addr, 256);

    // 验证数据是否一致
    for (uint16_t i = 0; i < 256; i++) {
        if (rx_buf[i] != tx_buf[i]) {
            // 验证失败，可添加错误处理
            while (1);
        }
    }

    // 操作成功（可添加指示灯提示）
    while (1) {
        Delay_Ms(500);
        // 此处可添加LED闪烁等操作表示成功
    }
}
#endif