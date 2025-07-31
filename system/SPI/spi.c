/**
 * SPI库函数实现 - 支持配置和基本通信操作
 * 适用于嵌入式系统，使用STM32 HAL库
 */
#include "spi.h"

/**
 * 初始化SPI设备
 * @param device SPI设备结构体指针
 * @param hspi SPI句柄指针
 * @param cs_port CS引脚端口
 * @param cs_pin CS引脚编号
 * @return HAL状态
 */
HAL_StatusTypeDef SPI_InitDevice(SPIDevice_Struct *device, SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin) {
    if (device == NULL || hspi == NULL) {
        return HAL_ERROR;
    }
    
    // 初始化设备结构体
    device->hspi = hspi;
    device->cs_port = cs_port;
    device->cs_pin = cs_pin;
    
    // 配置CS引脚
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = cs_pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(cs_port, &GPIO_InitStruct);
    
    // 初始状态为未选中
    SPI_DeselectDevice(device);
    
    return HAL_OK;
}

/**
 * 注销SPI设备
 * @param device SPI设备结构体指针
 * @return HAL状态
 */
HAL_StatusTypeDef SPI_DeInitDevice(SPIDevice_Struct *device) {
    if (device == NULL) {
        return HAL_ERROR;
    }
    
    // 释放CS引脚
    HAL_GPIO_DeInit(device->cs_port, device->cs_pin);
    
    // 注销SPI
    return HAL_SPI_DeInit(device->hspi);
}

/**
 * 选择SPI设备
 * @param device SPI设备结构体指针
 */
void SPI_SelectDevice(SPIDevice_Struct *device) {
    if (device != NULL) {
        HAL_GPIO_WritePin(device->cs_port, device->cs_pin, GPIO_PIN_RESET);
    }
}

/**
 * 取消选择SPI设备
 * @param device SPI设备结构体指针
 */
void SPI_DeselectDevice(SPIDevice_Struct *device) {
    if (device != NULL) {
        HAL_GPIO_WritePin(device->cs_port, device->cs_pin, GPIO_PIN_SET);
    }
}

/**
 * 通过SPI发送单个字节
 * @param device SPI设备结构体指针
 * @param data 要发送的数据
 * @return HAL状态
 */
HAL_StatusTypeDef SPI_TransmitByte(SPIDevice_Struct *device, uint8_t data) {
    if (device == NULL) {
        return HAL_ERROR;
    }
    
    SPI_SelectDevice(device);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(device->hspi, &data, 1, HAL_MAX_DELAY);
    SPI_DeselectDevice(device);
    
    return status;
}

/**
 * 通过SPI接收单个字节
 * @param device SPI设备结构体指针
 * @param data 接收数据的缓冲区
 * @return HAL状态
 */
HAL_StatusTypeDef SPI_ReceiveByte(SPIDevice_Struct *device, uint8_t *data) {
    if (device == NULL || data == NULL) {
        return HAL_ERROR;
    }
    
    SPI_SelectDevice(device);
    HAL_StatusTypeDef status = HAL_SPI_Receive(device->hspi, data, 1, HAL_MAX_DELAY);
    SPI_DeselectDevice(device);
    
    return status;
}

/**
 * 通过SPI发送并接收单个字节
 * @param device SPI设备结构体指针
 * @param tx_data 要发送的数据
 * @param rx_data 接收数据的缓冲区
 * @return HAL状态
 */
HAL_StatusTypeDef SPI_TransmitReceiveByte(SPIDevice_Struct *device, uint8_t tx_data, uint8_t *rx_data) {
    if (device == NULL || rx_data == NULL) {
        return HAL_ERROR;
    }
    
    SPI_SelectDevice(device);
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(device->hspi, &tx_data, rx_data, 1, HAL_MAX_DELAY);
    SPI_DeselectDevice(device);
    
    return status;
}

/**
 * 通过SPI发送多个字节
 * @param device SPI设备结构体指针
 * @param pData 发送数据的缓冲区
 * @param Size 数据大小
 * @param Timeout 超时时间
 * @return HAL状态
 */
HAL_StatusTypeDef SPI_Transmit(SPIDevice_Struct *device, uint8_t *pData, uint16_t Size, uint32_t Timeout) {
    if (device == NULL || pData == NULL) {
        return HAL_ERROR;
    }
    
    SPI_SelectDevice(device);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(device->hspi, pData, Size, Timeout);
    SPI_DeselectDevice(device);
    
    return status;
}

/**
 * 通过SPI接收多个字节
 * @param device SPI设备结构体指针
 * @param pData 接收数据的缓冲区
 * @param Size 数据大小
 * @param Timeout 超时时间
 * @return HAL状态
 */
HAL_StatusTypeDef SPI_Receive(SPIDevice_Struct *device, uint8_t *pData, uint16_t Size, uint32_t Timeout) {
    if (device == NULL || pData == NULL) {
        return HAL_ERROR;
    }
    
    SPI_SelectDevice(device);
    HAL_StatusTypeDef status = HAL_SPI_Receive(device->hspi, pData, Size, Timeout);
    SPI_DeselectDevice(device);
    
    return status;
}

/**
 * 通过SPI发送并接收多个字节
 * @param device SPI设备结构体指针
 * @param pTxData 发送数据的缓冲区
 * @param pRxData 接收数据的缓冲区
 * @param Size 数据大小
 * @param Timeout 超时时间
 * @return HAL状态
 */
HAL_StatusTypeDef SPI_TransmitReceive(SPIDevice_Struct *device, uint8_t *pTxData, uint8_t *pRxData, uint16_t Size, uint32_t Timeout) {
    if (device == NULL || pTxData == NULL || pRxData == NULL) {
        return HAL_ERROR;
    }
    
    SPI_SelectDevice(device);
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(device->hspi, pTxData, pRxData, Size, Timeout);
    SPI_DeselectDevice(device);
    
    return status;
}

// 用户需要在应用程序中实现SPI初始化配置
// 以下是一个示例配置函数
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