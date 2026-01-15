#ifndef __WIFI_MEDIA_H__
#define __WIFI_MEDIA_H__

#include "common_def.h"

#if CONFIG_ENABLE_WIFI_MEDIA

typedef struct {
    char wifi_name[32];
    bool dlna_en;
    bool airplay_en;
} WifiMediaConfig_t;

int wifi_media_init(WifiMediaConfig_t *cfg);
int wifi_media_deinit(void);
bool wifi_media_get_connect_state(void);

#endif // CONFIG_ENABLE_WIFI_MEDIA

#endif // __WIFI_MEDIA_H__