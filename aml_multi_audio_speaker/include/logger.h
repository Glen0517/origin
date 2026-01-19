#ifndef __LOGGER_H__
#define __LOGGER_H__

#include "common_def.h"
#include "product_type.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************************
 * 日志等级宏定义 - 从高到低，等级越高输出越少，量产仅保留ERROR/WARN
 * 宏控开关：RELEASE模式下关闭DEBUG/INFO，DEBUG模式全开，在lib/compile_flags.mk中定义
 ******************************************************************************************/
typedef enum {
    LOG_LEVEL_NONE    = 0,    // 关闭所有日志
    LOG_LEVEL_ERROR   = 1,    // 错误日志：必须处理的严重错误（如硬件初始化失败、功能执行异常）
    LOG_LEVEL_WARN    = 2,    // 警告日志：非严重异常，不影响功能但需关注（如参数超限、配置项缺失）
    LOG_LEVEL_INFO    = 3,    // 信息日志：正常业务流程（如模块初始化成功、音源切换、音量调节）
    LOG_LEVEL_DEBUG   = 4     // 调试日志：详细调试信息（如函数入参出参、循环状态、寄存器值）
} LogLevel_e;

/******************************************************************************************
 * 全局日志配置 - 与log_config.json联动，宏控适配4类产品，无需手动修改
 ******************************************************************************************/
#define LOG_FILE_PATH            "./log/aml_audio.log"  // 日志文件存储路径
#define LOG_MAX_SIZE_MB          8                       // 单日志文件最大8MB，防止占满FLASH
#define LOG_MAX_BACKUP_COUNT     5                       // 最多保留5个轮转日志文件
#define LOG_PRINT_CONSOLE        1                       // 同时输出到串口控制台
#define LOG_PRINT_FILE           1                       // 同时落地到日志文件

/******************************************************************************************
 * 核心宏控：根据产品类型自动配置日志等级【完美匹配你的4类产品】
 * 低端产品：仅打印ERROR日志，极致精简；高端产品：全开DEBUG日志，调试便利
 * 无需手动修改，编译时自动根据PRODUCT_TYPE切换
 ******************************************************************************************/
#if (CURRENT_PRODUCT_TYPE == PRODUCT_LOW_END_BT_SPEAKER) || (CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER) || (CURRENT_PRODUCT_TYPE == PRODUCT_GAME_LOW_END)
    #define SYS_LOG_LEVEL         LOG_LEVEL_ERROR
#elif (CURRENT_PRODUCT_TYPE == PRODUCT_MID_END_SOUNDBAR) || (CURRENT_PRODUCT_TYPE == PRODUCT_GAME_MID_END)
    #define SYS_LOG_LEVEL         LOG_LEVEL_WARN
#else
    #define SYS_LOG_LEVEL         LOG_LEVEL_DEBUG
#endif

/******************************************************************************************
 * 模块日志宏控：被裁剪的模块，日志宏自动置空，无任何日志输出【完美匹配你的模块裁剪】
 * 如：低端产品关闭HDMI/SPDIF，对应的日志宏直接为空，不编译、不占体积、无冗余
 ******************************************************************************************/
#ifndef CONFIG_ENABLE_HDMI_ARC
    #define LOG_HDMI(level, fmt, ...)
#endif

#ifndef CONFIG_ENABLE_SPDIF
    #define LOG_SPDIF(level, fmt, ...)
#endif

#ifndef CONFIG_ENABLE_DOLBY_DTS
    #define LOG_EFFECT(level, fmt, ...)
#endif

#ifndef CONFIG_ENABLE_BT_MESH
    #define LOG_BT_MESH(level, fmt, ...)
#endif

#ifndef CONFIG_ENABLE_WIFI_MEDIA
    #define LOG_WIFI(level, fmt, ...)
#endif

/******************************************************************************************
 * 【日志模块对外接口】- 标准化工业级实现
 ******************************************************************************************/

/**
 * @brief  日志模块初始化
 * @param  level 日志级别
 * @param  log_path 日志文件路径，NULL则只输出到控制台
 * @return SUCCESS/FAILURE
 */
int log_init(int level, const char *log_path);

/**
 * @brief  日志模块反初始化
 * @return SUCCESS/FAILURE
 */
int log_deinit(void);

