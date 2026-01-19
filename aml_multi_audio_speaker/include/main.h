#ifndef __MAIN_H__
#define __MAIN_H__

#include "common_def.h"

#if CONFIG_ENABLE_CALIB_MODE || CONFIG_ENABLE_AGE_TEST

int prod_test_init(void);
int prod_test_deinit(void);
int prod_test_hw_check(void);
int prod_test_age_start(int hour);
int prod_test_age_stop(void);
int prod_test_calib_eq(void);

#endif

#endif // __MAIN_H__