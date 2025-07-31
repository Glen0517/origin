#include "flash.h"

/**
 * @brief  初始化SPI Flash
 * @param  无
 * @retval 状态码: FLASH_OK(0)表示成功, 其他值表示失败
 */
void Flash_SPI_Init(void)
{
    SPIConfig_Struct spi_config;
    GPIO_InitTypeDef gpio_init;

    /* 使能SPI和GPIO时钟 */
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* 配置CS引脚 */
    gpio_init.Pin = SPI_FLASH_CS_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SPI_FLASH_CS_PORT, &gpio_init);
    Flash_CS_Disable();

    /* 配置SPI参数 */
    spi_config.baseAddress = SPI_FLASH_BASE_ADDR;
    spi_config.mode = SPI_FLASH_MODE;
    spi_config.dataSize = SPI_DATA_8BIT;
    spi_config.prescaler = SPI_FLASH_SPEED;
    spi_config.masterMode = 1;
    spi_config.lsbFirst = 0;

    /* 初始化SPI */
    SPI_Init(&spi_config);
    SPI_Enable(SPI_FLASH_BASE_ADDR);
}

/**
 * @brief  使能Flash片选
 * @param  无
 * @retval 无
 */
void Flash_CS_Enable(void)
{
    HAL_GPIO_WritePin(SPI_FLASH_CS_PORT, SPI_FLASH_CS_PIN, GPIO_PIN_RESET);
}

/**
 * @brief  禁用Flash片选
 * @param  无
 * @retval 无
 */
void Flash_CS_Disable(void)
{
    HAL_GPIO_WritePin(SPI_FLASH_CS_PORT, SPI_FLASH_CS_PIN, GPIO_PIN_SET);
}

/**
 * @brief  初始化Flash
 * @param  无
 * @retval 状态码: FLASH_OK(0)表示成功, 其他值表示失败
 */
uint8_t Flash_Init(void)
{
    Flash_SPI_Init();
    uint32_t id = Flash_ReadID();
    /* 检查ID是否为W25Q系列常见ID格式 */
    return (id != 0 && id != 0xFFFFFFFF) ? FLASH_OK : FLASH_ERROR;
}

/**
 * @brief  读取Flash ID
 * @param  无
 * @retval ID值(3字节)
 */
uint32_t Flash_ReadID(void)
{
    uint32_t id = 0;

    Flash_CS_Enable();
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, W25Q_JEDEC_ID);
    id |= (uint32_t)SPI_TransferByte(SPI_FLASH_BASE_ADDR, 0xFF) << 16;
    id |= (uint32_t)SPI_TransferByte(SPI_FLASH_BASE_ADDR, 0xFF) << 8;
    id |= SPI_TransferByte(SPI_FLASH_BASE_ADDR, 0xFF);
    Flash_CS_Disable();

    return id;
}

/**
 * @brief  等待Flash空闲
 * @param  timeout: 超时时间(ms)
 * @retval 状态码: FLASH_OK(0)表示成功, FLASH_TIMEOUT表示超时
 */
static uint8_t Flash_WaitBusy(uint32_t timeout)
{
    uint8_t status;
    uint32_t start_time = HAL_GetTick();
    
    Flash_CS_Enable();
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, W25Q_READ_STATUS1);
    do {
        status = SPI_TransferByte(SPI_FLASH_BASE_ADDR, 0xFF);
        
        if ((HAL_GetTick() - start_time) > timeout)
        {
            Flash_CS_Disable();
            return FLASH_TIMEOUT;
        }
    } while ((status & W25Q_BUSY_BIT) != 0);
    Flash_CS_Disable();
    return FLASH_OK;
}

/**
 * @brief  写使能
 * @param  无
 * @retval 无
 */
static void Flash_WriteEnable(void)
{
    Flash_CS_Enable();
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, W25Q_WRITE_ENABLE);
    Flash_CS_Disable();
}

/**
 * @brief  擦除指定扇区
 * @param  sector: 要擦除的扇区编号
 * @retval 状态码: FLASH_OK(0)表示成功, 其他值表示失败
 */
