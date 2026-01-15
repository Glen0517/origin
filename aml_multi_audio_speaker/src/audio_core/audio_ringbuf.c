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

int audio_ringbuf_init(unsigned int size) {
    if (size == 0) {
        LOG_ERROR("Ringbuf size invalid");
        return -1;
    }
    
    g_ringbuf = (unsigned char *)malloc(size);
    if (!g_ringbuf) {
        LOG_ERROR("Ringbuf malloc failed");
        return -1;
    }
    
    MUTEX_LOCK_LOCK(g_ringbuf_mutex);
    g_size = size;
    g_wr_idx = 0;
    g_rd_idx = 0;
    memset(g_ringbuf, 0, g_size);
    MUTEX_LOCK_UNLOCK(g_ringbuf_mutex);
    
    LOG_INFO("Ringbuf init: %dKB", size/1024);
    return 0;
}

void audio_ringbuf_deinit(void) {
    MUTEX_LOCK_LOCK(g_ringbuf_mutex);
    if (g_ringbuf) {
        free(g_ringbuf);
        g_ringbuf = NULL;
    }
    g_size = g_wr_idx = g_rd_idx = 0;
    MUTEX_LOCK_UNLOCK(g_ringbuf_mutex);
    
    MUTEX_LOCK_DESTROY(g_ringbuf_mutex);
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