#include "storage_priv.h"
#include "logger.h"
#include "event.h"

#include <dirent.h>
#include <string.h>
#include <stdlib.h>

#define MAX_MEDIA_FILES 1000
#define MAX_FILE_PATH 256

static bool g_media_scan_init = false;
static char g_media_files[MAX_MEDIA_FILES][MAX_FILE_PATH];
static int g_media_file_count = 0;

/**
 * @brief 检查文件是否为支持的音频文件格式
 */
static bool is_supported_audio_file(const char *file_name)
{
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
 * @brief 检查文件名是否安全（防止路径遍历攻击）
 */
static bool is_safe_filename(const char *file_name)
{
    if (!file_name) {
        return false;
    }
    
    // 检查是否包含路径遍历字符
    if (strstr(file_name, "../") || strstr(file_name, "..\\") || strcmp(file_name, "..") == 0) {
        return false;
    }
    
    return true;
}

/**
 * @brief 扫描目录中的音频文件
 */
static int scan_directory(const char *dir_path)
{
    if (!dir_path || g_media_file_count >= MAX_MEDIA_FILES) {
        return g_media_file_count;
    }

    // 检查路径长度
    if (strlen(dir_path) >= MAX_FILE_PATH - 32) { // 预留足够空间给文件名
        LOG_ERROR("Directory path too long: %s", dir_path);
        return g_media_file_count;
    }

    DIR *dir = opendir(dir_path);
    if (!dir) {
        LOG_ERROR("Failed to open directory: %s", dir_path);
        return g_media_file_count;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // 跳过当前目录和父目录
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 检查文件名安全性
        if (!is_safe_filename(entry->d_name)) {
            LOG_WARN("Skipping unsafe filename: %s", entry->d_name);
            continue;
        }

        char full_path[MAX_FILE_PATH];
        int path_len = snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        // 检查路径长度是否超过缓冲区大小
        if (path_len >= sizeof(full_path)) {
            LOG_WARN("Path too long, skipping: %s/%s", dir_path, entry->d_name);
            continue;
        }

        // 检查是否为目录
        struct stat statbuf;
        if (stat(full_path, &statbuf) == 0) {
            if (S_ISDIR(statbuf.st_mode)) {
                // 递归扫描子目录
                scan_directory(full_path);
            } else if (S_ISREG(statbuf.st_mode)) {
                // 检查是否为支持的音频文件
                if (is_supported_audio_file(entry->d_name)) {
                    // 添加到媒体文件列表
                    if (g_media_file_count < MAX_MEDIA_FILES) {
                        strncpy(g_media_files[g_media_file_count], full_path, sizeof(g_media_files[g_media_file_count]) - 1);
                        g_media_file_count++;
                    }
                }
            }
        } else {
            LOG_WARN("Failed to stat file: %s", full_path);
        }

        // 检查是否达到最大文件数
        if (g_media_file_count >= MAX_MEDIA_FILES) {
            LOG_WARN("Reached maximum media file count: %d", MAX_MEDIA_FILES);
            break;
        }
    }

    closedir(dir);
    return g_media_file_count;
}

int media_scan_init(void)
{
    if (g_media_scan_init) {
        LOG_INFO("Media scanner already initialized");
        return 0;
    }

    g_media_scan_init = true;
    g_media_file_count = 0;

    LOG_INFO("Storage: Media scanner init success");
    LOG_INFO("  Max media files: %d", MAX_MEDIA_FILES);
    LOG_INFO("  Supported formats: MP3/WAV/FLAC/AAC/WMA/OGG");

    return 0;
}

void media_scan_deinit(void)
{
    if (g_media_scan_init) {
        // 清空媒体文件列表
        g_media_file_count = 0;

        g_media_scan_init = false;

        LOG_INFO("Media scanner deinitialized");
    }
}

/**
 * @brief 比较两个文件名，用于排序
 */
static int compare_filenames(const void *a, const void *b)
{
    return strcasecmp(*(const char **)a, *(const char **)b);
}

/**
 * @brief 扫描指定路径下的所有音频文件
 */
int media_scan_scan_path(const char *path)
{
    if (!g_media_scan_init || !path) {
        LOG_ERROR("Media scan failed: invalid parameters");
        return -1;
    }

    LOG_INFO("Scanning media files in: %s", path);

    // 清空现有媒体文件列表
    g_media_file_count = 0;

    // 开始扫描
    int count = scan_directory(path);

    if (count < 0) {
        LOG_ERROR("Media scan failed: scan_directory returned error");
        // 通知系统媒体文件扫描失败
        event_notify(EVENT_MEDIA_SCAN_ERROR, NULL);
        return count;
    }

    // 对媒体文件列表进行排序
    if (count > 1) {
        qsort(g_media_files, count, sizeof(g_media_files[0]), compare_filenames);
        LOG_INFO("Media files sorted by name");
    }

    LOG_INFO("Media scan completed, found %d audio files", count);

    // 通知系统媒体文件扫描完成，无论是否找到文件
    event_notify(EVENT_MEDIA_SCAN_COMPLETE, &count);

    return count;
}

/**
 * @brief 获取媒体文件列表
 */
int media_scan_get_file_list(char **file_list, int max_count)
{
    if (!g_media_scan_init || !file_list) {
        return 0;
    }

    int count = (g_media_file_count < max_count) ? g_media_file_count : max_count;
    
    for (int i = 0; i < count; i++) {
        file_list[i] = g_media_files[i];
    }

    return count;
}

/**
 * @brief 获取媒体文件总数
 */
int media_scan_get_file_count(void)
{
    return g_media_file_count;
}

/**
 * @brief 获取指定索引的媒体文件路径
 */
const char *media_scan_get_file_path(int index)
{
    if (index >= 0 && index < g_media_file_count) {
        return g_media_files[index];
    }
    return NULL;
}