uint8_t Flash_EraseSector(uint32_t sector)
{
    uint32_t address;

    /* 检查扇区编号是否有效 */
    if(Flash_CheckSector(sector) != FLASH_OK)
    {
        return FLASH_ERROR;
    }

    /* 计算扇区起始地址 */
    address = Flash_GetSectorStartAddress(sector);

    Flash_WriteEnable();
    /* 等待写使能完成 */
    if(Flash_WaitBusy(1000) != FLASH_OK)
    {
        return FLASH_TIMEOUT;
    }

    /* 发送扇区擦除命令和地址 */
    Flash_CS_Enable();
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, W25Q_SECTOR_ERASE);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 16) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 8) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, address & 0xFF);
    Flash_CS_Disable();

    if(Flash_WaitBusy(5000) != FLASH_OK)
    {
        return FLASH_TIMEOUT;
    }
    return FLASH_OK;
}

/**
 * @brief  32KB块擦除
 * @param  address: 块起始地址
 * @retval 状态码: FLASH_OK(0)表示成功, 其他值表示失败
 */
uint8_t Flash_EraseBlock32K(uint32_t address)
{
    /* 检查地址是否有效 */
    if(address > FLASH_MAX_ADDRESS)
    {
        return FLASH_ERROR;
    }

    Flash_WriteEnable();
    /* 等待写使能完成 */
    if(Flash_WaitBusy(1000) != FLASH_OK)
    {
        return FLASH_TIMEOUT;
    }

    /* 发送32KB块擦除命令和地址 */
    Flash_CS_Enable();
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, W25Q_BLOCK_ERASE_32K);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 16) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 8) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, address & 0xFF);
    Flash_CS_Disable();

    if(Flash_WaitBusy(30000) != FLASH_OK)
    {
        return FLASH_TIMEOUT;
    }
    return FLASH_OK;
}

/**
 * @brief  64KB块擦除
 * @param  address: 块起始地址
 * @retval 状态码: FLASH_OK(0)表示成功, 其他值表示失败
 */
uint8_t Flash_EraseBlock64K(uint32_t address)
{
    /* 检查地址是否有效 */
    if(address > FLASH_MAX_ADDRESS)
    {
        return FLASH_ERROR;
    }

    Flash_WriteEnable();
    /* 等待写使能完成 */
    if(Flash_WaitBusy(1000) != FLASH_OK)
    {
        return FLASH_TIMEOUT;
    }

    /* 发送64KB块擦除命令和地址 */
    Flash_CS_Enable();
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, W25Q_BLOCK_ERASE_64K);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 16) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 8) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, address & 0xFF);
    Flash_CS_Disable();

    if(Flash_WaitBusy(60000) != FLASH_OK)
    {
        return FLASH_TIMEOUT;
    }
    return FLASH_OK;
}

/**
 * @brief  整片擦除
 * @param  无
 * @retval 状态码: FLASH_OK(0)表示成功, 其他值表示失败
 */
uint8_t Flash_EraseChip(void)
{
    Flash_WriteEnable();
    /* 等待写使能完成 */
    if(Flash_WaitBusy(1000) != FLASH_OK)
    {
        return FLASH_TIMEOUT;
    }

    /* 发送整片擦除命令 */
    Flash_CS_Enable();
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, W25Q_CHIP_ERASE);
    Flash_CS_Disable();

    if(Flash_WaitBusy(120000) != FLASH_OK)
    {
        return FLASH_TIMEOUT;
    }
    return FLASH_OK;
}

/**
 * @brief  写入一个字节
 * @param  address: 写入地址
 * @param  data: 要写入的数据
 * @retval 状态码: FLASH_OK(0)表示成功, 其他值表示失败
 */
uint8_t Flash_WriteByte(uint32_t address, uint8_t data)
{
    /* 检查地址有效性 */
    if(address > FLASH_MAX_ADDRESS)
    {
        return FLASH_ERROR;
    }

    Flash_WriteEnable();
    if(Flash_WaitBusy(1000) != FLASH_OK)
    {
        return FLASH_TIMEOUT;
    }

    /* 发送页编程命令和地址 */
    Flash_CS_Enable();
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, W25Q_PAGE_PROGRAM);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 16) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 8) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, address & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, data);
    Flash_CS_Disable();

    if(Flash_WaitBusy(5000) != FLASH_OK)
    {
        return FLASH_TIMEOUT;
    }

    /* 验证写入数据 */
    if(Flash_ReadByte(address) != data)
    {
        return FLASH_ERROR;
    }

    return FLASH_OK;
}

/**
 * @brief  写入一个半字(16位)
 * @param  address: 写入地址
 * @param  data: 要写入的数据
 * @retval 状态码: FLASH_OK(0)表示成功, 其他值表示失败
 */
