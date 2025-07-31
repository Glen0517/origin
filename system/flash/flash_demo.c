#include "flash_demo.h"
#include "delay.h"


/**
 * @brief  Flash功能演示
 * @param  无
 * @retval 无
 */
void Flash_Demo(void)
{
    uint8_t status;
    uint32_t sector = FLASH_SECTOR_5;  /* 选择扇区5进行测试 */
    uint32_t address = FLASH_SECTOR_5;  /* 测试地址 */
    uint8_t write_data = 0xAA;  /* 要写入的数据 */
    uint8_t read_data;  /* 读取的数据 */
    uint8_t buffer[10] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
    uint8_t read_buffer[10];
    uint32_t i;

    printf("\n===== Flash功能演示开始 =====\n");

    /* 初始化Flash */
    status = Flash_Init();
    if(status != FLASH_OK)
    {
        printf("Flash初始化失败！\n");
        return;
    }
    printf("Flash初始化成功\n");

    /* 擦除扇区 */
    printf("正在擦除扇区 %d...\n", sector);
    status = Flash_EraseSector(sector);
    if(status != FLASH_OK)
    {
        printf("扇区擦除失败！\n");
        return;
    }
    printf("扇区擦除成功\n");

    /* 写入一个字节 */
    printf("写入字节数据 0x%02X 到地址 0x%08X...\n", write_data, address);
    status = Flash_WriteByte(address, write_data);
    if(status != FLASH_OK)
    {
        printf("字节写入失败！\n");
        return;
    }
    printf("字节写入成功\n");

    /* 读取一个字节 */
    read_data = Flash_ReadByte(address);
    printf("从地址 0x%08X 读取数据: 0x%02X\n", address, read_data);
    if(read_data == write_data)
    {
        printf("字节读写验证成功\n");
    }
    else
    {
        printf("字节读写验证失败！\n");
    }

    /* 写入半字 */
    address += 2;  /* 地址对齐 */
    printf("写入半字数据 0x%04X 到地址 0x%08X...\n", 0x55AA, address);
    status = Flash_WriteHalfWord(address, 0x55AA);
    if(status != FLASH_OK)
    {
        printf("半字写入失败！\n");
        return;
    }
    printf("半字写入成功\n");

    /* 读取半字 */
    printf("从地址 0x%08X 读取半字数据: 0x%04X\n", address, Flash_ReadHalfWord(address));

    /* 写入字 */
    address += 4;  /* 地址对齐 */
    printf("写入字数据 0x%08X 到地址 0x%08X...\n", 0x12345678, address);
    status = Flash_WriteWord(address, 0x12345678);
    if(status != FLASH_OK)
    {
        printf("字写入失败！\n");
        return;
    }
    printf("字写入成功\n");

    /* 读取字 */
    printf("从地址 0x%08X 读取字数据: 0x%08X\n", address, Flash_ReadWord(address));

    /* 写入双字 */
    address += 8;  /* 地址对齐 */
    printf("写入双字数据 0x%016llX 到地址 0x%08X...\n", 0x1122334455667788ULL, address);
    status = Flash_WriteDoubleWord(address, 0x1122334455667788ULL);
    if(status != FLASH_OK)
    {
        printf("双字写入失败！\n");
        return;
    }
    printf("双字写入成功\n");

    /* 读取双字 */
    printf("从地址 0x%08X 读取双字数据: 0x%016llX\n", address, Flash_ReadDoubleWord(address));

    /* 写入缓冲区 */
    address += 8;  /* 移动到下一个地址 */
    printf("写入缓冲区数据到地址 0x%08X...\n", address);
    status = Flash_WriteBuffer(address, buffer, 10);
    if(status != FLASH_OK)
    {
        printf("缓冲区写入失败！\n");
        return;
    }
    printf("缓冲区写入成功\n");

    /* 读取缓冲区 */
    printf("从地址 0x%08X 读取缓冲区数据: ", address);
    status = Flash_ReadBuffer(address, read_buffer, 10);
    if(status != FLASH_OK)
    {
        printf("缓冲区读取失败！\n");
        return;
    }
    for(i = 0; i < 10; i++)
    {
        printf("0x%02X ", read_buffer[i]);
    }
    printf("\n");

    /* 验证缓冲区数据 */
    for(i = 0; i < 10; i++)
    {
        if(read_buffer[i] != buffer[i])
        {
            printf("缓冲区数据验证失败！索引: %d\n", i);
            return;
        }
    }
    printf("缓冲区数据验证成功\n");

    printf("\n===== Flash功能演示结束 =====\n");
}