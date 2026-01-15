#ifndef __PRODUCT_TYPE_H__
#define __PRODUCT_TYPE_H__

/******************************************************************************************
 * 【产品类型枚举+宏定义】- 核心！编译时通过该宏切换产品类型，一键编译4类产品固件
 * 取值：1-高端回音壁  2-中端回音壁  3-低端蓝牙音箱  4-独立低音炮
 * 编译指令示例：make PRODUCT=1 编译高端产品，make PRODUCT=4 编译低音炮
 ******************************************************************************************/
#define PRODUCT_HIGH_END_SOUNDBAR    1   // 高端回音壁：全功能+HDMI+光纤+杜比+蓝牙MESH+WIFI投屏
#define PRODUCT_MID_END_SOUNDBAR     2   // 中端回音壁：HDMI+光纤+基础音效+无杜比+无WIFI投屏
#define PRODUCT_LOW_END_BT_SPEAKER   3   // 低端蓝牙音箱：仅蓝牙+U盘+基础播放+单音量
#define PRODUCT_SUBWOOFER            4   // 独立低音炮：仅蓝牙MESH+低音增益+配对功能

// 默认产品类型，编译时通过顶层Makefile传参覆盖：CFLAGS += -DCURRENT_PRODUCT_TYPE=1
#ifndef CURRENT_PRODUCT_TYPE
#define CURRENT_PRODUCT_TYPE         PRODUCT_HIGH_END_SOUNDBAR
#endif

/******************************************************************************************
 * 【产品功能开关宏】- 基于产品类型自动裁剪，所有模块通过该宏做条件编译
 * 规则：CURRENT_PRODUCT_TYPE匹配 → 开启对应功能，否则屏蔽，无冗余代码
 ******************************************************************************************/
// 硬件接口开关
#if (CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END_SOUNDBAR) || (CURRENT_PRODUCT_TYPE == PRODUCT_MID_END_SOUNDBAR)
#define CONFIG_ENABLE_HDMI_ARC       1   // 开启HDMI ARC
#define CONFIG_ENABLE_SPDIF          1   // 开启光纤/同轴
#else
#define CONFIG_ENABLE_HDMI_ARC       0   // 屏蔽HDMI ARC
#define CONFIG_ENABLE_SPDIF          0   // 屏蔽光纤/同轴
#endif

// 音效功能开关
#if CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END_SOUNDBAR
#define CONFIG_ENABLE_DOLBY_DTS      1   // 开启杜比DTS解码
#define CONFIG_ENABLE_VIRTUAL_5_1    1   // 开启虚拟5.1声场
#define CONFIG_ENABLE_WIFI_MEDIA     1   // 开启WIFI投屏
#define CONFIG_ENABLE_IR_LEARN       1   // 开启红外学习
#else
#define CONFIG_ENABLE_DOLBY_DTS      0   // 屏蔽杜比DTS
#define CONFIG_ENABLE_VIRTUAL_5_1    0   // 屏蔽虚拟5.1
#define CONFIG_ENABLE_WIFI_MEDIA     0   // 屏蔽WIFI投屏
#define CONFIG_ENABLE_IR_LEARN       0   // 屏蔽红外学习
#endif

// 蓝牙功能开关
#if (CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END_SOUNDBAR) || (CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER)
#define CONFIG_ENABLE_BT_MESH        1   // 开启蓝牙MESH
#else
#define CONFIG_ENABLE_BT_MESH        0   // 屏蔽蓝牙MESH
#endif

// 音量控制开关
#if CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END_SOUNDBAR
#define CONFIG_ENABLE_3VOL_CTRL      1   // 开启三轨音量(主音量+低音+高音)
#elif CURRENT_PRODUCT_TYPE == PRODUCT_MID_END_SOUNDBAR
#define CONFIG_ENABLE_2VOL_CTRL      1   // 开启双轨音量(主音量+低音)
#else
#define CONFIG_ENABLE_1VOL_CTRL      1   // 开启单轨音量(仅主音量)
#endif

// 产测/校准开关
#if CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END_SOUNDBAR || CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER
#define CONFIG_ENABLE_CALIB_MODE     1   // 开启校准模式
#define CONFIG_ENABLE_AGE_TEST       1   // 开启老化测试
#else
#define CONFIG_ENABLE_CALIB_MODE     0   // 屏蔽校准模式
#define CONFIG_ENABLE_AGE_TEST       0   // 屏蔽老化测试
#endif

// OTA升级开关
#if CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END_SOUNDBAR
#define CONFIG_ENABLE_DUAL_OTA       1   // 开启双路OTA(网络+U盘)
#else
#define CONFIG_ENABLE_USB_OTA        1   // 仅开启U盘OTA
#endif

#endif // __PRODUCT_TYPE_H__