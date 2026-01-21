#ifndef __LOGGER_H__
#define __LOGGER_H__

#include "../aml_log.h"
#include "product_type.h"

#ifdef __cplusplus
extern "C" {
#endif

// 声明默认日志分类
AML_LOG_EXTERN(default_log);
#define AML_LOG_DEFAULT AML_LOG_GET_CAT(default_log)

/******************************************************************************************
 * 兼容原有日志宏，保持向后兼容
 ******************************************************************************************/
#define LOG_ERROR(fmt, ...) AML_LOGE(fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  AML_LOGW(fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  AML_LOGI(fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) AML_LOGD(fmt, ##__VA_ARGS__)

/******************************************************************************************
 * 日志等级宏定义 - 从高到低，等级越高输出越少，量产仅保留ERROR/WARN
 ******************************************************************************************/
typedef enum {
    LOG_LEVEL_NONE    = 0,    // 关闭所有日志
    LOG_LEVEL_ERROR   = 1,    // 错误日志：必须处理的严重错误（如硬件初始化失败、功能执行异常）
    LOG_LEVEL_WARN    = 2,    // 警告日志：非严重异常，不影响功能但需关注（如参数超限、配置项缺失）
    LOG_LEVEL_INFO    = 3,    // 信息日志：正常业务流程（如模块初始化成功、音源切换、音量调节）
    LOG_LEVEL_DEBUG   = 4     // 调试日志：详细调试信息（如函数入参出参、循环状态、寄存器值）
} LogLevel_e;

/******************************************************************************************
 * 模块专属日志宏：针对核心模块的独立日志，便于问题分类排查，与模块裁剪联动
 ******************************************************************************************/
#define LOG_AUDIO(fmt, ...)        AML_LOGD("[AUDIO] " fmt, ##__VA_ARGS__)
#define LOG_BLUETOOTH(fmt, ...)    AML_LOGD("[BT] " fmt, ##__VA_ARGS__)
#define LOG_HDMI(fmt, ...)         AML_LOGD("[HDMI] " fmt, ##__VA_ARGS__)
#define LOG_SPDIF(fmt, ...)        AML_LOGD("[SPDIF] " fmt, ##__VA_ARGS__)
#define LOG_KEY(fmt, ...)          AML_LOGD("[KEY] " fmt, ##__VA_ARGS__)
#define LOG_STORAGE(fmt, ...)      AML_LOGD("[STORAGE] " fmt, ##__VA_ARGS__)

/******************************************************************************************
 * 日志系统核心接口声明（无需src业务层调用，日志宏自动封装）
 ******************************************************************************************/
/**
 * @brief 日志系统初始化：创建日志目录、初始化互斥锁、加载日志配置，在main.c中调用一次即可
 */
int log_system_init(void);

/**
 * @brief 日志系统反初始化：关闭日志文件、释放锁资源，程序退出时调用
 */
void log_system_deinit(void);

/**
 * @brief 日志核心打印函数（被日志宏封装，src无需直接调用）
 */
void log_print(LogLevel_e level, const char *file, int line, const char *func, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // __LOGGER_H__