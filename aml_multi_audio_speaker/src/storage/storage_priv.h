#ifndef __STORAGE_PRIV_H__
#define __STORAGE_PRIV_H__

#include "storage.h"

// 内部私有函数声明 - 细粒度拆分
int usb_mount_init(void);
void usb_mount_deinit(void);
int file_reader_init(void);
void file_reader_deinit(void);

#endif // __STORAGE_PRIV_H__