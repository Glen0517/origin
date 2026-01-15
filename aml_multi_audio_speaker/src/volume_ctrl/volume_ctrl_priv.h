#ifndef __VOLUME_CTRL_PRIV_H__
#define __VOLUME_CTRL_PRIV_H__

#include "volume_ctrl.h"
#include "product_type.h"

int main_volume_init(void);
void main_volume_deinit(void);
int channel_volume_init(void);
void channel_volume_deinit(void);

#endif // __VOLUME_CTRL_PRIV_H__