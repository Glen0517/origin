#ifndef __SUBWOOFER_COMM_H__
#define __SUBWOOFER_COMM_H__

#include "common_def.h"

#if CONFIG_ENABLE_BT_MESH

typedef struct {
    char bt_name[32];
    int bass_gain;
    bool vol_sync_en;
} SubwooferConfig_t;

int subwoofer_comm_init(SubwooferConfig_t *cfg);
int subwoofer_comm_deinit(void);
bool subwoofer_comm_get_connect_state(void);
int subwoofer_comm_set_bass_gain(int gain);

#endif

#endif // __SUBWOOFER_COMM_H__