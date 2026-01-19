#ifndef __SYSTEM_PRIV_H__
#define __SYSTEM_PRIV_H__

#include "system.h"
#include "product_type.h"
#include "common_def.h"

// 系统配置结构体
typedef struct {
    bool init_ok;            // 初始化状态
    bool ota_enable;         // OTA功能使能
    bool ota_upgrading;      // OTA升级中
    char ota_file_path[256];  // OTA升级文件路径
    int ota_progress;         // OTA升级进度(0-100)
} SystemCfg_t;

// 内部私有函数声明
int sys_init_core(void);
void sys_init_deinit(void);
int sys_ota_init(void);
void sys_ota_deinit(void);

#endif // __SYSTEM_PRIV_H__