uint8_t Flash_WriteHalfWord(uint32_t address, uint16_t data)
{
    /* 检查地址是否有效且对齐 */
    if((address > FLASH_MAX_ADDRESS) || (address % 2 != 0))
    {
        return FLASH_ERROR;
    }

    Flash_WriteEnable();
    if(Flash_WaitBusy(1000) != FLASH_OK)
    {
        return FLASH_TIMEOUT;
    }

    /* 发送页编程命令和地址 */
    Flash_CS_Enable();
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, W25Q_PAGE_PROGRAM);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 16) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 8) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, address & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (data >> 8) & 0xFF);  /* 高字节 */
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, data & 0xFF);         /* 低字节 */
    Flash_CS_Disable();

    if(Flash_WaitBusy(5000) != FLASH_OK)
    {
        return FLASH_TIMEOUT;
    }

    /* 验证写入数据 */
    if(Flash_ReadHalfWord(address) != data)
    {
        return FLASH_ERROR;
    }

    return FLASH_OK;
}

/**
 * @brief  写入一个字(32位)
 * @param  address: 写入地址
 * @param  data: 要写入的数据
 * @retval 状态码: FLASH_OK(0)表示成功, 其他值表示失败
 */
uint8_t Flash_WriteWord(uint32_t address, uint32_t data)
{
    /* 检查地址是否有效且对齐 */
    if((address > FLASH_MAX_ADDRESS) || (address % 4 != 0))
    {
        return FLASH_ERROR;
    }

    Flash_WriteEnable();
    if(Flash_WaitBusy(1000) != FLASH_OK)
    {
        return FLASH_TIMEOUT;
    }

    /* 发送页编程命令和地址 */
    Flash_CS_Enable();
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, W25Q_PAGE_PROGRAM);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 16) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 8) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, address & 0xFF);
    uint8_t data_bytes[4];
        data_bytes[0] = (data >> 24) & 0xFF; /* 最高字节 */
        data_bytes[1] = (data >> 16) & 0xFF;
        data_bytes[2] = (data >> 8) & 0xFF;
        data_bytes[3] = data & 0xFF;         /* 最低字节 */
        SPI_Transfer(SPI_FLASH_BASE_ADDR, NULL, data_bytes, 4);
    Flash_CS_Disable();

    if(Flash_WaitBusy(5000) != FLASH_OK)
    {
        return FLASH_TIMEOUT;
    }

    /* 验证写入数据 */
    if(Flash_ReadWord(address) != data)
    {
        return FLASH_ERROR;
    }

    return FLASH_OK;
}

/**
 * @brief  写入一个双字(64位)
 * @param  address: 写入地址
 * @param  data: 要写入的数据
 * @retval 状态码: FLASH_OK(0)表示成功, 其他值表示失败
 */
uint8_t Flash_WriteDoubleWord(uint32_t address, uint64_t data)
{
    /* 检查地址是否有效且对齐 */
    if((address > FLASH_MAX_ADDRESS) || (address % 8 != 0))
    {
        return FLASH_ERROR;
    }

    Flash_WriteEnable();
    if(Flash_WaitBusy(1000) != FLASH_OK)
    {
        return FLASH_TIMEOUT;
    }

    /* 发送页编程命令和地址 */
    Flash_CS_Enable();
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, W25Q_PAGE_PROGRAM);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 16) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 8) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, address & 0xFF);
    uint8_t data_bytes[8];
        data_bytes[0] = (data >> 56) & 0xFF; /* 最高字节 */
        data_bytes[1] = (data >> 48) & 0xFF;
        data_bytes[2] = (data >> 40) & 0xFF;
        data_bytes[3] = (data >> 32) & 0xFF;
        data_bytes[4] = (data >> 24) & 0xFF;
        data_bytes[5] = (data >> 16) & 0xFF;
        data_bytes[6] = (data >> 8) & 0xFF;
        data_bytes[7] = data & 0xFF;         /* 最低字节 */
        SPI_Transfer(SPI_FLASH_BASE_ADDR, NULL, data_bytes, 8);
    Flash_CS_Disable();

    if(Flash_WaitBusy(5000) != FLASH_OK)
    {
        return FLASH_TIMEOUT;
    }

    /* 验证写入数据 */
    if(Flash_ReadDoubleWord(address) != data)
    {
        return FLASH_ERROR;
    }

    return FLASH_OK;
}

