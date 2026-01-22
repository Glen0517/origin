#ifndef __STORAGE_PRIV_H__
#define __STORAGE_PRIV_H__

#include "storage.h"

// 内部私有函数声明 - 细粒度拆分
int usb_mount_init(void);
void usb_mount_deinit(void);
int file_reader_init(void);
void file_reader_deinit(void);
int file_reader_play_file(const char *file_path);
int file_reader_pause(void);
int file_reader_resume(void);
int file_reader_stop(void);
int file_reader_play_next(void);
int file_reader_play_prev(void);
int file_reader_set_audio_file_list(char **file_list, int count);
void file_reader_check_play_status(void);
int media_scan_init(void);
void media_scan_deinit(void);
int media_scan_scan_path(const char *path);

#endif // __STORAGE_PRIV_H__