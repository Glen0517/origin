/*
 * 平台抽象层实现
 * 提供通用的平台接口实现，支持不同MCU平台的移植
 */

#include "platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// 平台状态
static platform_state_t platform_state = PLATFORM_STATE_UNINIT;

// 平台信息
static platform_info_t platform_info = {
    .name = "Generic Platform",
    .mcu_name = "Unknown MCU",
    .clock_freq_hz = PLATFORM_CONFIG_CLOCK_FREQ_HZ,
    .flash_size_kb = 0,
    .ram_size_kb = 0,
    .cpu_id = {0, 0, 0},
    .clock_source = PLATFORM_CLOCK_SOURCE_INTERNAL
};

// 平台特定实现接口（由具体平台提供）
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

/**
 * @brief 默认平台初始化函数
 * @param config 平台初始化配置（默认实现不使用）
 * @return 初始化是否成功
 * @note 这是一个通用的默认实现，实际平台应该提供更具体的初始化逻辑
 */
static bool default_init(const platform_init_config_t *config) {
    // 消除未使用参数警告
    (void)config;
    
    // 设置默认时钟频率并更新状态
    platform_info.clock_freq_hz = PLATFORM_CONFIG_CLOCK_FREQ_HZ;
    platform_state = PLATFORM_STATE_INITIALIZED;
    
    printf("默认平台初始化完成\n");
    return true;
}

/**
 * @brief 默认获取系统时间（毫秒）
 * @return 系统时间（毫秒）
 * @note 这是一个简单实现，仅返回0，实际平台应提供真实的时间值
 */
static uint32_t default_get_time_ms(void) {
    // 简单实现，返回0
    return 0;
}

/**
 * @brief 默认获取系统时间（微秒）
 * @return 系统时间（微秒）
 * @note 这是一个简单实现，仅返回0，实际平台应提供真实的时间值
 */
static uint32_t default_get_time_us(void) {
    // 简单实现，返回0
    return 0;
}

/**
 * @brief 默认毫秒级延时
 * @param ms 延时毫秒数
 * @note 这是一个空实现，实际平台应提供精确的延时机制
 */
static void default_delay_ms(uint32_t ms) {
    // 消除未使用参数警告
    (void)ms;
    
    // 简单实现，空函数
    // 实际应用中应该使用定时器或忙等待实现
}

/**
 * @brief 默认微秒级延时
 * @param us 延时微秒数
 * @note 这是一个空实现，实际平台应提供精确的微秒级延时机制
 */
static void default_delay_us(uint32_t us) {
    // 消除未使用参数警告
    (void)us;
    
    // 简单实现，空函数
    // 实际应用中应该使用DWT计数器或其他高精度定时器实现
}

/**
 * @brief 默认使能全局中断
 * @note 这是一个空实现，实际平台应调用相应的汇编指令
 *       例如在ARM Cortex-M系列上应调用__enable_irq()
 */
static void default_enable_interrupts(void) {
    // 简单实现，空函数
}

/**
 * @brief 默认禁用全局中断
 * @note 这是一个空实现，实际平台应调用相应的汇编指令
 *       例如在ARM Cortex-M系列上应调用__disable_irq()
 */
static void default_disable_interrupts(void) {
    // 简单实现，空函数
}

/**
 * @brief 默认设置系统时钟频率
 * @param source 时钟源（默认实现不使用）
 * @param freq_hz 目标频率（默认实现不使用）
 * @return 实际设置的频率
 * @note 这是一个简单实现，总是返回配置的默认频率
 *       实际平台应根据硬件能力实现时钟配置
 */
static uint32_t default_set_clock_freq(platform_clock_source_t source, uint32_t freq_hz) {
    // 消除未使用参数警告
    (void)source;
    (void)freq_hz;
    
    return PLATFORM_CONFIG_CLOCK_FREQ_HZ;
}

/**
 * @brief 默认获取系统时钟频率
 * @return 当前系统时钟频率
 * @note 这是一个简单实现，返回配置的默认频率
 *       实际平台可能需要读取硬件寄存器获取真实频率
 */
static uint32_t default_get_clock_freq(void) {
    return PLATFORM_CONFIG_CLOCK_FREQ_HZ;
}

/**
 * @brief 默认系统重置
 * @param to_bootloader 是否进入bootloader模式
 * @note 这是一个空实现，实际平台应调用相应的重置指令
 *       例如在ARM Cortex-M系列上应调用NVIC_SystemReset()
 */
static void default_reset(bool to_bootloader) {
    // 消除未使用参数警告
    (void)to_bootloader;
    
    // 简单实现，空函数
}

