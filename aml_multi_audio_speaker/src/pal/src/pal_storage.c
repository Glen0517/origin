/**
 * @file pal_storage.c
 * @brief 存储服务抽象实现
 * @details 实现存储服务抽象层的接口函数，封装存储相关的服务和功能
 * @author AML Audio Team
 * @date 2026-01-16
 */

#include "pal_storage.h"
#include "log/aml_log.h"
#include <string.h>
#include <errno.h>

// 条件编译：只在Linux系统上包含Linux特定的头文件
#ifdef __linux__
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <dirent.h>
#else
// Windows系统下的模拟定义
#define MNT_FORCE 1
#define DT_REG 8
#define DT_DIR 4
int mount(const char *dev_path, const char *mount_point, const char *type, unsigned long flags, const void *data) { return 0; }
int umount(const char *target) { return 0; }
int umount2(const char *target, int flags) { return 0; }
int stat(const char *path, struct stat *buf) { return 0; }
int mkdir(const char *pathname) { return 0; }
typedef long mode_t;
typedef void *DIR;
struct dirent { int d_type; char d_name[256]; };
struct stat { int st_mode; long st_size; };
struct statvfs { long f_bavail; long f_frsize; long f_blocks; };
int statvfs(const char *path, struct statvfs *buf) { memset(buf, 0, sizeof(struct statvfs)); return 0; }
DIR *opendir(const char *path) { return (DIR *)1; }
int closedir(DIR *dir) { return 0; }
struct dirent *readdir(DIR *dir) { static struct dirent entry = {0}; return &entry; }
#endif

// 定义存储服务模块日志分类
AML_LOG_DEFINE(storage_log);
// 设置默认日志分类
#define AML_LOG_DEFAULT AML_LOG_GET_CAT(storage_log)

static bool g_storage_init = false;

/**
 * @brief 初始化存储服务
 * @details 初始化存储服务模块
 * @return 初始化结果：0表示成功，非0表示失败
 */
int pal_storage_init(void) {
    if (g_storage_init) {
        AML_LOGI("PAL storage service already initialized");
        return SUCCESS;
    }
    
    g_storage_init = true;
    AML_LOGI("PAL storage service init success");
    return SUCCESS;
}

/**
 * @brief 反初始化存储服务
 * @details 反初始化存储服务模块
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int pal_storage_deinit(void) {
    if (!g_storage_init) {
        AML_LOGI("PAL storage service not initialized");
        return SUCCESS;
    }
    
    g_storage_init = false;
    AML_LOGI("PAL storage service deinit success");
    return SUCCESS;
}

/**
 * @brief 挂载存储设备
 * @details 挂载指定的存储设备到指定的挂载点
 * @param dev_path 设备路径，如"/dev/sda1"
 * @param mount_point 挂载点，如"/mnt/usb"
 * @return 挂载结果：0表示成功，非0表示失败
 */
