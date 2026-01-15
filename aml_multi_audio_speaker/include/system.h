#include "common_def.h"

#ifndef __SYSTEM_API_H__
#define __SYSTEM_API_H__

int system_api_init(void);
int system_api_deinit(void);
SysState_e system_api_get_state(void);
int system_api_set_state(SysState_e state);
int system_api_ota_upgrade(const char *file_path);
int system_api_factory_reset(void);
int system_api_restart(void);

#endif // __SYSTEM_API_H__