/**
 * @brief 默认获取CPU ID
 * @param id 存储CPU ID的数组（至少3个元素）
 * @return 获取是否成功
 * @note 这是一个简单实现，返回固定值，实际平台应读取芯片的唯一ID寄存器
 *       例如在STM32上应读取0x1FFF7A10地址开始的寄存器
 */
static bool default_get_cpu_id(uint32_t id[3]) {
    if (id == NULL) {
        return false;
    }
    
    // 返回默认的CPU ID值
    id[0] = 1;
    id[1] = 2;
    id[2] = 3;
    return true;
}

/**
 * @brief 默认获取内存信息
 * @param free_ram 空闲RAM大小
 * @param total_ram 总RAM大小
 * @return 获取是否成功
 * @note 这是一个简单实现，返回固定值，实际平台应根据内存映射提供真实信息
 */
static bool default_get_memory_info(uint32_t *free_ram, uint32_t *total_ram) {
    if (free_ram == NULL || total_ram == NULL) {
        return false;
    }
    
    // 返回默认内存值
    *free_ram = 1024 * 1024;  // 1MB free RAM
    *total_ram = 2 * 1024 * 1024;  // 2MB total RAM
    return true;
}

// 平台特定实现（默认使用通用实现）
static platform_impl_t platform_impl = {
    .init = default_init,
    .get_time_ms = default_get_time_ms,
    .get_time_us = default_get_time_us,
    .delay_ms = default_delay_ms,
    .delay_us = default_delay_us,
    .enable_interrupts = default_enable_interrupts,
    .disable_interrupts = default_disable_interrupts,
    .set_clock_freq = default_set_clock_freq,
    .get_clock_freq = default_get_clock_freq,
    .reset = default_reset,
    .get_cpu_id = default_get_cpu_id,
    .get_memory_info = default_get_memory_info
};

/**
 * @brief 注册平台特定实现
 * @param impl 平台实现函数表
 * @return 是否成功注册
 */
bool platform_register_impl(const platform_impl_t *impl) {
    if (impl == NULL) {
        return false;
    }
    
    // 复制实现函数指针
    memcpy(&platform_impl, impl, sizeof(platform_impl_t));
    return true;
}

bool platform_register_default_impl(void) {
    // 确认芯片平台是什么
    printf("Default platform: %s\n", platform_info.name);
    // 使用已定义的默认实现
    return true;
}

// STM32平台实现
bool platform_register_stm32_impl(void) {
    // 更新平台信息为STM32F4系列
    platform_info.name = "STM32 Platform";
    platform_info.mcu_name = "STM32F407VG";
    platform_info.clock_freq_hz = 168000000;  // 168MHz
    platform_info.flash_size_kb = 1024;       // 1MB Flash
    platform_info.ram_size_kb = 192;          // 192KB RAM
    platform_info.clock_source = PLATFORM_CLOCK_SOURCE_EXTERNAL;
    
    printf("STM32 platform registered: %s\n", platform_info.name);
    
    // 保留默认实现，因为这只是一个DEMO
    return true;
}

/**
 * @brief 初始化平台
 * @param config 平台初始化配置
 * @return 是否成功初始化
 */
bool platform_init(const platform_init_config_t *config) {
    if (platform_state != PLATFORM_STATE_UNINIT) {
        return false;
    }
    
    platform_state = PLATFORM_STATE_INITIALIZING;
    
    // 检查是否有平台特定实现
    if (platform_impl.init == NULL) {
        platform_state = PLATFORM_STATE_ERROR;
        return false;
    }
    
    // 调用平台特定初始化函数
    bool success = platform_impl.init(config);
    
    if (success) {
        platform_state = PLATFORM_STATE_INITIALIZED;
        
        // 更新平台信息
        if (platform_impl.get_clock_freq != NULL) {
            platform_info.clock_freq_hz = platform_impl.get_clock_freq();
        }
        
        if (platform_impl.get_cpu_id != NULL) {
            if (!platform_impl.get_cpu_id(platform_info.cpu_id)) {
                // If get_cpu_id fails, reset cpu_id to zero
                memset(platform_info.cpu_id, 0, sizeof(platform_info.cpu_id));
            }
        }
        
        if (platform_impl.get_memory_info != NULL) {
            uint32_t free_ram, total_ram;
            if (platform_impl.get_memory_info(&free_ram, &total_ram)) {
                platform_info.ram_size_kb = total_ram / 1024;
            }
        }
    } else {
        platform_state = PLATFORM_STATE_ERROR;
    }
    
    return success;
}

/**
 * @brief 获取平台信息
 * @param info 平台信息结构体指针
 * @return 是否成功获取信息
 */
