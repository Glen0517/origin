#ifndef __AUDIO_CORE_PRIV_H__
#define __AUDIO_CORE_PRIV_H__

#include "audio_core.h"
#include "product_type.h"

typedef enum {
    AUDIO_DECODE_PCM = 0,
    AUDIO_DECODE_MP3,
    AUDIO_DECODE_AAC
} AudioDecodeType_e;

typedef struct {
    AudioDecodeType_e type;
    int dolby_en;
    int dts_en;
    int init_ok;
} AudioDecodeCfg_t;

typedef struct {
    int channels;
    int sample_rate;
    int gain;
    int init_ok;
} AudioMixerCfg_t;

// 内部函数声明
int audio_decode_init(void);
void audio_decode_deinit(void);
int audio_mixer_init(void);
void audio_mixer_deinit(void);
int audio_ringbuf_init(unsigned int size);
void audio_ringbuf_deinit(void);
unsigned int audio_ringbuf_write(unsigned char *data, unsigned int len);
unsigned int audio_ringbuf_read(unsigned char *data, unsigned int len);
unsigned int audio_ringbuf_get_free_space(void);
unsigned int audio_ringbuf_get_used_space(void);

#endif // __AUDIO_CORE_PRIV_H__