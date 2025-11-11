/*
 * 平台抽象层默认实现
 * 提供基础的通用实现，适用于大多数平台
 */

// 注意：PLATFORM_CONFIG_CLOCK_FREQ_HZ宏已在platform.h中定义，这里不再重复定义

#include "platform.h"
#include <stdint.h>
#include <string.h>

// 前向声明平台实现结构体，与platform.c中的定义保持一致
typedef struct {
    bool (*init)(const platform_init_config_t *config);
    uint32_t (*get_time_ms)(void);
    uint32_t (*get_time_us)(void);
    void (*delay_ms)(uint32_t ms);
    void (*delay_us)(uint32_t us);
    void (*enable_interrupts)(void);
    void (*disable_interrupts)(void);
    uint32_t (*set_clock_freq)(platform_clock_source_t source, uint32_t freq_hz);
    uint32_t (*get_clock_freq)(void);
    void (*reset)(bool to_bootloader);
    bool (*get_cpu_id)(uint32_t id[3]);
    bool (*get_memory_info)(uint32_t *free_ram, uint32_t *total_ram);
} platform_impl_t;

// 外部函数声明
extern bool platform_register_impl(const platform_impl_t *impl);

// 模拟的系统时间计数器
static volatile uint32_t system_time_ms = 0;
static volatile uint32_t system_time_us = 0;

// 默认平台信息
static platform_info_t default_platform_info = {
    .name = "Generic Platform",
    .mcu_name = "Generic MCU",
    .clock_freq_hz = PLATFORM_CONFIG_CLOCK_FREQ_HZ,
    .flash_size_kb = 512,
    .ram_size_kb = 64,
    .cpu_id = {0xDEADBEEF, 0xCAFEBABE, 0xBAADF00D},
    .clock_source = PLATFORM_CLOCK_SOURCE_INTERNAL
};

/**
 * @brief 默认平台初始化函数
 */
static bool platform_impl_init(const platform_init_config_t *config) {
    // 初始化系统时间计数器
    system_time_ms = 0;
    system_time_us = 0;
    
    // 记录配置信息
    if (config != NULL) {
        default_platform_info.clock_source = config->clock_source;
        if (config->target_freq_hz > 0) {
            default_platform_info.clock_freq_hz = config->target_freq_hz;
        }
    }
    (void)config;  // 避免未使用参数警告
    
    return true;
}

/**
 * @brief 获取系统时间（毫秒）
 */
static uint32_t platform_impl_get_time_ms(void) {
    // 简单递增，实际应用中应使用硬件定时器
    system_time_ms += 10;  // 模拟时间流逝
    return system_time_ms;
}

/**
 * @brief 获取系统时间（微秒）
 */
static uint32_t platform_impl_get_time_us(void) {
    // 简单递增，实际应用中应使用硬件定时器
    system_time_us += 10000;  // 模拟时间流逝
    return system_time_us;
}

/**
 * @brief 毫秒级延时
 */
static void platform_impl_delay_ms(uint32_t ms) {
    // 简单的软件延时实现
    // 注意：在实际应用中应该使用硬件定时器或RTOS的延时函数
    uint32_t i, j;
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 1000; j++) {
            // 空操作，根据实际MCU性能调整循环次数
        }
    }
}

/**
 * @brief 微秒级延时
 */
static void platform_impl_delay_us(uint32_t us) {
    // 简单的软件延时实现
    // 注意：在实际应用中应该使用硬件定时器
    uint32_t i;
    for (i = 0; i < us; i++) {
        // 空操作，根据实际MCU性能调整循环次数
    }
}

/**
 * @brief 使能全局中断
 */
static void platform_impl_enable_interrupts(void) {
    // 注意：这是一个占位实现
    // 在实际应用中，应该使用特定平台的中断使能指令
    // 例如：
    // __enable_irq();  // ARM Cortex-M系列
    // 或者 asm("sti");  // x86
}

/**
 * @brief 禁用全局中断
 */
static void platform_impl_disable_interrupts(void) {
    // 注意：这是一个占位实现
    // 在实际应用中，应该使用特定平台的中断禁用指令
    // 例如：
    // __disable_irq();  // ARM Cortex-M系列
    // 或者 asm("cli");  // x86
}

/**
 * @brief 设置系统时钟频率
 */
static uint32_t platform_impl_set_clock_freq(platform_clock_source_t source, uint32_t freq_hz) {
    // 注意：这是一个占位实现
    // 在实际应用中，应该配置特定平台的时钟系统
    default_platform_info.clock_source = source;
    default_platform_info.clock_freq_hz = freq_hz;
    return freq_hz;
}

/**
 * @brief 获取系统时钟频率
 */
static uint32_t platform_impl_get_clock_freq(void) {
    return default_platform_info.clock_freq_hz;
}

/**
 * @brief 系统重置
 */
static void platform_impl_reset(bool to_bootloader) {
    // 注意：这是一个占位实现
    // 在实际应用中，应该使用特定平台的重置方法
    (void)to_bootloader;  // 避免未使用参数警告
    
    // 示例实现：
    // NVIC_SystemReset();  // ARM Cortex-M系列
    // 或者 while(1);  // 等待看门狗重置
}

/**
 * @brief 获取CPU ID
 */
static bool platform_impl_get_cpu_id(uint32_t id[3]) {
    if (id == NULL) {
        return false;
    }
    
    // 复制默认的CPU ID
    memcpy(id, default_platform_info.cpu_id, 3 * sizeof(uint32_t));
    return true;
}

/**
 * @brief 获取内存信息
 */
static bool platform_impl_get_memory_info(uint32_t *free_ram, uint32_t *total_ram) {
    if (free_ram == NULL || total_ram == NULL) {
        return false;
    }
    
    // 返回默认的内存信息
    *total_ram = default_platform_info.ram_size_kb * 1024;
    *free_ram = *total_ram / 2;  // 假设一半内存可用
    return true;
}

/**
 * @brief 注册默认平台实现
 * 在main函数中调用此函数注册平台实现
 */
bool platform_register_default_impl(void) {
    // 创建平台实现结构体并动态赋值
    platform_impl_t platform_impl;
    
    // 在运行时赋值函数指针
    platform_impl.init = platform_impl_init;
    platform_impl.get_time_ms = platform_impl_get_time_ms;
    platform_impl.get_time_us = platform_impl_get_time_us;
    platform_impl.delay_ms = platform_impl_delay_ms;
    platform_impl.delay_us = platform_impl_delay_us;
    platform_impl.enable_interrupts = platform_impl_enable_interrupts;
    platform_impl.disable_interrupts = platform_impl_disable_interrupts;
    platform_impl.set_clock_freq = platform_impl_set_clock_freq;
    platform_impl.get_clock_freq = platform_impl_get_clock_freq;
    platform_impl.reset = platform_impl_reset;
    platform_impl.get_cpu_id = platform_impl_get_cpu_id;
    platform_impl.get_memory_info = platform_impl_get_memory_info;
    
    return platform_register_impl(&platform_impl);
}