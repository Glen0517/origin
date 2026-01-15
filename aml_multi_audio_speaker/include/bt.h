#ifndef __BLUETOOTH_H__
#define __BLUETOOTH_H__

#include "common_def.h"

/******************************************************************************************
 * 蓝牙配置结构体
 ******************************************************************************************/
typedef struct {
    char bt_name[32];      // 蓝牙设备名
    char bt_pin[8];        // 蓝牙配对码
    bool bt_auto_connect;  // 是否自动重连
    bool bt_mesh_en;       // 是否开启蓝牙MESH
} BluetoothConfig_t;

/******************************************************************************************
 * 对外暴露接口 - 蓝牙模块所有功能，宏控裁剪蓝牙MESH
 ******************************************************************************************/
/**
 * @brief  蓝牙模块初始化
 * @param  cfg 蓝牙配置结构体指针
 * @return SUCCESS/FAILURE
 */
int bluetooth_init(BluetoothConfig_t *cfg);

/**
 * @brief  蓝牙模块反初始化
 * @return SUCCESS/FAILURE
 */
int bluetooth_deinit(void);

/**
 * @brief  开启蓝牙配对
 * @return SUCCESS/FAILURE
 */
int bluetooth_start_pair(void);

/**
 * @brief  关闭蓝牙配对
 * @return SUCCESS/FAILURE
 */
int bluetooth_stop_pair(void);

/**
 * @brief  获取蓝牙连接状态
 * @return TRUE-已连接 FALSE-未连接
 */
bool bluetooth_get_connect_state(void);

/**
 * @brief  获取当前连接的蓝牙设备名
 * @param  name 设备名缓冲区
 * @param  len 缓冲区长度
 * @return SUCCESS/FAILURE
 */
int bluetooth_get_dev_name(char *name, int len);

#if CONFIG_ENABLE_BT_MESH
/**
 * @brief  蓝牙MESH组网(高端+低音炮专属)
 * @return SUCCESS/FAILURE
 */
int bluetooth_mesh_create_network(void);

/**
 * @brief  蓝牙MESH配对(高端+低音炮专属)
 * @return SUCCESS/FAILURE
 */
int bluetooth_mesh_pair(void);

/**
 * @brief  蓝牙MESH音量同步(高端+低音炮专属)
 * @param  vol 音量值
 * @return SUCCESS/FAILURE
 */
int bluetooth_mesh_sync_volume(int vol);
#endif

#endif // __BLUETOOTH_H__