int pal_storage_mount(const char *dev_path, const char *mount_point) {
    if (!g_storage_init) {
        AML_LOGE("PAL storage service not initialized");
        return FAILURE;
    }

    if (!dev_path || !mount_point) {
        AML_LOGE("Invalid device path or mount point");
        return FAILURE;
    }

    AML_LOGI("Mounting device %s to %s", dev_path, mount_point);

    // 确保挂载点目录存在
    struct stat st;
    if (stat(mount_point, &st) != 0) {
        // 根据不同平台使用不同的mkdir调用方式
#ifdef __linux__
        if (mkdir(mount_point, 0755) != 0) {
#else
        if (mkdir(mount_point) != 0) {
#endif
            AML_LOGE("Create mount point failed: %s", strerror(errno));
            return FAILURE;
        }
        AML_LOGI("Created mount point directory: %s", mount_point);
    }

    // 尝试挂载设备（支持vfat和ext4文件系统）
    if (mount(dev_path, mount_point, "fat32", 0, NULL) != 0) {
        if (mount(dev_path, mount_point, "ext4", 0, NULL) != 0) {
            AML_LOGE("Mount device failed: %s", strerror(errno));
            return FAILURE;
        }
        AML_LOGI("Mounted as ext4 filesystem");
    } else {
        AML_LOGI("Mounted as fat32 filesystem");
    }

    AML_LOGI("Device mounted successfully");
    return SUCCESS;
}

/**
 * @brief 卸载存储设备
 * @details 卸载指定挂载点的存储设备
 * @param mount_point 挂载点，如"/mnt/usb"
 * @return 卸载结果：0表示成功，非0表示失败
 */
int pal_storage_unmount(const char *mount_point) {
    if (!g_storage_init) {
        AML_LOGE("PAL storage service not initialized");
        return FAILURE;
    }

    if (!mount_point) {
        AML_LOGE("Invalid mount point");
        return FAILURE;
    }

    AML_LOGI("Unmounting device from %s", mount_point);

    // 尝试卸载设备
    if (umount(mount_point) != 0) {
        // 如果失败，尝试强制卸载
        if (umount2(mount_point, MNT_FORCE) != 0) {
            AML_LOGE("Unmount device failed: %s", strerror(errno));
            return FAILURE;
        }
        AML_LOGW("Forced unmount successful");
    }

    AML_LOGI("Device unmounted successfully");
    return SUCCESS;
}

/**
 * @brief 获取存储设备可用空间
 * @details 获取指定路径的存储设备可用空间
 * @param path 存储设备路径，如"/mnt/usb"
 * @param free_space 可用空间指针，用于存储获取的可用空间（单位为字节）
 * @return 获取结果：0表示成功，非0表示失败
 */
int pal_storage_get_free_space(const char *path, long *free_space) {
    if (!g_storage_init) {
        AML_LOGE("PAL storage service not initialized");
        return FAILURE;
    }

    if (!path || !free_space) {
        AML_LOGE("Invalid path or free_space parameter");
        return FAILURE;
    }
    
    struct statvfs stat;
    
    // 调用系统API来获取空间信息
    if (statvfs(path, &stat) != 0) {
        AML_LOGE("Get storage space failed: %s", strerror(errno));
        return FAILURE;
    }
    *free_space = stat.f_bavail * stat.f_frsize;

    AML_LOGD("Free space for %s: %ld bytes", path, *free_space);
    return SUCCESS;
}

/**
 * @brief 获取存储设备总空间
 * @details 获取指定路径的存储设备总空间
 * @param path 存储设备路径，如"/mnt/usb"
 * @param total_space 总空间指针，用于存储获取的总空间（单位为字节）
 * @return 获取结果：0表示成功，非0表示失败
 */
int pal_storage_get_total_space(const char *path, long *total_space) {
    if (!g_storage_init) {
        AML_LOGE("PAL storage service not initialized");
        return FAILURE;
    }

    if (!path || !total_space) {
        AML_LOGE("Invalid path or total_space parameter");
        return FAILURE;
    }

    struct statvfs stat;

    // 调用系统API来获取空间信息
    if (statvfs(path, &stat) != 0) {
        AML_LOGE("Get storage space failed: %s", strerror(errno));
        return FAILURE;
    }
    *total_space = stat.f_blocks * stat.f_frsize;

    AML_LOGD("Total space for %s: %ld bytes", path, *total_space);
    return SUCCESS;
}

/**
 * @brief 检查文件是否为支持的媒体文件
 * @details 根据文件扩展名判断是否为支持的媒体文件
 * @param file_name 文件名
 * @return true表示是媒体文件，false表示不是
 */
static bool is_media_file(const char *file_name) {
    if (!file_name) {
        return false;
    }
    
    const char *extensions[] = {".mp3", ".wav", ".flac", ".aac", ".wma", ".ogg", NULL};
    int i = 0;
    
    while (extensions[i]) {
        if (strcasecmp(strrchr(file_name, '.'), extensions[i]) == 0) {
            return true;
        }
        i++;
    }
    
    return false;
}

/**
 * @brief 递归扫描目录中的媒体文件
 * @details 递归扫描目录，查找媒体文件并调用回调函数
 * @param path 扫描路径
 * @param callback 扫描回调函数
 * @param user_data 用户自定义数据
 * @return 扫描结果：0表示成功，非0表示失败
 */
static int scan_directory(const char *path, PalMediaScanCallback_t callback, void *user_data) {
    DIR *dir;
    struct dirent *entry;
    
    if ((dir = opendir(path)) == NULL) {
        AML_LOGE("Open directory failed: %s", strerror(errno));
        return FAILURE;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            // 检查文件扩展名，判断是否为媒体文件
            if (is_media_file(entry->d_name)) {
                // 构造完整的文件路径
                char file_path[512];
                snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);
                
                // 获取文件信息
                struct stat st;
                if (stat(file_path, &st) == 0) {
                    // 调用回调函数
                    if (callback) {
                        PalMediaFile_t file;
                        memset(&file, 0, sizeof(PalMediaFile_t));
                        strncpy(file.file_path, file_path, sizeof(file.file_path) - 1);
                        strncpy(file.file_name, entry->d_name, sizeof(file.file_name) - 1);
                        file.file_size = st.st_size;
                        // 暂时设置默认时长，实际应用中可以通过解析文件获取
                        file.duration = 0;
                        callback(&file, user_data);
                    }
                }
            }
        } else if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            // 构造子目录路径
            char subdir_path[512];
            snprintf(subdir_path, sizeof(subdir_path), "%s/%s", path, entry->d_name);
            // 递归扫描子目录
            scan_directory(subdir_path, callback, user_data);
        }
    }
    
    closedir(dir);
    return SUCCESS;
}

