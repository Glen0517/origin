/*
 * 平台抽象层定义
 * 提供通用的平台接口，使系统可以移植到不同的MCU上
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "../include/types.h"

// 平台配置宏定义
#define PLATFORM_CONFIG_CLOCK_FREQ_HZ     (72000000U)  // 系统时钟频率
#define PLATFORM_CONFIG_TIMER_FREQ_HZ     (1000U)      // 系统定时器频率
#define PLATFORM_CONFIG_TICK_PER_MS       (PLATFORM_CONFIG_CLOCK_FREQ_HZ / 1000U)  // 每毫秒的时钟周期数
#define PLATFORM_CONFIG_MAX_GPIO_PORTS    (12U)        // 最大GPIO端口数
#define PLATFORM_CONFIG_MAX_GPIO_PINS     (16U)        // 最大GPIO引脚数

// 平台初始化状态
typedef enum {
    PLATFORM_STATE_UNINIT = 0,
    PLATFORM_STATE_INITIALIZING,
    PLATFORM_STATE_INITIALIZED,
    PLATFORM_STATE_ERROR
} platform_state_t;

// 平台时钟源
typedef enum {
    PLATFORM_CLOCK_SOURCE_INTERNAL = 0,
    PLATFORM_CLOCK_SOURCE_EXTERNAL,
    PLATFORM_CLOCK_SOURCE_PLL
} platform_clock_source_t;

// 平台信息结构
typedef struct {
    const char *name;                   // 平台名称
    const char *mcu_name;               // MCU名称
    uint32_t clock_freq_hz;             // 系统时钟频率
    uint32_t flash_size_kb;             // Flash大小
    uint32_t ram_size_kb;               // RAM大小
    uint32_t cpu_id[3];                 // CPU ID
    platform_clock_source_t clock_source; // 时钟源
} platform_info_t;

// 平台初始化配置
typedef struct {
    platform_clock_source_t clock_source; // 时钟源选择
    uint32_t target_freq_hz;            // 目标频率
    bool use_cache;                     // 是否使用缓存
    bool use_fpu;                       // 是否使用FPU
} platform_init_config_t;

/*
 * 平台核心函数接口
 */

/**
 * @brief 初始化平台
 * @param config 平台初始化配置
 * @return 是否成功初始化
 */
bool platform_init(const platform_init_config_t *config);

/**
 * @brief 获取平台信息
 * @param info 平台信息结构体指针
 * @return 是否成功获取信息
 */
bool platform_get_info(platform_info_t *info);

/**
 * @brief 获取当前平台状态
 * @return 平台状态
 */
platform_state_t platform_get_state(void);

/**
 * @brief 获取系统当前时间（毫秒）
 * @return 系统时间
 */
uint32_t platform_get_time_ms(void);

/**
 * @brief 获取系统当前时间（微秒）
 * @return 系统时间
 */
uint32_t platform_get_time_us(void);

/**
 * @brief 系统延时（毫秒）
 * @param ms 延时毫秒数
 */
void platform_delay_ms(uint32_t ms);

/**
 * @brief 系统延时（微秒）
 * @param us 延时微秒数
 */
void platform_delay_us(uint32_t us);

/**
 * @brief 使能全局中断
 */
void platform_enable_interrupts(void);

/**
 * @brief 禁用全局中断
 */
void platform_disable_interrupts(void);

/**
 * @brief 设置系统时钟频率
 * @param source 时钟源
 * @param freq_hz 目标频率
 * @return 实际设置的频率
 */
uint32_t platform_set_clock_freq(platform_clock_source_t source, uint32_t freq_hz);

/**
 * @brief 获取系统时钟频率
 * @return 当前系统时钟频率
 */
uint32_t platform_get_clock_freq(void);

/**
 * @brief 系统重置
 * @param to_bootloader 是否进入bootloader
 */
void platform_reset(bool to_bootloader);

/**
 * @brief 获取CPU ID
 * @param id 存储CPU ID的数组（至少3个元素）
 * @return 是否成功获取
 */
bool platform_get_cpu_id(uint32_t id[3]);

/**
 * @brief 获取可用内存信息
 * @param free_ram 空闲RAM大小
 * @param total_ram 总RAM大小
 * @return 是否成功获取
 */
bool platform_get_memory_info(uint32_t *free_ram, uint32_t *total_ram);

/**
 * @brief 注册默认平台实现
 * 在main函数中调用此函数注册平台实现
 * @return 是否成功注册
 */
bool platform_register_default_impl(void);

#endif /* PLATFORM_H */