bool platform_get_info(platform_info_t *info) {
    if (info == NULL || platform_state != PLATFORM_STATE_INITIALIZED) {
        return false;
    }
    
    memcpy(info, &platform_info, sizeof(platform_info_t));
    return true;
}

/**
 * @brief 获取当前平台状态
 * @return 平台状态
 */
platform_state_t platform_get_state(void) {
    return platform_state;
}

/**
 * @brief 获取系统当前时间（毫秒）
 * @return 系统时间
 */
uint32_t platform_get_time_ms(void) {
    if (platform_state != PLATFORM_STATE_INITIALIZED || platform_impl.get_time_ms == NULL) {
        // 简化实现：返回0
        return 0;
    }
    
    return platform_impl.get_time_ms();
}

/**
 * @brief 获取系统当前时间（微秒）
 * @return 系统时间
 */
uint32_t platform_get_time_us(void) {
    if (platform_state != PLATFORM_STATE_INITIALIZED || platform_impl.get_time_us == NULL) {
        // 简化实现：返回0
        return 0;
    }
    
    return platform_impl.get_time_us();
}

/**
 * @brief 系统延时（毫秒）
 * @param ms 延时毫秒数
 */
void platform_delay_ms(uint32_t ms) {
    if (platform_state != PLATFORM_STATE_INITIALIZED || platform_impl.delay_ms == NULL) {
        // 简化实现：空操作
        return;
    }
    
    platform_impl.delay_ms(ms);
}

/**
 * @brief 系统延时（微秒）
 * @param us 延时微秒数
 */
void platform_delay_us(uint32_t us) {
    if (platform_state != PLATFORM_STATE_INITIALIZED || platform_impl.delay_us == NULL) {
        // 简化实现：空操作
        return;
    }
    
    platform_impl.delay_us(us);
}

/**
 * @brief 使能全局中断
 */
void platform_enable_interrupts(void) {
    if (platform_state != PLATFORM_STATE_INITIALIZED || platform_impl.enable_interrupts == NULL) {
        // 简化实现：空操作
        return;
    }
    
    platform_impl.enable_interrupts();
}

/**
 * @brief 禁用全局中断
 */
void platform_disable_interrupts(void) {
    if (platform_state != PLATFORM_STATE_INITIALIZED || platform_impl.disable_interrupts == NULL) {
        // 简化实现：空操作
        return;
    }
    
    platform_impl.disable_interrupts();
}

/**
 * @brief 设置系统时钟频率
 * @param source 时钟源
 * @param freq_hz 目标频率
 * @return 实际设置的频率
 */
uint32_t platform_set_clock_freq(platform_clock_source_t source, uint32_t freq_hz) {
    if (platform_state != PLATFORM_STATE_INITIALIZED || platform_impl.set_clock_freq == NULL) {
        // 简化实现：返回当前频率
        return platform_info.clock_freq_hz;
    }
    
    uint32_t actual_freq = platform_impl.set_clock_freq(source, freq_hz);
    if (actual_freq > 0) {
        platform_info.clock_freq_hz = actual_freq;
    }
    
    return actual_freq;
}

/**
 * @brief 获取系统时钟频率
 * @return 当前系统时钟频率
 */
uint32_t platform_get_clock_freq(void) {
    if (platform_state != PLATFORM_STATE_INITIALIZED || platform_impl.get_clock_freq == NULL) {
        return platform_info.clock_freq_hz;
    }
    
    return platform_impl.get_clock_freq();
}

/**
 * @brief 系统重置
 * @param to_bootloader 是否进入bootloader
 */
void platform_reset(bool to_bootloader) {
    if (platform_state != PLATFORM_STATE_INITIALIZED || platform_impl.reset == NULL) {
        // 简化实现：空操作
        return;
    }
    
    platform_impl.reset(to_bootloader);
}

/**
 * @brief 获取CPU ID
 * @param id 存储CPU ID的数组（至少3个元素）
 * @return 是否成功获取
 */
bool platform_get_cpu_id(uint32_t id[3]) {
    if (id == NULL || platform_state != PLATFORM_STATE_INITIALIZED || platform_impl.get_cpu_id == NULL) {
        return false;
    }
    
    return platform_impl.get_cpu_id(id);
}

/**
 * @brief 获取可用内存信息
 * @param free_ram 空闲RAM大小
 * @param total_ram 总RAM大小
 * @return 是否成功获取
 */
bool platform_get_memory_info(uint32_t *free_ram, uint32_t *total_ram) {
    if (free_ram == NULL || total_ram == NULL || 
        platform_state != PLATFORM_STATE_INITIALIZED || 
        platform_impl.get_memory_info == NULL) {
        return false;
    }
    
    return platform_impl.get_memory_info(free_ram, total_ram);
}