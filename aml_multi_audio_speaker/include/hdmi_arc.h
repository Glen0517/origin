#ifndef __HDMI_ARC_H__
#define __HDMI_ARC_H__

#include "common_def.h"
//仅高 / 中端编译
#if CONFIG_ENABLE_HDMI_ARC

typedef struct {
    bool cec_en;
    bool auto_switch_en;
    int sample_rate;
} HdmiArcConfig_t;

int hdmi_arc_init(HdmiArcConfig_t *cfg);
int hdmi_arc_deinit(void);
bool hdmi_arc_detect_signal(void);
int hdmi_arc_set_audio_format(int fmt);

#endif

#endif // __HDMI_ARC_H__