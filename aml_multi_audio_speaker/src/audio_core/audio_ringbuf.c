#include "audio_core_priv.h"
#include <stdlib.h>
#include <string.h>
#include "logger.h"

static unsigned char *g_ringbuf = NULL;
static unsigned int g_size = 0;
static unsigned int g_wr_idx = 0;
static unsigned int g_rd_idx = 0;

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
    g_size = size;
    memset(g_ringbuf, 0, g_size);
    LOG_INFO("Ringbuf init: %dKB", size/1024);
    return 0;
}

void audio_ringbuf_deinit(void) {
    if (g_ringbuf) {
        free(g_ringbuf);
        g_ringbuf = NULL;
    }
    g_size = g_wr_idx = g_rd_idx = 0;
}

unsigned int audio_ringbuf_write(unsigned char *data, unsigned int len) {
    if (!data || len == 0 || !g_ringbuf) return 0;
    unsigned int free_size = (g_rd_idx > g_wr_idx) ? (g_rd_idx - g_wr_idx - 1) : (g_size - g_wr_idx + g_rd_idx - 1);
    unsigned int write_len = len < free_size ? len : free_size;

    if (g_wr_idx + write_len <= g_size) {
        memcpy(g_ringbuf + g_wr_idx, data, write_len);
    } else {
        unsigned int part1 = g_size - g_wr_idx;
        memcpy(g_ringbuf + g_wr_idx, data, part1);
        memcpy(g_ringbuf, data + part1, write_len - part1);
    }
    g_wr_idx = (g_wr_idx + write_len) % g_size;
    return write_len;
}

unsigned int audio_ringbuf_read(unsigned char *data, unsigned int len) {
    if (!data || len == 0 || !g_ringbuf) return 0;
    unsigned int data_size = (g_wr_idx > g_rd_idx) ? (g_wr_idx - g_rd_idx) : (g_size - g_rd_idx + g_wr_idx);
    unsigned int read_len = len < data_size ? len : data_size;

    if (g_rd_idx + read_len <= g_size) {
        memcpy(data, g_ringbuf + g_rd_idx, read_len);
    } else {
        unsigned int part1 = g_size - g_rd_idx;
        memcpy(data, g_ringbuf + g_rd_idx, part1);
        memcpy(data + part1, g_ringbuf, read_len - part1);
    }
    g_rd_idx = (g_rd_idx + read_len) % g_size;
    return read_len;
}