/**
 * @brief  设置日志级别
 * @param  level 日志级别
 * @return SUCCESS/FAILURE
 */
int log_set_level(int level);

/**
 * @brief  获取当前日志级别
 * @return 日志级别
 */
int log_get_level(void);

/**
 * @brief  调试日志
 * @param  func 函数名
 * @param  line 行号
 * @param  fmt 格式化字符串
 * @param  ... 可变参数
 */
void log_debug(const char *func, int line, const char *fmt, ...);

/**
 * @brief  信息日志
 * @param  func 函数名
 * @param  line 行号
 * @param  fmt 格式化字符串
 * @param  ... 可变参数
 */
void log_info(const char *func, int line, const char *fmt, ...);

/**
 * @brief  警告日志
 * @param  func 函数名
 * @param  line 行号
 * @param  fmt 格式化字符串
 * @param  ... 可变参数
 */
void log_warn(const char *func, int line, const char *fmt, ...);

/**
 * @brief  错误日志
 * @param  func 函数名
 * @param  line 行号
 * @param  fmt 格式化字符串
 * @param  ... 可变参数
 */
void log_error(const char *func, int line, const char *fmt, ...);

/**
 * @brief  日志核心打印函数（被日志宏封装，src无需直接调用）
 */
void log_print(LogLevel_e level, const char *file, int line, const char *func, const char *fmt, ...);

/**
 * @brief  日志系统初始化：创建日志目录、初始化互斥锁、加载日志配置，在main.c中调用一次即可
 */
int log_system_init(void);

/**
 * @brief  日志系统反初始化：关闭日志文件、释放锁资源，程序退出时调用
 */
void log_system_deinit(void);

/******************************************************************************************
 * 日志格式化宏：自动拼接【时间+模块+等级+文件名+行号】，一键定位问题，无需手动拼接
 * 核心：src业务层直接调用以下宏即可，无需调用任何函数，极致简洁！
 ******************************************************************************************/
#define LOG_BASE(level, fmt, ...)  log_print(level, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)        do{ if(SYS_LOG_LEVEL >= LOG_LEVEL_ERROR) LOG_BASE(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__); }while(0)
#define LOG_WARN(fmt, ...)         do{ if(SYS_LOG_LEVEL >= LOG_LEVEL_WARN)  LOG_BASE(LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__); }while(0)
#define LOG_INFO(fmt, ...)         do{ if(SYS_LOG_LEVEL >= LOG_LEVEL_INFO)  LOG_BASE(LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__); }while(0)
#define LOG_DEBUG(fmt, ...)        do{ if(SYS_LOG_LEVEL >= LOG_LEVEL_DEBUG) LOG_BASE(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__); }while(0)

/******************************************************************************************
 * 模块专属日志宏：针对核心模块的独立日志，便于问题分类排查，与模块裁剪联动
 ******************************************************************************************/
#define LOG_AUDIO(fmt, ...)        LOG_DEBUG("[AUDIO] " fmt, ##__VA_ARGS__)
#define LOG_BLUETOOTH(fmt, ...)    LOG_DEBUG("[BT] " fmt, ##__VA_ARGS__)
#define LOG_HDMI(fmt, ...)         LOG_DEBUG("[HDMI] " fmt, ##__VA_ARGS__)
#define LOG_SPDIF(fmt, ...)        LOG_DEBUG("[SPDIF] " fmt, ##__VA_ARGS__)
#define LOG_KEY(fmt, ...)          LOG_DEBUG("[KEY] " fmt, ##__VA_ARGS__)
#define LOG_STORAGE(fmt, ...)      LOG_DEBUG("[STORAGE] " fmt, ##__VA_ARGS__)

/******************************************************************************************
 * 兼容原有日志宏，保持向后兼容
 ******************************************************************************************/
#ifndef LOG_LEVEL_DEBUG
#define LOG_LEVEL_DEBUG     4
#endif

#ifndef LOG_LEVEL_INFO
#define LOG_LEVEL_INFO      3
#endif

#ifndef LOG_LEVEL_WARN
#define LOG_LEVEL_WARN      2
#endif

#ifndef LOG_LEVEL_ERROR
#define LOG_LEVEL_ERROR     1
#endif

#ifdef __cplusplus
}
#endif

#endif // __LOGGER_H__