/**
 * @brief 扫描媒体文件
 * @details 扫描指定路径下的媒体文件
 * @param path 扫描路径，如"/mnt/usb"
 * @param callback 扫描回调函数，用于处理扫描到的媒体文件
 * @param user_data 用户自定义数据，传递给回调函数
 * @return 扫描结果：0表示成功，非0表示失败
 */
int pal_storage_scan_media(const char *path, PalMediaScanCallback_t callback, void *user_data) {
    if (!g_storage_init) {
        LOG_ERROR("PAL storage service not initialized");
        return FAILURE;
    }
    
    if (!path) {
        LOG_ERROR("Invalid scan path");
        return FAILURE;
    }
    
    LOG_INFO("Scanning media files in %s", path);
    
    // 递归扫描目录，查找媒体文件
    int result = scan_directory(path, callback, user_data);
    
    LOG_INFO("Media scan completed");
    return result;
}

/**
 * @brief 检查存储设备是否已挂载
 * @details 检查指定的挂载点是否已挂载
 * @param mount_point 挂载点，如"/mnt/usb"
 * @return 检查结果：1表示已挂载，0表示未挂载，-1表示检查失败
 */
int pal_storage_is_mounted(const char *mount_point) {
    if (!g_storage_init) {
        LOG_ERROR("PAL storage service not initialized");
        return -1;
    }
    
    if (!mount_point) {
        LOG_ERROR("Invalid mount point");
        return -1;
    }
    
    LOG_DEBUG("Checking if %s is mounted", mount_point);
    
    // 检查挂载点是否已挂载
    FILE *fp = fopen("/proc/mounts", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            char dev[64], mnt[64], fs[64], opts[64];
            if (sscanf(line, "%s %s %s %s", dev, mnt, fs, opts) == 4) {
                if (strcmp(mnt, mount_point) == 0) {
                    fclose(fp);
                    LOG_DEBUG("%s is mounted", mount_point);
                    return 1;
                }
            }
        }
        fclose(fp);
        LOG_DEBUG("%s is not mounted", mount_point);
        return 0;
    }
    
    LOG_ERROR("Open /proc/mounts failed: %s", strerror(errno));
    return -1;
}