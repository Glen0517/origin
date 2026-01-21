#ifndef __STORAGE_H__
#define __STORAGE_H__

#include "common_def.h"

/******************************************************************************************
 * FLASH校准参数结构体(产线专用)
 ******************************************************************************************/
typedef struct __attribute__((packed, aligned(4))) {
    int eq_gain[9];                // EQ频点增益
    int bass_gain;                 // 低音增益
    int treble_gain;               // 高音增益
    int master_volume_gain;        // 主音量补偿
    int channel_balance;           // 声道平衡
    int product_type_match;        // 产品类型匹配码
    unsigned int crc32_checksum;   // CRC32校验和
} FlashCalibParam_t;

/******************************************************************************************
 * 对外暴露接口 - 存储管理所有功能：JSON配置+FLASH校准+产线数据
 ******************************************************************************************/
/**
 * @brief  存储模块初始化
 * @return SUCCESS/FAILURE
 */
int storage_init(void);

/**
 * @brief  存储模块反初始化
 * @return SUCCESS/FAILURE
 */
int storage_deinit(void);

/**
 * @brief  加载JSON配置文件
 * @return SUCCESS/FAILURE
 */
int storage_load_json_config(void);

/**
 * @brief  读取FLASH校准参数
 * @param  param 校准参数结构体指针
 * @return SUCCESS/FAILURE
 */
int storage_read_flash_calib(FlashCalibParam_t *param);

/**
 * @brief  写入FLASH校准参数
 * @param  param 校准参数结构体指针
 * @return SUCCESS/FAILURE
 */
int storage_write_flash_calib(FlashCalibParam_t *param);

/**
 * @brief  擦除FLASH校准参数
 * @return SUCCESS/FAILURE
 */
int storage_erase_flash_calib(void);

#endif // __STORAGE_H__