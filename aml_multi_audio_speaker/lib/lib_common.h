#ifndef __LIB_COMMON_H__
#define __LIB_COMMON_H__

#include "product_type.h"
#include "common_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************************
 * 库版本宏定义 - 量产溯源必备 统一管理第三方库/SDK库版本
 ******************************************************************************************/
#define LIB_VERSION_AML_SDK      "3.0.0"
#define LIB_VERSION_ALSA         "1.2.8"
#define LIB_VERSION_JSON_C       "0.15"
#define LIB_VERSION_BLUETOOTH    "5.0"
#define LIB_VERSION_DOLBY        "7.1"

/******************************************************************************************
 * 库链接宏定义 - 与lib_deps.mk完全一致 供src代码条件编译使用
 ******************************************************************************************/
#define LIB_LINK_HDMI            CONFIG_ENABLE_HDMI_ARC
#define LIB_LINK_SPDIF           CONFIG_ENABLE_SPDIF
#define LIB_LINK_DOLBY           CONFIG_ENABLE_DOLBY_DTS
#define LIB_LINK_BT_MESH         CONFIG_ENABLE_BT_MESH
#define LIB_LINK_WIFI            CONFIG_ENABLE_WIFI_MEDIA
#define LIB_LINK_IR              CONFIG_ENABLE_IR_LEARN

/******************************************************************************************
 * 库加载状态枚举 - 供src代码检测库依赖是否正常 统一错误码
 ******************************************************************************************/
typedef enum {
    LIB_LOAD_SUCCESS      = 0,    // 库加载成功
    LIB_LOAD_FAIL         = -1,   // 库加载失败
    LIB_LOAD_NOT_SUPPORT  = -2,   // 库不支持当前产品
    LIB_LOAD_VERSION_ERR  = -3    // 库版本不匹配
} LibLoadState_e;

/******************************************************************************************
 * 库依赖检测接口 - 供src模块初始化时调用 检测库是否存在/可用
 ******************************************************************************************/
LibLoadState_e lib_check_dependency(const char *lib_name);

#ifdef __cplusplus
}
#endif

#endif // __LIB_COMMON_H__