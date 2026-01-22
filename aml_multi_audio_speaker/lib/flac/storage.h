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

/******************************************************************************************
 * 对外暴露接口 - 文件播放功能
 ******************************************************************************************/
/**
 * @brief  播放指定的音频文件
 * @param  file_path 文件路径
 * @return SUCCESS/FAILURE
 */
int storage_play_file(const char *file_path);

/**
 * @brief  暂停当前播放的音频文件
 * @return SUCCESS/FAILURE
 */
int storage_pause(void);

/**
 * @brief  继续播放当前暂停的音频文件
 * @return SUCCESS/FAILURE
 */
int storage_resume(void);

/**
 * @brief  停止当前播放的音频文件
 * @return SUCCESS/FAILURE
 */
int storage_stop(void);

/**
 * @brief  设置播放音量
 * @param  volume 音量值（0-100）
 * @return SUCCESS/FAILURE
 */
int storage_set_volume(int volume);

/**
 * @brief  扫描指定路径下的所有音频文件
 * @param  path 扫描路径
 * @return 扫描到的文件数量，失败返回-1
 */
int storage_scan_media(const char *path);

/**
 * @brief  获取扫描到的媒体文件总数
 * @return 媒体文件总数
 */
int storage_get_media_count(void);

/**
 * @brief  获取指定索引的媒体文件路径
 * @param  index 文件索引
 * @return 文件路径，失败返回NULL
 */
const char *storage_get_media_file(int index);

/**
 * @brief  检查当前是否正在播放文件
 * @return true 正在播放，false 未在播放
 */
bool storage_is_playing(void);

/**
 * @brief  获取当前播放的文件名
 * @return 当前播放的文件名，未在播放返回NULL
 */
const char *storage_get_current_file(void);

/**
 * @brief  检查存储系统状态
 * @details 检查文件播放状态，处理U盘拔出等错误情况
 * @return SUCCESS/FAILURE
 */
int storage_check_status(void);

#endif // __STORAGE_H__