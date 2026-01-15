#ifndef __AUDIO_SOURCE_PRIV_H__
#define __AUDIO_SOURCE_PRIV_H__

#include "audio_source.h"
#include "product_type.h"

int src_bt_init(void);
void src_bt_deinit(void);
int src_hdmi_init(void);
void src_hdmi_deinit(void);
int src_spdif_init(void);
void src_spdif_deinit(void);
int src_usb_init(void);
void src_usb_deinit(void);
int src_aux_init(void);
void src_aux_deinit(void);
int src_uac_init(void);  // 新增：UAC音源
void src_uac_deinit(void);
int src_wifi_init(void); // 新增：WIFI音源
void src_wifi_deinit(void);

#endif // __AUDIO_SOURCE_PRIV_H__