/**
 * @brief  写入缓冲区数据
 * @param  address: 起始写入地址
 * @param  buffer: 数据缓冲区
 * @param  length: 数据长度(字节)
 * @retval 状态码: FLASH_OK(0)表示成功, 其他值表示失败
 */
uint8_t Flash_WriteBuffer(uint32_t address, uint8_t *buffer, uint32_t length)
{
    uint32_t i, j;
    uint32_t page_size = FLASH_PAGE_SIZE;  /* 页大小 */
    uint32_t page_offset;
    uint32_t remaining_bytes;
    uint32_t write_bytes;

    /* 参数检查 */
    if((buffer == NULL) || (length == 0))
    {
        return FLASH_ERROR;
    }

    /* 检查地址范围 */
    if(address > FLASH_MAX_ADDRESS || (address + length) > (FLASH_BASE + FLASH_SIZE))
    {
        return FLASH_ERROR;
    }

    page_offset = address % page_size;
    remaining_bytes = length;
    i = 0;

    /* 分块写入数据，处理页边界问题 */
    while(remaining_bytes > 0)
    {
        /* 计算当前页可写入的字节数 */
        if(page_offset == 0 && remaining_bytes >= page_size)
        {
            write_bytes = page_size;
        }
        else
        {
            write_bytes = page_size - page_offset;
            if(write_bytes > remaining_bytes)
            {
                write_bytes = remaining_bytes;
            }
        }

        Flash_WriteEnable();
        if(Flash_WaitBusy(1000) != FLASH_OK)
        {
            return FLASH_TIMEOUT;
        }

        /* 发送页编程命令和地址 */
        Flash_CS_Enable();
        SPI_TransferByte(SPI_FLASH_BASE_ADDR, W25Q_PAGE_PROGRAM);
        SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 16) & 0xFF);
        SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 8) & 0xFF);
        SPI_TransferByte(SPI_FLASH_BASE_ADDR, address & 0xFF);

        /* 发送数据 */
        for(j = 0; j < write_bytes; j++)
        {
            SPI_TransferByte(SPI_FLASH_BASE_ADDR, buffer[i + j]);
        }

        Flash_CS_Disable();

        if(Flash_WaitBusy(5000) != FLASH_OK)
        {
            return FLASH_TIMEOUT;
        }

        /* 更新变量 */
        address += write_bytes;
        i += write_bytes;
        remaining_bytes -= write_bytes;
        page_offset = 0;  /* 下一页从偏移0开始 */
    }

    /* 验证写入数据 */
    for(i = 0; i < length; i++)
    {
        if(Flash_ReadByte(address - length + i) != buffer[i])
        {
            return FLASH_ERROR;
        }
    }

    return FLASH_OK;
}

/**
 * @brief  读取一个字节
 * @param  address: 读取地址
 * @retval 读取的数据
 */
uint8_t Flash_ReadByte(uint32_t address)
{
    uint8_t data;

    /* 检查地址有效性 */
    if(address > FLASH_MAX_ADDRESS)
    {
        return 0xFF;
    }

    Flash_CS_Enable();
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, W25Q_READ_DATA);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 16) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 8) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, address & 0xFF);
    data = SPI_TransferByte(SPI_FLASH_BASE_ADDR, 0xFF);
    Flash_CS_Disable();

    return data;
}

/**
 * @brief  读取一个半字(16位)
 * @param  address: 读取地址
 * @retval 读取的数据
 */
uint16_t Flash_ReadHalfWord(uint32_t address)
{
    uint8_t data[2];

    /* 检查地址有效性和对齐 */
    if((address > FLASH_MAX_ADDRESS) || (address % 2 != 0))
    {
        return 0xFFFF;
    }

    Flash_CS_Enable();
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, W25Q_READ_DATA);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 16) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 8) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, address & 0xFF);
    data[0] = SPI_TransferByte(SPI_FLASH_BASE_ADDR, 0xFF);
    data[1] = SPI_TransferByte(SPI_FLASH_BASE_ADDR, 0xFF);
    Flash_CS_Disable();

    return (data[0] << 8) | data[1];
}

/**
 * @brief  读取一个字(32位)
 * @param  address: 读取地址
 * @retval 读取的数据
 */
