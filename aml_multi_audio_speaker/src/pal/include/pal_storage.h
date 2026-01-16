/**
 * @file pal_storage.h
 * @brief 存储服务抽象接口
 * @details 定义存储服务抽象层的接口函数，封装存储相关的服务和功能
 * @author AML Audio Team
 * @date 2026-01-16
 */

#ifndef PAL_STORAGE_H
#define PAL_STORAGE_H

#include "common_def.h"

/**
 * @brief 媒体文件信息结构体
 */
typedef struct {
    char file_path[256];    // 文件路径
    char file_name[128];    // 文件名
    long file_size;         // 文件大小，单位为字节
    int duration;           // 音频文件时长，单位为秒（如果适用）
} PalMediaFile_t;

/**
 * @brief 媒体扫描回调函数
 * @param file 媒体文件信息结构体指针
 * @param user_data 用户自定义数据
 */
typedef void (*PalMediaScanCallback_t)(PalMediaFile_t *file, void *user_data);

/**
 * @brief 初始化存储服务
 * @details 初始化存储服务模块
 * @return 初始化结果：0表示成功，非0表示失败
 */
int pal_storage_init(void);

/**
 * @brief 反初始化存储服务
 * @details 反初始化存储服务模块
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int pal_storage_deinit(void);

/**
 * @brief 挂载存储设备
 * @details 挂载指定的存储设备到指定的挂载点
 * @param dev_path 设备路径，如"/dev/sda1"
 * @param mount_point 挂载点，如"/mnt/usb"
 * @return 挂载结果：0表示成功，非0表示失败
 */
int pal_storage_mount(const char *dev_path, const char *mount_point);

/**
 * @brief 卸载存储设备
 * @details 卸载指定挂载点的存储设备
 * @param mount_point 挂载点，如"/mnt/usb"
 * @return 卸载结果：0表示成功，非0表示失败
 */
int pal_storage_unmount(const char *mount_point);

/**
 * @brief 获取存储设备可用空间
 * @details 获取指定路径的存储设备可用空间
 * @param path 存储设备路径，如"/mnt/usb"
 * @param free_space 可用空间指针，用于存储获取的可用空间（单位为字节）
 * @return 获取结果：0表示成功，非0表示失败
 */
int pal_storage_get_free_space(const char *path, long *free_space);

/**
 * @brief 获取存储设备总空间
 * @details 获取指定路径的存储设备总空间
 * @param path 存储设备路径，如"/mnt/usb"
 * @param total_space 总空间指针，用于存储获取的总空间（单位为字节）
 * @return 获取结果：0表示成功，非0表示失败
 */
int pal_storage_get_total_space(const char *path, long *total_space);

/**
 * @brief 扫描媒体文件
 * @details 扫描指定路径下的媒体文件
 * @param path 扫描路径，如"/mnt/usb"
 * @param callback 扫描回调函数，用于处理扫描到的媒体文件
 * @param user_data 用户自定义数据，传递给回调函数
 * @return 扫描结果：0表示成功，非0表示失败
 */
int pal_storage_scan_media(const char *path, PalMediaScanCallback_t callback, void *user_data);

/**
 * @brief 检查存储设备是否已挂载
 * @details 检查指定的挂载点是否已挂载
 * @param mount_point 挂载点，如"/mnt/usb"
 * @return 检查结果：1表示已挂载，0表示未挂载，-1表示检查失败
 */
int pal_storage_is_mounted(const char *mount_point);

#endif /* PAL_STORAGE_H */