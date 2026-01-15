#ifndef __RES_MANAGER_H__
#define __RES_MANAGER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "product_type.h"
#include "common_def.h"
#include "logger.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************************
 * 资源类型枚举定义 - 标准化所有资源类型，src业务层按需调用，无硬编码资源标识
 * 覆盖嵌入式音响所有刚需资源，与assets目录资源类型一一对应，后期扩展资源仅需新增枚举即可
 ******************************************************************************************/
typedef enum {
    // 提示音资源：所有产品必备，优先级最高
    RES_TYPE_TONE_BOOT = 0,        // 开机提示音
    RES_TYPE_TONE_CONNECT,         // 蓝牙连接成功提示音
    RES_TYPE_TONE_DISCONNECT,      // 蓝牙断开提示音
    RES_TYPE_TONE_VOL_UP,          // 音量+提示音
    RES_TYPE_TONE_VOL_DOWN,        // 音量-提示音
    RES_TYPE_TONE_SRC_SWITCH,      // 音源切换提示音
    RES_TYPE_TONE_ERROR,           // 错误提示音（如操作失败）
    // 字体资源：显示用，低端用基础字体，高端用高清字体
    RES_TYPE_FONT_DEFAULT,         // 默认显示字体
    RES_TYPE_FONT_HD,              // 高清显示字体（仅高端）
    // EQ音效预设资源：与产品分级/EQ开关联动
    RES_TYPE_EQ_DEFAULT,           // 默认EQ预设（所有产品）
    RES_TYPE_EQ_ROCK,              // 摇滚EQ（中/高端）
    RES_TYPE_EQ_CLASSIC,           // 古典EQ（中/高端）
    RES_TYPE_EQ_DOLBY,             // 杜比EQ预设（仅高端，需CONFIG_ENABLE_DOLBY_DTS）
    RES_TYPE_EQ_BASS,              // 低音增强EQ（低音炮专属）
    // 图标资源：高端产品大屏显示用，低端无大屏自动屏蔽
    RES_TYPE_ICON_BT,              // 蓝牙图标
    RES_TYPE_ICON_HDMI,            // HDMI图标（需CONFIG_ENABLE_HDMI_ARC）
    RES_TYPE_ICON_SPDIF,           // 光纤图标（需CONFIG_ENABLE_SPDIF）
    RES_TYPE_ICON_USB,             // U盘音源图标
    // 最大资源类型，用于边界校验
    RES_TYPE_MAX
} ResType_e;

/******************************************************************************************
 * 资源结构体定义 - 封装资源的核心信息，src业务层仅需操作此结构体，无需关心底层文件
 * 资源加载成功后返回此结构体，包含资源数据、大小、路径，统一管理无冗余
 ******************************************************************************************/
typedef struct {
    char res_path[256];            // 资源文件完整路径
    unsigned char *res_data;       // 资源数据缓存指针（内存中）
    unsigned int res_size;         // 资源数据大小（字节）
    int res_valid;                 // 资源有效性标识：1=有效，0=无效/缺失
} ResInfo_t;

/******************************************************************************************
 * 核心宏控：资源加载开关 - 与产品类型/模块裁剪宏完全联动【核心，无硬编码】
 * 被裁剪的模块，其对应的资源宏自动置空，资源加载接口直接返回默认资源，无任何冗余操作
 * 完美匹配你的include/product_type.h，无需手动修改，编译时自动生效
 ******************************************************************************************/
// 模块资源开关：HDMI/SPDIF/杜比等模块关闭，则对应资源不加载
#ifndef CONFIG_ENABLE_HDMI_ARC
    #define RES_LOAD_HDMI_ICON      0
#else
    #define RES_LOAD_HDMI_ICON      1
#endif

#ifndef CONFIG_ENABLE_SPDIF
    #define RES_LOAD_SPDIF_ICON     0
#else
    #define RES_LOAD_SPDIF_ICON     1
#endif

#ifndef CONFIG_ENABLE_DOLBY_DTS
    #define RES_LOAD_DOLBY_EQ       0
#else
    #define RES_LOAD_DOLBY_EQ       1
#endif

#ifndef CONFIG_ENABLE_BT_MESH
    #define RES_LOAD_BASS_EQ        0
#else
    #define RES_LOAD_BASS_EQ        1
#endif

// 产品资源级别：低端/低音炮仅加载基础资源，中/高端加载全量资源
#if (CURRENT_PRODUCT_TYPE == PRODUCT_LOW_END) || (CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER)
    #define RES_LOAD_LEVEL          1   // 基础资源级别：仅加载必备资源
#elif (CURRENT_PRODUCT_TYPE == PRODUCT_MID_END)
    #define RES_LOAD_LEVEL          2   // 中级资源级别：加载基础+部分音效资源
#else
    #define RES_LOAD_LEVEL          3   // 高级资源级别：加载全量资源
#endif

/******************************************************************************************
 * 资源管理核心接口声明（src业务层唯一需要调用的接口，极简、标准化、零学习成本）
 * 所有接口封装完整，无底层细节暴露，src仅需调用以下接口即可完成所有资源操作
 ******************************************************************************************/
/**
 * @brief 资源管理器初始化 - 项目启动核心接口
 * @note 1. 在main.c中调用，且仅调用一次，在日志初始化之后、业务模块初始化之前
 * @note 2. 初始化逻辑：创建资源目录、加载资源配置、初始化资源缓存池、校验公共资源
 * @return 0=成功，-1=失败
 */
int res_manager_init(void);

/**
 * @brief 资源管理器反初始化 - 项目退出核心接口
 * @note 释放所有缓存的资源数据、清空内存、关闭资源句柄，无内存泄漏
 */
void res_manager_deinit(void);

/**
 * @brief 加载指定类型的资源 - src业务层核心调用接口【最常用】
 * @param type 资源类型，取值ResType_e枚举
 * @return ResInfo_t 资源信息结构体，加载失败则返回默认兜底资源
 * @note src业务层无需关心资源路径/产品类型，传入枚举即可，自动匹配加载对应资源
 */
ResInfo_t res_load_resource(ResType_e type);

/**
 * @brief 释放单个资源的缓存 - 按需释放内存，嵌入式内存优化
 * @param res 资源信息结构体指针
 */
void res_free_resource(ResInfo_t *res);

/**
 * @brief 获取资源文件的完整路径 - 辅助接口，用于资源文件校验
 * @param type 资源类型
 * @param path_buf 路径缓存缓冲区
 * @param buf_size 缓冲区大小
 * @return 0=成功，-1=失败
 */
int res_get_resource_path(ResType_e type, char *path_buf, int buf_size);

#ifdef __cplusplus
}
#endif

#endif // __RES_MANAGER_H__