uint32_t Flash_ReadWord(uint32_t address)
{
    uint8_t data[4];

    /* 检查地址有效性和对齐 */
    if((address > FLASH_MAX_ADDRESS) || (address % 4 != 0))
    {
        return 0xFFFFFFFF;
    }

    Flash_CS_Enable();
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, W25Q_READ_DATA);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 16) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 8) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, address & 0xFF);
    SPI_Transfer(SPI_FLASH_BASE_ADDR, NULL, data, 4);
    Flash_CS_Disable();

    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

/**
 * @brief  读取一个双字(64位)
 * @param  address: 读取地址
 * @retval 读取的数据
 */
uint64_t Flash_ReadDoubleWord(uint32_t address)
{
    uint8_t data[8];

    /* 检查地址有效性和对齐 */
    if((address > FLASH_MAX_ADDRESS) || (address % 8 != 0))
    {
        return 0xFFFFFFFFFFFFFFFF;
    }

    Flash_CS_Enable();
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, W25Q_READ_DATA);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 16) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 8) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, address & 0xFF);
    SPI_Transfer(SPI_FLASH_BASE_ADDR, NULL, data, 8);
    Flash_CS_Disable();

    return ((uint64_t)data[0] << 56) | ((uint64_t)data[1] << 48) | 
           ((uint64_t)data[2] << 40) | ((uint64_t)data[3] << 32) | 
           ((uint64_t)data[4] << 24) | ((uint64_t)data[5] << 16) | 
           ((uint64_t)data[6] << 8) | data[7];
}

/**
 * @brief  读取缓冲区数据
 * @param  address: 起始读取地址
 * @param  buffer: 数据缓冲区
 * @param  length: 数据长度(字节)
 * @retval 状态码: FLASH_OK(0)表示成功, 其他值表示失败
 */
uint8_t Flash_ReadBuffer(uint32_t address, uint8_t *buffer, uint32_t length)
{
    /* 参数检查 */
    if((buffer == NULL) || (length == 0))
    {
        return FLASH_ERROR;
    }

    /* 检查地址范围 */
        if(address > FLASH_MAX_ADDRESS || (address + length) > (FLASH_BASE + FLASH_SIZE))
    {
        return FLASH_ERROR;
    }

    Flash_CS_Enable();
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, W25Q_READ_DATA);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 16) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, (address >> 8) & 0xFF);
    SPI_TransferByte(SPI_FLASH_BASE_ADDR, address & 0xFF);
    SPI_Transfer(SPI_FLASH_BASE_ADDR, NULL, buffer, length);
    Flash_CS_Disable();

    return FLASH_OK;
}

/**
 * @brief  检查扇区是否有效
 * @param  sector: 要检查的扇区
 * @retval 状态码: FLASH_OK(0)表示有效, FLASH_ERROR表示无效
 */
uint8_t Flash_CheckSector(uint32_t sector)
{
    /* 检查扇区是否在有效范围内 */
    if(sector < FLASH_MAX_SECTOR)
    {
        return FLASH_OK;
    }
    return FLASH_ERROR;
}

/**
 * @brief  获取地址对应的扇区编号
 * @param  address:  Flash地址
 * @retval 扇区编号
 */
uint8_t Flash_GetSectorNumber(uint32_t address)
{
    /* 检查地址是否有效 */
    if(address < FLASH_BASE + FLASH_SIZE)
    {
        return (uint8_t)(address / FLASH_SECTOR_SIZE);
    }
    return 0xFF;  /* 无效扇区 */
}

/**
 * @brief  获取扇区的起始地址
 * @param  sector: 扇区编号
 * @retval 扇区起始地址
 */
uint32_t Flash_GetSectorStartAddress(uint32_t sector)
{
    /* 检查扇区编号是否有效 */
    if(sector < FLASH_MAX_SECTOR)
    {
        return sector * FLASH_SECTOR_SIZE;
    }
    return 0;  /* 无效扇区返回0 */
}

/**
 * @brief  获取扇区的结束地址
 * @param  sector: 扇区编号
 * @retval 扇区结束地址
 */
uint32_t Flash_GetSectorEndAddress(uint32_t sector)
{
    /* 检查扇区编号是否有效 */
    if(sector < FLASH_MAX_SECTOR)
    {
        return Flash_GetSectorStartAddress(sector) + (FLASH_SECTOR_SIZE - 1);
    }
    return 0;  /* 无效扇区返回0 */
}

/**
 * @brief  Flash错误处理函数
 * @param  无
 * @retval 无
 */
void Flash_ErrorHandler(void)
{
    /* 用户可以根据需要实现错误处理 */
    while(1)
    {
        /* 错误处理 */
    }
}