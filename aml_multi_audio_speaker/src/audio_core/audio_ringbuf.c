#include "audio_core_priv.h"
#include <stdlib.h>
#include <string.h>
#include "logger.h"
#include "common_def.h"

static unsigned char *g_ringbuf = NULL;
static unsigned int g_size = 0;
static unsigned int g_wr_idx = 0;
static unsigned int g_rd_idx = 0;
static pthread_mutex_t g_ringbuf_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_ringbuf_init = false;

int audio_ringbuf_init(unsigned int size) {
    if (size == 0) {
        LOG_ERROR("Ringbuf size invalid");
        return -1;
    }
    
    // 检查是否已经初始化
    if (g_ringbuf_init) {
        LOG_WARN("Ringbuf already initialized, reinitializing");
        audio_ringbuf_deinit();
    }
    
    // 确保大小是2的幂，以优化取模操作
    unsigned int aligned_size = 1;
    while (aligned_size < size) {
        aligned_size <<= 1;
    }
    
    g_ringbuf = (unsigned char *)malloc(aligned_size);
    if (!g_ringbuf) {
        LOG_ERROR("Ringbuf malloc failed");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_ringbuf_mutex);
    g_size = aligned_size;
    g_wr_idx = 0;
    g_rd_idx = 0;
    memset(g_ringbuf, 0, g_size);
    g_ringbuf_init = true;
    MUTEX_LOCK_UNLOCK(g_ringbuf_mutex);
    
    LOG_INFO("Ringbuf init: %dKB (aligned to %dKB)", size/1024, aligned_size/1024);
    return 0;
}

void audio_ringbuf_deinit(void) {
    MUTEX_LOCK_LOCK(g_ringbuf_mutex);
    if (g_ringbuf) {
        free(g_ringbuf);
        g_ringbuf = NULL;
    }
    g_size = g_wr_idx = g_rd_idx = 0;
    g_ringbuf_init = false;
    MUTEX_LOCK_UNLOCK(g_ringbuf_mutex);
    
    MUTEX_LOCK_DESTROY(g_ringbuf_mutex);
}

/**
 * @brief 调整环形缓冲区大小
 * @param size 新的缓冲区大小
 * @return 操作结果：0表示成功，非0表示失败
 */
int audio_ringbuf_resize(unsigned int size) {
    if (size == 0) {
        LOG_ERROR("Ringbuf size invalid");
        return -1;
    }
    
    // 确保大小是2的幂，以优化取模操作
    unsigned int aligned_size = 1;
    while (aligned_size < size) {
        aligned_size <<= 1;
    }
    
    unsigned char *new_buf = (unsigned char *)malloc(aligned_size);
    if (!new_buf) {
        LOG_ERROR("Ringbuf resize malloc failed");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_ringbuf_mutex);
    
    // 复制现有数据到新缓冲区
    unsigned int used_space = (g_wr_idx >= g_rd_idx) ? (g_wr_idx - g_rd_idx) : (g_size - g_rd_idx + g_wr_idx);
    unsigned int copy_size = (used_space < aligned_size) ? used_space : (aligned_size - 1);
    
    if (copy_size > 0) {
        if (g_wr_idx >= g_rd_idx) {
            // 数据在缓冲区的连续部分
            memcpy(new_buf, g_ringbuf + g_rd_idx, copy_size);
        } else {
            // 数据环绕缓冲区
            unsigned int part1 = g_size - g_rd_idx;
            unsigned int part2 = copy_size - part1;
            memcpy(new_buf, g_ringbuf + g_rd_idx, part1);
            memcpy(new_buf + part1, g_ringbuf, part2);
        }
    }
    
    // 释放旧缓冲区
    if (g_ringbuf) {
        free(g_ringbuf);
    }
    
    // 更新缓冲区信息
    g_ringbuf = new_buf;
    g_size = aligned_size;
    g_wr_idx = copy_size;
    g_rd_idx = 0;
    
    MUTEX_LOCK_UNLOCK(g_ringbuf_mutex);
    
    LOG_INFO("Ringbuf resized: %dKB (aligned to %dKB), preserved %d bytes", 
             size/1024, aligned_size/1024, copy_size);
    return 0;
}

unsigned int audio_ringbuf_write(unsigned char *data, unsigned int len) {
    if (!data || len == 0 || !g_ringbuf) return 0;
    
    unsigned int free_size = 0;
    unsigned int write_len = 0;
    
    MUTEX_LOCK_LOCK(g_ringbuf_mutex);
    
    // 计算可用空间
    free_size = (g_rd_idx > g_wr_idx) ? (g_rd_idx - g_wr_idx - 1) : (g_size - g_wr_idx + g_rd_idx - 1);
    write_len = len < free_size ? len : free_size;
    
    if (write_len > 0) {
        if (g_wr_idx + write_len <= g_size) {
            memcpy(g_ringbuf + g_wr_idx, data, write_len);
        } else {
            unsigned int part1 = g_size - g_wr_idx;
            memcpy(g_ringbuf + g_wr_idx, data, part1);
            memcpy(g_ringbuf, data + part1, write_len - part1);
        }
        g_wr_idx = (g_wr_idx + write_len) % g_size;
    }
    
    MUTEX_LOCK_UNLOCK(g_ringbuf_mutex);
    
    return write_len;
}

unsigned int audio_ringbuf_read(unsigned char *data, unsigned int len) {
    if (!data || len == 0 || !g_ringbuf) return 0;
    
    unsigned int data_size = 0;
    unsigned int read_len = 0;
    
    MUTEX_LOCK_LOCK(g_ringbuf_mutex);
    
    // 计算已用空间
    data_size = (g_wr_idx > g_rd_idx) ? (g_wr_idx - g_rd_idx) : (g_size - g_rd_idx + g_wr_idx);
    read_len = len < data_size ? len : data_size;
    
    if (read_len > 0) {
        if (g_rd_idx + read_len <= g_size) {
            memcpy(data, g_ringbuf + g_rd_idx, read_len);
        } else {
            unsigned int part1 = g_size - g_rd_idx;
            memcpy(data, g_ringbuf + g_rd_idx, part1);
            memcpy(data + part1, g_ringbuf, read_len - part1);
        }
        g_rd_idx = (g_rd_idx + read_len) % g_size;
    }
    
    MUTEX_LOCK_UNLOCK(g_ringbuf_mutex);
    
    return read_len;
}

unsigned int audio_ringbuf_get_free_space(void) {
    if (!g_ringbuf) return 0;
    
    unsigned int free_size = 0;
    
    MUTEX_LOCK_LOCK(g_ringbuf_mutex);
    free_size = (g_rd_idx > g_wr_idx) ? (g_rd_idx - g_wr_idx - 1) : (g_size - g_wr_idx + g_rd_idx - 1);
    MUTEX_LOCK_UNLOCK(g_ringbuf_mutex);
    
    return free_size;
}

unsigned int audio_ringbuf_get_used_space(void) {
    if (!g_ringbuf) return 0;
    
    unsigned int used_size = 0;
    
    MUTEX_LOCK_LOCK(g_ringbuf_mutex);
    used_size = (g_wr_idx > g_rd_idx) ? (g_wr_idx - g_rd_idx) : (g_size - g_rd_idx + g_wr_idx);
    MUTEX_LOCK_UNLOCK(g_ringbuf_mutex);
    
    return used_size;
}