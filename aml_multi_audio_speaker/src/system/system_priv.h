#ifndef __SYSTEM_PRIV_H__
#define __SYSTEM_PRIV_H__

#include "system.h"
#include "product_type.h"

// 内部私有函数声明
int sys_init_core(void);
void sys_init_deinit(void);
int sys_ota_init(void);
void sys_ota_deinit(void);

#endif // __SYSTEM_PRIV_H__