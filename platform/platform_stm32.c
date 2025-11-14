/*
 * STM32平台抽象层实现
 * 支持多系列STM32平台，通过条件编译进行切换
 */

// 平台选择宏定义 - 默认使用STM32F4系列
#ifndef PLATFORM_STM32
#define PLATFORM_STM32
#endif

// STM32系列选择
#ifndef PLATFORM_STM32_FAMILY
#define PLATFORM_STM32_FAMILY    PLATFORM_STM32_F4
#endif

// 系列定义
#define PLATFORM_STM32_F1        1
#define PLATFORM_STM32_F4        4
#define PLATFORM_STM32_H7        7

// 功能模块条件开关
#ifndef PLATFORM_ENABLE_DWT
#define PLATFORM_ENABLE_DWT      1   // 启用DWT计时功能
#endif

#ifndef PLATFORM_ENABLE_IRQ
#define PLATFORM_ENABLE_IRQ      1   // 启用中断支持
#endif

#ifndef PLATFORM_ENABLE_MEMORY_MONITOR
#define PLATFORM_ENABLE_MEMORY_MONITOR 1   // 启用内存监控
#endif

#ifndef PLATFORM_ENABLE_DMA
#define PLATFORM_ENABLE_DMA         1   // 启用DMA功能支持
#endif

#ifndef PLATFORM_MAX_DMA_CHANNELS
#define PLATFORM_MAX_DMA_CHANNELS   8   // 最大DMA通道数量
#endif

#include "platform.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

// 平台错误码定义
#define PLATFORM_ERR_NONE           0       // 无错误
#define PLATFORM_ERR_PARAM          -1      // 参数错误
#define PLATFORM_ERR_INIT           -2      // 初始化错误
#define PLATFORM_ERR_CLK_SETTING    -3      // 时钟设置错误
#define PLATFORM_ERR_RESOURCE       -4      // 资源不足错误
#define PLATFORM_ERR_TIMEOUT        -5      // 超时错误

// 错误计数统计
static volatile uint32_t platform_error_count = 0;
static volatile int32_t last_platform_error = PLATFORM_ERR_NONE;

// 根据不同STM32系列定义时钟频率
#if PLATFORM_STM32_FAMILY == PLATFORM_STM32_F1
    #define STM32_SYSCLK_FREQ_DEFAULT     (72000000U)
    #define STM32_MAX_FREQ                (72000000U)
    #define STM32_HCLK_FREQ               (STM32_SYSCLK_FREQ_DEFAULT)
    #define STM32_PCLK1_FREQ              (STM32_HCLK_FREQ / 2)
    #define STM32_PCLK2_FREQ              (STM32_HCLK_FREQ)
#elif PLATFORM_STM32_FAMILY == PLATFORM_STM32_F4
    #define STM32_SYSCLK_FREQ_DEFAULT     (72000000U)
    #define STM32_MAX_FREQ                (168000000U)
    #define STM32_HCLK_FREQ               (STM32_SYSCLK_FREQ_DEFAULT)
    #define STM32_PCLK1_FREQ              (STM32_HCLK_FREQ / 2)
    #define STM32_PCLK2_FREQ              (STM32_HCLK_FREQ)
#elif PLATFORM_STM32_FAMILY == PLATFORM_STM32_H7
    #define STM32_SYSCLK_FREQ_DEFAULT     (216000000U)
    #define STM32_MAX_FREQ                (400000000U)
    #define STM32_HCLK_FREQ               (STM32_SYSCLK_FREQ_DEFAULT)
    #define STM32_PCLK1_FREQ              (STM32_HCLK_FREQ / 2)
    #define STM32_PCLK2_FREQ              (STM32_HCLK_FREQ)
#else
    #error "未定义有效的STM32系列"
#endif

// 模拟DWT(数据观察点和跟踪单元)寄存器 - 根据功能开关条件编译
#if PLATFORM_ENABLE_DWT
    #define DWT_CTRL                    ((volatile uint32_t *)0xE0001000)
    #define DWT_CYCCNT                  ((volatile uint32_t *)0xE0001004)
    #define DWT_CTRL_CYCCNTENA_Msk      (1UL << 0)
#endif

// 中断相关寄存器和功能 - 根据功能开关条件编译
#if PLATFORM_ENABLE_IRQ
    // 中断相关寄存器模拟 (NVIC)
    #define NVIC_ISER0                  ((volatile uint32_t *)0xE000E100)
    #define NVIC_ICER0                  ((volatile uint32_t *)0xE000E180)
    #define NVIC_ISPR0                  ((volatile uint32_t *)0xE000E200)
    #define NVIC_ICPR0                  ((volatile uint32_t *)0xE000E280)
    #define NVIC_IABR0                  ((volatile uint32_t *)0xE000E300)
    #define NVIC_IPR_BASE               ((volatile uint8_t *)0xE000E400)

    // 中断优先级配置 - 根据不同系列可能有所不同
    #if PLATFORM_STM32_FAMILY == PLATFORM_STM32_F1
        #define NVIC_PRIORITY_BITS      4       // STM32F1使用4位优先级
    #elif PLATFORM_STM32_FAMILY == PLATFORM_STM32_F4
        #define NVIC_PRIORITY_BITS      4       // STM32F4使用4位优先级
    #elif PLATFORM_STM32_FAMILY == PLATFORM_STM32_H7
        #define NVIC_PRIORITY_BITS      4       // STM32H7使用4位优先级
    #endif
    
    #define NVIC_IRQ_PRIO_0             0       // 最高优先级
    #define NVIC_IRQ_PRIO_1             4       // 高优先级
    #define NVIC_IRQ_PRIO_2             8       // 中优先级
    #define NVIC_IRQ_PRIO_3             12      // 低优先级
    #define NVIC_IRQ_PRIO_4             15      // 最低优先级 (默认)

    // STM32常用中断编号定义 - 各系列基本相同
    #define SYSTICK_IRQn                15      // SysTick中断
    #define USART1_IRQn                 37      // USART1中断
    #define USART2_IRQn                 38      // USART2中断
    #define TIM2_IRQn                   28      // TIM2中断
    #define EXTI0_IRQn                  6       // EXTI0中断

    // 中断回调函数类型定义
typedef void (*platform_irq_handler_t)(void *arg);

    // 中断处理结构
typedef struct {
    platform_irq_handler_t handler;
    void *arg;
    uint8_t priority;
    bool enabled;
} platform_irq_desc_t;

    // 最大中断数量 (为了演示，这里只支持有限的几个中断)
    #define MAX_IRQ_HANDLERS            64

    // 中断处理描述数组
    static platform_irq_desc_t irq_handlers[MAX_IRQ_HANDLERS];
#endif

// SysTick相关定义 - 基本所有Cortex-M系列都有
#define SYSTICK_CTRL                ((volatile uint32_t *)0xE000E010)
#define SYSTICK_LOAD                ((volatile uint32_t *)0xE000E014)
#define SYSTICK_VAL                 ((volatile uint32_t *)0xE000E018)
#define SYSTICK_CTRL_ENABLE         (1UL << 0)
#define SYSTICK_CTRL_TICKINT        (1UL << 1)
#define SYSTICK_CTRL_CLKSOURCE      (1UL << 2)
#define SYSTICK_CTRL_COUNTFLAG      (1UL << 16)

// 根据不同STM32系列定义平台信息
#if PLATFORM_STM32_FAMILY == PLATFORM_STM32_F1
    static platform_info_t stm32_platform_info = {
        .name = "STM32F1 Platform",
        .mcu_name = "STM32F103ZE",
        .clock_freq_hz = STM32_SYSCLK_FREQ_DEFAULT,
        .flash_size_kb = 512,
        .ram_size_kb = 64,
        .cpu_id = {0x12345678, 0x87654321, 0xABCDEF01},
        .clock_source = PLATFORM_CLOCK_SOURCE_PLL
    };
#elif PLATFORM_STM32_FAMILY == PLATFORM_STM32_F4
    static platform_info_t stm32_platform_info = {
        .name = "STM32F4 Platform",
        .mcu_name = "STM32F407VG",
        .clock_freq_hz = STM32_SYSCLK_FREQ_DEFAULT,
        .flash_size_kb = 1024,
        .ram_size_kb = 192,
        .cpu_id = {0x12345678, 0x87654321, 0xABCDEF01},
        .clock_source = PLATFORM_CLOCK_SOURCE_PLL
    };
#elif PLATFORM_STM32_FAMILY == PLATFORM_STM32_H7
    static platform_info_t stm32_platform_info = {
        .name = "STM32H7 Platform",
        .mcu_name = "STM32H743VI",
        .clock_freq_hz = STM32_SYSCLK_FREQ_DEFAULT,
        .flash_size_kb = 2048,
        .ram_size_kb = 1024,
        .cpu_id = {0x12345678, 0x87654321, 0xABCDEF01},
        .clock_source = PLATFORM_CLOCK_SOURCE_PLL
    };
#endif

// 内存监控结构体 - 增强的内存使用监控功能
typedef struct {
    uint32_t total_heap_size;    // 总堆内存大小
    uint32_t used_heap_size;     // 已使用堆内存大小
    uint32_t max_heap_usage;     // 最大堆内存使用量
    uint32_t heap_alloc_count;   // 堆分配次数
    uint32_t heap_free_count;    // 堆释放次数
    uint32_t heap_fragmentation; // 堆碎片程度(%)
    uint32_t last_stack_ptr;     // 上一次记录的栈指针
    uint32_t peak_stack_usage;   // 峰值栈使用量
    uint32_t max_stack_size;     // 最大栈大小
} memory_monitor_t;

// DMA配置结构体 - 支持不同STM32系列的DMA功能
typedef struct {
    uint8_t dma_channel;        // DMA通道
    uint8_t dma_stream;         // DMA流(对于F4/H7系列)
    uint32_t periph_addr;       // 外设地址
    uint32_t mem_addr;          // 内存地址
    uint32_t data_size;         // 数据大小(字节)
    uint8_t data_width;         // 数据宽度(8/16/32位)
    uint8_t direction;          // 传输方向(外设到内存/内存到外设/内存到内存)
    uint8_t priority;           // DMA优先级
    bool circular_mode;         // 循环模式
    bool mem_inc;               // 内存地址递增
    bool periph_inc;            // 外设地址递增
} dma_config_t;

// DMA回调函数类型
typedef void (*dma_callback_t)(uint8_t channel, uint8_t event_type, void *user_data);

// DMA通道状态结构体
typedef struct {
    uint8_t channel;            // DMA通道号
    uint8_t stream;             // DMA流号
    bool initialized;           // 是否已初始化
    bool active;                // 是否活动中
    uint32_t transfer_count;    // 传输计数器
    uint32_t error_count;       // 错误计数器
    dma_callback_t callback;    // 回调函数
    void *user_data;            // 用户数据
} dma_channel_info_t;

// 内存监控实例
static memory_monitor_t mem_monitor = {
    .total_heap_size = 0,
    .used_heap_size = 0,
    .max_heap_usage = 0,
    .heap_alloc_count = 0,
    .heap_free_count = 0,
    .heap_fragmentation = 0,
    .last_stack_ptr = 0,
    .peak_stack_usage = 0,
    .max_stack_size = 0,
};

// 系统时间计数器 - 优化版本
static volatile uint32_t system_time_ms = 0;         // 系统时间(毫秒)
static volatile uint32_t system_tick_count = 0;      // SysTick中断计数
static volatile bool systick_initialized = false;
static volatile uint32_t last_cycle_count = 0;       // 用于性能优化的循环计数
static uint32_t clock_cycles_per_us = 0;             // 每微秒的时钟周期数 - 缓存以提高性能

/**
 * @brief 记录平台错误
 * @param err_code 错误码
 * @param func_name 函数名
 */
static void platform_record_error(int32_t err_code, const char *func_name) {
    last_platform_error = err_code;
    platform_error_count++;
    printf("[STM32] 错误: %s(), 错误码: %d\n", func_name, err_code);
}

/**
 * @brief 初始化中断处理系统 - 根据功能开关条件编译
 */
#if PLATFORM_ENABLE_IRQ
static void platform_stm32_init_irq_system(void) {
    printf("[STM32] 初始化中断系统...\n");
    
    // 初始化中断处理描述数组
    for (uint32_t i = 0; i < MAX_IRQ_HANDLERS; i++) {
        irq_handlers[i].handler = NULL;
        irq_handlers[i].arg = NULL;
        irq_handlers[i].priority = NVIC_IRQ_PRIO_4; // 默认最低优先级
        irq_handlers[i].enabled = false;
    }
    
    printf("[STM32] 中断系统初始化完成\n");
}
#endif

/**
 * @brief STM32平台初始化函数
 */
static bool platform_stm32_init(const platform_init_config_t *config) {
    bool success = true;
    printf("[STM32] 平台初始化开始...\n");
    
    // 初始化系统错误状态
    platform_error_count = 0;
    last_platform_error = PLATFORM_ERR_NONE;
    
    // 初始化中断系统 - 根据功能开关条件编译
    #if PLATFORM_ENABLE_IRQ
        platform_stm32_init_irq_system();
    #endif
    
    // 初始化系统时间计数器
    system_time_ms = 0;
    system_tick_count = 0;
    last_cycle_count = 0;
    
    // 预计算每微秒的时钟周期数，用于提高延时函数性能
    clock_cycles_per_us = stm32_platform_info.clock_freq_hz / 1000000;
    if (clock_cycles_per_us == 0) {
        clock_cycles_per_us = 1;  // 防止除零错误
    }
    
    // 初始化内存监控 - 根据功能开关条件编译
    #if PLATFORM_ENABLE_MEMORY_MONITOR
        // 对于不同系列的STM32，设置适当的堆和栈大小
        #if PLATFORM_STM32_FAMILY == PLATFORM_STM32_F1
            mem_monitor.total_heap_size = 8 * 1024;  // F1系列典型8KB堆
            mem_monitor.max_stack_size = 4 * 1024;   // F1系列典型4KB栈
        #elif PLATFORM_STM32_FAMILY == PLATFORM_STM32_F4
            mem_monitor.total_heap_size = 16 * 1024; // F4系列典型16KB堆
            mem_monitor.max_stack_size = 8 * 1024;   // F4系列典型8KB栈
        #elif PLATFORM_STM32_FAMILY == PLATFORM_STM32_H7
            mem_monitor.total_heap_size = 64 * 1024; // H7系列典型64KB堆
            mem_monitor.max_stack_size = 16 * 1024;  // H7系列典型16KB栈
        #else
            mem_monitor.total_heap_size = 8 * 1024;
            mem_monitor.max_stack_size = 4 * 1024;
        #endif
        printf("[STM32] 内存监控初始化完成，堆大小: %u KB，栈大小: %u KB\n", 
               mem_monitor.total_heap_size / 1024, mem_monitor.max_stack_size / 1024);
    #endif
    
    // 配置DWT单元用于精确计时 - 根据功能开关条件编译
    #if PLATFORM_ENABLE_DWT
        // 在真实STM32中，这通常在启动文件中完成
        if (DWT_CTRL != NULL && DWT_CYCCNT != NULL) {
            *DWT_CTRL |= DWT_CTRL_CYCCNTENA_Msk;
            *DWT_CYCCNT = 0;
        } else {
            printf("[STM32] 警告: 无法访问DWT寄存器，精确计时功能可能不可用\n");
            // 仍然继续初始化，不会因为这个而失败
        }
    #endif
    
    // 记录配置信息
    if (config != NULL) {
        // 验证时钟源参数
        if (config->clock_source < PLATFORM_CLOCK_SOURCE_HSI || 
            config->clock_source > PLATFORM_CLOCK_SOURCE_PLL) {
            printf("[STM32] 警告: 无效的时钟源，使用默认值\n");
        } else {
            stm32_platform_info.clock_source = config->clock_source;
        }
        
        // 验证和设置时钟频率
    if (config->target_freq_hz > 0) {
        if (config->target_freq_hz <= STM32_MAX_FREQ) { // 根据不同系列使用不同最大值
            stm32_platform_info.clock_freq_hz = config->target_freq_hz;
        } else {
            printf("[STM32] 警告: 目标频率超出最大范围，使用最大值\n");
            stm32_platform_info.clock_freq_hz = STM32_MAX_FREQ;
        }
    }
    }
    
    // 验证平台信息的有效性
    if (stm32_platform_info.clock_freq_hz == 0) {
        printf("[STM32] 错误: 无效的系统时钟频率\n");
        success = false;
        platform_record_error(PLATFORM_ERR_INIT, __func__);
    }
    
    if (success) {
        printf("[STM32] 平台初始化完成! 系统时钟频率: %u Hz\n", stm32_platform_info.clock_freq_hz);
    } else {
        printf("[STM32] 平台初始化失败!\n");
    }
    
    return success;
}

// =============================================================================
// DMA功能实现 - 支持不同STM32系列的DMA功能
// =============================================================================
#ifdef PLATFORM_ENABLE_DMA

// DMA通道状态数组
static dma_channel_info_t dma_channels[PLATFORM_MAX_DMA_CHANNELS] = {0};

/**
 * @brief 初始化DMA通道
 * @param channel DMA通道号
 * @param config DMA配置结构体
 * @return 初始化是否成功
 */
bool platform_stm32_dma_init(uint8_t channel, dma_config_t *config) {
    // 参数有效性检查
    if (channel >= PLATFORM_MAX_DMA_CHANNELS || config == NULL) {
        platform_record_error(PLATFORM_ERR_PARAM, __func__);
        return false;
    }
    
    // 根据不同STM32系列实现不同的初始化逻辑
#if defined(PLATFORM_STM32_F1)
    // STM32F1系列DMA初始化实现
    printf("[STM32] 初始化F1系列DMA通道 %d\n", channel);
    
    // 这里预留F1系列的DMA初始化代码
    // ...
    
#elif defined(PLATFORM_STM32_F4)
    // STM32F4系列DMA初始化实现
    printf("[STM32] 初始化F4系列DMA流 %d 通道 %d\n", config->dma_stream, channel);
    
    // 这里预留F4系列的DMA初始化代码
    // ...
    
#elif defined(PLATFORM_STM32_H7)
    // STM32H7系列DMA初始化实现
    printf("[STM32] 初始化H7系列DMA流 %d 通道 %d\n", config->dma_stream, channel);
    
    // 这里预留H7系列的DMA初始化代码
    // ...
    
#else
    printf("[STM32] 错误: 不支持的STM32系列\n");
    platform_record_error(PLATFORM_ERR_NOT_SUPPORTED, __func__);
    return false;
#endif
    
    // 更新通道信息
    dma_channels[channel].channel = channel;
    dma_channels[channel].stream = config->dma_stream;
    dma_channels[channel].initialized = true;
    dma_channels[channel].active = false;
    dma_channels[channel].transfer_count = 0;
    dma_channels[channel].error_count = 0;
    dma_channels[channel].callback = NULL;
    dma_channels[channel].user_data = NULL;
    
    printf("[STM32] DMA通道 %d 初始化成功\n", channel);
    return true;
}

/**
 * @brief 反初始化DMA通道
 * @param channel DMA通道号
 * @return 反初始化是否成功
 */
bool platform_stm32_dma_deinit(uint8_t channel) {
    if (channel >= PLATFORM_MAX_DMA_CHANNELS) {
        platform_record_error(PLATFORM_ERR_PARAM, __func__);
        return false;
    }
    
    if (!dma_channels[channel].initialized) {
        printf("[STM32] DMA通道 %d 未初始化\n", channel);
        return false;
    }
    
    // 根据不同STM32系列实现不同的反初始化逻辑
#if defined(PLATFORM_STM32_F1)
    // STM32F1系列DMA反初始化实现
    // ...
#elif defined(PLATFORM_STM32_F4) || defined(PLATFORM_STM32_H7)
    // STM32F4/H7系列DMA反初始化实现
    // ...
#endif
    
    // 重置通道信息
    memset(&dma_channels[channel], 0, sizeof(dma_channel_info_t));
    
    printf("[STM32] DMA通道 %d 反初始化成功\n", channel);
    return true;
}

/**
 * @brief 启动DMA传输
 * @param channel DMA通道号
 * @return 启动是否成功
 */
bool platform_stm32_dma_start(uint8_t channel) {
    if (channel >= PLATFORM_MAX_DMA_CHANNELS) {
        platform_record_error(PLATFORM_ERR_PARAM, __func__);
        return false;
    }
    
    if (!dma_channels[channel].initialized) {
        printf("[STM32] DMA通道 %d 未初始化\n", channel);
        return false;
    }
    
    // 根据不同STM32系列实现不同的启动逻辑
#if defined(PLATFORM_STM32_F1)
    // STM32F1系列DMA启动实现
    // ...
#elif defined(PLATFORM_STM32_F4) || defined(PLATFORM_STM32_H7)
    // STM32F4/H7系列DMA启动实现
    // ...
#endif
    
    dma_channels[channel].active = true;
    printf("[STM32] DMA通道 %d 开始传输\n", channel);
    return true;
}

/**
 * @brief 停止DMA传输
 * @param channel DMA通道号
 * @return 停止是否成功
 */
bool platform_stm32_dma_stop(uint8_t channel) {
    if (channel >= PLATFORM_MAX_DMA_CHANNELS) {
        platform_record_error(PLATFORM_ERR_PARAM, __func__);
        return false;
    }
    
    if (!dma_channels[channel].initialized) {
        printf("[STM32] DMA通道 %d 未初始化\n", channel);
        return false;
    }
    
    // 根据不同STM32系列实现不同的停止逻辑
#if defined(PLATFORM_STM32_F1)
    // STM32F1系列DMA停止实现
    // ...
#elif defined(PLATFORM_STM32_F4) || defined(PLATFORM_STM32_H7)
    // STM32F4/H7系列DMA停止实现
    // ...
#endif
    
    dma_channels[channel].active = false;
    printf("[STM32] DMA通道 %d 停止传输\n", channel);
    return true;
}

/**
 * @brief 检查DMA传输是否完成
 * @param channel DMA通道号
 * @return 传输是否完成
 */
bool platform_stm32_dma_is_complete(uint8_t channel) {
    if (channel >= PLATFORM_MAX_DMA_CHANNELS || !dma_channels[channel].initialized) {
        return false;
    }
    
    // 根据不同STM32系列实现不同的状态检查逻辑
    bool is_complete = false;
#if defined(PLATFORM_STM32_F1)
    // STM32F1系列DMA状态检查实现
    // ...
#elif defined(PLATFORM_STM32_F4) || defined(PLATFORM_STM32_H7)
    // STM32F4/H7系列DMA状态检查实现
    // ...
#endif
    
    return !dma_channels[channel].active || is_complete;
}

/**
 * @brief 设置DMA回调函数
 * @param channel DMA通道号
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 设置是否成功
 */
bool platform_stm32_dma_set_callback(uint8_t channel, dma_callback_t callback, void *user_data) {
    if (channel >= PLATFORM_MAX_DMA_CHANNELS || !dma_channels[channel].initialized) {
        platform_record_error(PLATFORM_ERR_PARAM, __func__);
        return false;
    }
    
    dma_channels[channel].callback = callback;
    dma_channels[channel].user_data = user_data;
    printf("[STM32] DMA通道 %d 回调函数设置成功\n", channel);
    return true;
}

/**
 * @brief 获取DMA通道状态信息
 * @param channel DMA通道号
 * @param info 状态信息结构体指针
 * @return 获取是否成功
 */
bool platform_stm32_dma_get_status(uint8_t channel, dma_channel_info_t *info) {
    if (channel >= PLATFORM_MAX_DMA_CHANNELS || !dma_channels[channel].initialized || info == NULL) {
        platform_record_error(PLATFORM_ERR_PARAM, __func__);
        return false;
    }
    
    // 复制状态信息
    memcpy(info, &dma_channels[channel], sizeof(dma_channel_info_t));
    
    // 根据不同STM32系列更新实时状态
#if defined(PLATFORM_STM32_F1)
    // STM32F1系列DMA状态更新
    // ...
#elif defined(PLATFORM_STM32_F4) || defined(PLATFORM_STM32_H7)
    // STM32F4/H7系列DMA状态更新
    // ...
#endif
    
    return true;
}

/**
 * @brief 配置DMA传输参数
 * @param channel DMA通道号
 * @param mem_addr 内存地址
 * @param periph_addr 外设地址
 * @param size 数据大小
 * @return 配置是否成功
 */
bool platform_stm32_dma_configure_transfer(uint8_t channel, uint32_t mem_addr, uint32_t periph_addr, uint32_t size) {
    if (channel >= PLATFORM_MAX_DMA_CHANNELS || !dma_channels[channel].initialized) {
        platform_record_error(PLATFORM_ERR_PARAM, __func__);
        return false;
    }
    
    // 根据不同STM32系列实现不同的配置逻辑
#if defined(PLATFORM_STM32_F1)
    // STM32F1系列DMA配置实现
    // ...
#elif defined(PLATFORM_STM32_F4) || defined(PLATFORM_STM32_H7)
    // STM32F4/H7系列DMA配置实现
    // ...
#endif
    
    printf("[STM32] DMA通道 %d 配置传输: 内存 0x%08X -> 外设 0x%08X, 大小 %d 字节\n", 
           channel, mem_addr, periph_addr, size);
    return true;
}

#endif // PLATFORM_ENABLE_DMA

/**
 * @brief 配置SysTick定时器
 * @param ticks 定时器计数值
 */
#if PLATFORM_ENABLE_IRQ
static void platform_stm32_systick_config(uint32_t ticks) {
    if (ticks > 0xFFFFFF) {
        ticks = 0xFFFFFF;  // SysTick_LOAD是24位寄存器
        platform_record_error(PLATFORM_ERR_PARAM, __func__);
    }
    
    if (SYSTICK_LOAD != NULL && SYSTICK_CTRL != NULL && SYSTICK_VAL != NULL) {
        // 设置重载值
        *SYSTICK_LOAD = ticks - 1;
        
        // 清除当前计数值
        *SYSTICK_VAL = 0;
        
        // 设置SysTick中断优先级
        platform_stm32_irq_set_priority(SYSTICK_IRQn, NVIC_IRQ_PRIO_1);
        
        // 配置SysTick控制寄存器
        *SYSTICK_CTRL |= SYSTICK_CTRL_CLKSOURCE; // 使用处理器时钟
        *SYSTICK_CTRL |= SYSTICK_CTRL_TICKINT;   // 使能中断
        *SYSTICK_CTRL |= SYSTICK_CTRL_ENABLE;    // 使能SysTick
        
        systick_initialized = true;
        printf("[STM32] SysTick配置完成，计数值: %u\n", ticks);
    } else {
        printf("[STM32] 错误: 无法访问SysTick寄存器\n");
        platform_record_error(PLATFORM_ERR_RESOURCE, __func__);
    }
}

/**
 * @brief SysTick中断处理函数 - 根据功能开关条件编译
 */
#if PLATFORM_ENABLE_IRQ
static void platform_stm32_systick_handler(void) {
    // 性能优化：最小化SysTick中断处理函数的执行时间
    // 1. 先递增计数器，这是最关键的操作
    system_tick_count++;
    system_time_ms++;  // 假设SysTick配置为1ms中断一次
    
    // 2. 检查是否有注册的回调函数
    // 使用局部变量减少访问volatile变量的次数
    platform_irq_handler_t handler = irq_handlers[SYSTICK_IRQn].handler;
    void *arg = irq_handlers[SYSTICK_IRQn].arg;
    
    // 3. 如果有回调函数且中断已启用，则调用
    if (handler != NULL && irq_handlers[SYSTICK_IRQn].enabled) {
        handler(arg);
    }
    
    // 4. 可选：记录中断执行时间用于性能分析
    #if defined(PERFORMANCE_ANALYSIS)
        static uint32_t last_systick_time = 0;
        uint32_t current_time = *DWT_CYCCNT;
        if (last_systick_time != 0) {
            // 计算中断处理时间（周期数）
            uint32_t execution_time = (current_time >= last_systick_time) ? 
                                      (current_time - last_systick_time) : 
                                      (0xFFFFFFFF - last_systick_time + current_time + 1);
            
            // 只在执行时间超过阈值时记录，避免过多的日志输出
            if (execution_time > (clock_cycles_per_us * 10)) {  // 超过10us
                // 注意：在实际应用中，这里不应该使用printf，因为它可能会导致中断延迟
                // 可以考虑使用一个循环缓冲区来存储这些信息，供主线程读取
                printf("[STM32] SysTick处理时间过长: %u 周期\n", execution_time);
            }
        }
        last_systick_time = current_time;
    #endif
}

/**
 * @brief 获取系统时间（毫秒）- 性能优化版本
 * 使用SysTick中断维护的计数器，避免每次调用都进行计算
 */
static uint32_t platform_stm32_get_time_ms(void) {
    // 在真实STM32中，这个函数只是简单地返回一个全局变量
    // 而不是每次都进行计算，这样性能更好
    uint32_t current_time;
    
    // 关闭中断以确保读取的时间值是原子的
    #if PLATFORM_ENABLE_IRQ
        platform_stm32_disable_interrupts();
    #endif
    
    current_time = system_time_ms;
    
    // 重新开启中断
    #if PLATFORM_ENABLE_IRQ
        platform_stm32_enable_interrupts();
    #endif
    
    // 优化：对于模拟器环境，如果SysTick未初始化，我们仍然需要模拟时间流逝
    // 但避免在高性能应用中频繁调用printf
    #if defined(DEBUG) && !defined(NDEBUG)
        static uint32_t last_check_time = 0;
        if (current_time - last_check_time > 1000) {  // 每秒最多检查一次
            last_check_time = current_time;
            if (current_time == 0xFFFFFFFF) {
                // 记录溢出事件但继续工作
                printf("[STM32] 注意: 系统时间计数器溢出\n");
            }
        }
    #endif
    
    return current_time;
}

/**
 * @brief 获取系统时间（微秒）- 根据功能开关条件编译
 */
#if PLATFORM_ENABLE_DWT
static uint32_t platform_stm32_get_time_us(void) {
    // 使用DWT计数器实现微秒级计时 - 性能优化版本
    // 在真实STM32中，这通常使用DWT_CYCCNT实现
    
    // 检查DWT寄存器是否可用
    if (DWT_CYCCNT == NULL) {
        platform_record_error(PLATFORM_ERR_RESOURCE, __func__);
        return 0;  // 返回默认值
    }
    
    // 性能优化：使用缓存的时钟周期数，避免重复计算
    static uint32_t cached_clock_cycles_per_us = 0;
    
    // 只在第一次调用或时钟频率改变时计算
    if (cached_clock_cycles_per_us == 0 || cached_clock_cycles_per_us != clock_cycles_per_us) {
        cached_clock_cycles_per_us = clock_cycles_per_us;
    }
    
    // 读取DWT计数器值
    uint32_t cycles = *DWT_CYCCNT;
    
    // 使用除法优化：对于常用的时钟频率，可以使用位操作代替除法
    #if defined(STM32_OPTIMIZED_US_DELAY)
        // 针对特定频率的优化示例
        if (cached_clock_cycles_per_us == 72) {  // 72MHz
            return cycles / 72;  // 可以考虑使用位操作进一步优化
        } else if (cached_clock_cycles_per_us == 168) {  // 168MHz
            return cycles / 168;
        } else {
            return cycles / cached_clock_cycles_per_us;
        }
    #else
        // 通用实现
        return cycles / cached_clock_cycles_per_us;
    #endif
}
#else
static uint32_t platform_stm32_get_time_us(void) {
    // 如果没有DWT，使用简单的毫秒时间放大
    // 这只是一个备用实现，精度较低
    return platform_stm32_get_time_ms() * 1000;
}
#endif

/**
 * @brief 毫秒级延时 - STM32 HAL风格
 */
static void platform_stm32_delay_ms(uint32_t ms) {
    // 防止不必要的延时
    if (ms == 0) {
        return;
    }
    
    printf("[STM32] 延时: %u ms\n", ms);
    
    // 模拟HAL_Delay()函数
    // 在真实STM32中，这个函数通常使用SysTick实现
    uint32_t start_time = platform_stm32_get_time_ms();
    uint32_t timeout = ms * 110 / 100;  // 添加10%的超时余量
    uint32_t elapsed = 0;
    
    // 使用带有超时检查的循环
    while (elapsed < ms) {
        uint32_t current_time = platform_stm32_get_time_ms();
        
        // 处理计数器溢出情况
        if (current_time < start_time) {
            elapsed = (0xFFFFFFFF - start_time) + current_time + 1;
        } else {
            elapsed = current_time - start_time;
        }
        
        // 超时检测
        if (elapsed > timeout) {
            platform_record_error(PLATFORM_ERR_TIMEOUT, __func__);
            break;
        }
    }
}

/**
 * @brief 微秒级延时 - 根据功能开关条件编译
 */
#if PLATFORM_ENABLE_DWT
static void platform_stm32_delay_us(uint32_t us) {
    // 防止不必要的延时
    if (us == 0) {
        return;
    }
    
    // 检查DWT寄存器是否可用
    if (DWT_CYCCNT == NULL) {
        platform_record_error(PLATFORM_ERR_RESOURCE, __func__);
        return;
    }
    
    // 性能优化：使用缓存的时钟周期数，避免重复计算
    uint32_t cycles_to_wait = us * clock_cycles_per_us;
    
    // 对于非常小的延时（<10us），使用简单的NOP循环可能更准确
    if (us < 10) {
        // 对于微秒级别的小延时，使用精确的NOP循环
        // 预计算需要的NOP数量
        uint32_t nop_count = cycles_to_wait / 3;  // 假设每个NOP需要约3个时钟周期
        
        // 使用内联汇编生成NOP指令，避免编译器优化
        __asm volatile (
            "1: subs %[count], %[count], #1\n"
            "bne 1b\n"
            : [count] "=r" (nop_count)
            : [count] "r" (nop_count)
            : "cc", "memory"
        );
        
        return;
    }
    
    // 对于较长的微秒延时，使用DWT计数器
    uint32_t start_cycles = *DWT_CYCCNT;
    uint32_t target_cycles = start_cycles + cycles_to_wait;
    
    // 优化的延时循环 - 减少计算次数
    // 注意：移除了超时检测，因为微秒级延时不应该出现超时
    while (1) {
        uint32_t current_cycles = *DWT_CYCCNT;
        
        // 检查是否到达目标周期数（考虑溢出）
        if ((current_cycles >= target_cycles && start_cycles <= target_cycles) ||
            (current_cycles < start_cycles && target_cycles < start_cycles)) {
            break;
        }
    }
}

/**
 * @brief 使能全局中断 - STM32 Cortex-M4风格
 */
static void platform_stm32_enable_interrupts(void) {
    printf("STM32 __enable_irq()\n");
    // 在真实STM32中，这是一个内联汇编指令
    // __enable_irq();
}

/**
 * @brief 禁用全局中断 - STM32 Cortex-M4风格
 */
static void platform_stm32_disable_interrupts(void) {
    printf("[STM32] 禁用全局中断\n");
    // 在真实STM32中，这是一个内联汇编指令
    // __disable_irq();
}

/**
 * @brief 通用中断处理函数示例 - 用于演示目的
 * 注：在真实系统中，这通常由启动文件或中断向量表自动处理
 */
static void platform_stm32_irq_handler_demo(uint32_t irq_num) {
    // 检查中断编号是否有效
    if (irq_num >= MAX_IRQ_HANDLERS) {
        return;
    }
    
    // 检查是否注册了处理函数
    if (irq_handlers[irq_num].handler != NULL && irq_handlers[irq_num].enabled) {
        // 调用注册的中断处理函数
        irq_handlers[irq_num].handler(irq_handlers[irq_num].arg);
    } else {
        printf("[STM32] 警告: 未处理的中断 %u\n", irq_num);
    }
    
    // 清除中断挂起位
    platform_stm32_irq_clear_pending(irq_num);
}

/**
 * @brief 设置系统时钟频率 - 根据不同系列使用不同的频率范围
 */
static uint32_t platform_stm32_set_clock_freq(platform_clock_source_t source, uint32_t freq_hz) {
    // 验证时钟源参数
    if (source < PLATFORM_CLOCK_SOURCE_HSI || source > PLATFORM_CLOCK_SOURCE_PLL) {
        printf("[STM32] 错误: 无效的时钟源参数: %d\n", source);
        platform_record_error(PLATFORM_ERR_PARAM, __func__);
        return stm32_platform_info.clock_freq_hz;  // 返回当前频率
    }
    
    // 根据时钟源设置有效频率范围 - 根据不同系列有所不同
    uint32_t max_freq = 0;
    switch (source) {
        case PLATFORM_CLOCK_SOURCE_HSI:
            #if PLATFORM_STM32_FAMILY == PLATFORM_STM32_F1
                max_freq = 8000000;  // F1系列HSI通常为8MHz
            #elif PLATFORM_STM32_FAMILY == PLATFORM_STM32_F4
                max_freq = 16000000;  // F4系列HSI通常为16MHz
            #elif PLATFORM_STM32_FAMILY == PLATFORM_STM32_H7
                max_freq = 64000000;  // H7系列HSI通常为64MHz
            #endif
            break;
        case PLATFORM_CLOCK_SOURCE_HSE:
            max_freq = 25000000;  // 外部高速时钟通常最大25MHz(各系列基本相同)
            break;
        case PLATFORM_CLOCK_SOURCE_PLL:
            max_freq = STM32_MAX_FREQ; // 使用预定义的最大频率
            break;
        default:
            max_freq = STM32_MAX_FREQ; // 默认为最大值
            break;
    }
    
    // 验证频率是否有效
    if (freq_hz == 0) {
        printf("[STM32] 错误: 频率不能为0\n");
        platform_record_error(PLATFORM_ERR_PARAM, __func__);
        return stm32_platform_info.clock_freq_hz;  // 返回当前频率
    }
    
    // 限制频率在有效范围内
    uint32_t new_freq = freq_hz;
    if (freq_hz > max_freq) {
        new_freq = max_freq;
        printf("[STM32] 警告: 频率超出范围，限制为: %u Hz\n", new_freq);
    }
    
    // 更新时钟设置
    stm32_platform_info.clock_source = source;
    stm32_platform_info.clock_freq_hz = new_freq;
    
    printf("[STM32] 时钟设置成功: 源=%d, 频率=%u Hz\n", source, new_freq);
    return new_freq;
}

/**
 * @brief 注册中断处理函数
 * @param irq_num 中断编号
 * @param handler 中断处理函数
 * @param arg 传递给中断处理函数的参数
 * @param priority 中断优先级
 * @return 是否注册成功
 */
#if PLATFORM_ENABLE_IRQ
static bool platform_stm32_irq_register(uint32_t irq_num, platform_irq_handler_t handler, void *arg, uint8_t priority) {
    // 检查中断编号是否有效
    if (irq_num >= MAX_IRQ_HANDLERS) {
        printf("[STM32] 错误: 无效的中断编号: %u\n", irq_num);
        platform_record_error(PLATFORM_ERR_PARAM, __func__);
        return false;
    }
    
    // 检查优先级是否有效
    if (priority > NVIC_IRQ_PRIO_4) {
        priority = NVIC_IRQ_PRIO_4; // 默认为最低优先级
    }
    
    // 保存中断处理函数和参数
    irq_handlers[irq_num].handler = handler;
    irq_handlers[irq_num].arg = arg;
    irq_handlers[irq_num].priority = priority;
    
    // 设置中断优先级
    platform_stm32_irq_set_priority(irq_num, priority);
    
    printf("[STM32] 中断 %u 处理函数注册成功，优先级: %u\n", irq_num, priority);
    return true;
}

/**
 * @brief 设置中断优先级
 * @param irq_num 中断编号
 * @param priority 中断优先级
 */
#if PLATFORM_ENABLE_IRQ
static void platform_stm32_irq_set_priority(uint32_t irq_num, uint8_t priority) {
    if (irq_num >= MAX_IRQ_HANDLERS) {
        platform_record_error(PLATFORM_ERR_PARAM, __func__);
        return;
    }
    
    // 检查优先级是否有效
    if (priority > NVIC_IRQ_PRIO_4) {
        priority = NVIC_IRQ_PRIO_4;
    }
    
    // 在真实STM32中，这里会配置NVIC_IPR寄存器
    // 对于演示目的，我们只更新我们的内部优先级值
    irq_handlers[irq_num].priority = priority;
    
    // 模拟设置NVIC优先级
    if (NVIC_IPR_BASE != NULL) {
        NVIC_IPR_BASE[irq_num] = priority << 4; // 只使用高4位
    }
    
    printf("[STM32] 中断 %u 优先级设置为: %u\n", irq_num, priority);
}

/**
 * @brief 使能中断
 * @param irq_num 中断编号
 */
#if PLATFORM_ENABLE_IRQ
static void platform_stm32_irq_enable(uint32_t irq_num) {
    if (irq_num >= MAX_IRQ_HANDLERS) {
        platform_record_error(PLATFORM_ERR_PARAM, __func__);
        return;
    }
    
    // 模拟NVIC中断使能
    if (NVIC_ISER0 != NULL) {
        uint32_t reg_index = irq_num / 32;
        uint32_t bit_pos = irq_num % 32;
        
        if (reg_index == 0) { // 简化处理，只支持ISER0
            *NVIC_ISER0 = (1UL << bit_pos);
        }
    }
    
    irq_handlers[irq_num].enabled = true;
    printf("[STM32] 中断 %u 已使能\n", irq_num);
}

/**
 * @brief 禁用中断
 * @param irq_num 中断编号
 */
#if PLATFORM_ENABLE_IRQ
static void platform_stm32_irq_disable(uint32_t irq_num) {
    if (irq_num >= MAX_IRQ_HANDLERS) {
        platform_record_error(PLATFORM_ERR_PARAM, __func__);
        return;
    }
    
    // 模拟NVIC中断禁用
    if (NVIC_ICER0 != NULL) {
        uint32_t reg_index = irq_num / 32;
        uint32_t bit_pos = irq_num % 32;
        
        if (reg_index == 0) { // 简化处理，只支持ICER0
            *NVIC_ICER0 = (1UL << bit_pos);
        }
    }
    
    irq_handlers[irq_num].enabled = false;
    printf("[STM32] 中断 %u 已禁用\n", irq_num);
}

/**
 * @brief 清除中断标志
 * @param irq_num 中断编号
 */
#if PLATFORM_ENABLE_IRQ
static void platform_stm32_irq_clear_pending(uint32_t irq_num) {
    if (irq_num >= MAX_IRQ_HANDLERS) {
        platform_record_error(PLATFORM_ERR_PARAM, __func__);
        return;
    }
    
    // 模拟NVIC清除挂起位
    if (NVIC_ICPR0 != NULL) {
        uint32_t reg_index = irq_num / 32;
        uint32_t bit_pos = irq_num % 32;
        
        if (reg_index == 0) { // 简化处理，只支持ICPR0
            *NVIC_ICPR0 = (1UL << bit_pos);
        }
    }
    
    printf("[STM32] 中断 %u 挂起位已清除\n", irq_num);
}

/**
 * @brief 获取中断状态
 * @param irq_num 中断编号
 * @return 中断是否处于活跃状态
 */
#if PLATFORM_ENABLE_IRQ
static bool platform_stm32_irq_is_active(uint32_t irq_num) {
    if (irq_num >= MAX_IRQ_HANDLERS) {
        platform_record_error(PLATFORM_ERR_PARAM, __func__);
        return false;
    }
    
    // 模拟NVIC检查活跃位
    if (NVIC_IABR0 != NULL) {
        uint32_t reg_index = irq_num / 32;
        uint32_t bit_pos = irq_num % 32;
        
        if (reg_index == 0) { // 简化处理，只支持IABR0
            return ((*NVIC_IABR0) & (1UL << bit_pos)) != 0;
        }
    }
    
    return false;
}

/**
 * @brief 获取系统时钟频率 - STM32 SystemCoreClock风格
 */
static uint32_t platform_stm32_get_clock_freq(void) {
    // 在真实STM32中，这通常返回SystemCoreClock变量
    return stm32_platform_info.clock_freq_hz;
}

/**
 * @brief 系统重置 - STM32 NVIC风格
 */
static void platform_stm32_reset(bool to_bootloader) {
    printf("STM32系统重置 %s\n", to_bootloader ? "进入Bootloader" : "");
    
    // 在真实STM32中，这是一个CMSIS函数
    // if (to_bootloader) {
    //     // 跳转到Bootloader
    //     void (*SysMemBootJump)(void) = (void (*)(void))(*((uint32_t *)(0x1FFF0000 + 4)));
    //     // 准备跳转到Bootloader
    //     __disable_irq();
    //     SysMemBootJump();
    // } else {
    //     NVIC_SystemReset();
    // }
}

/**
 * @brief 获取CPU ID - STM32 UID风格
 */
static bool platform_stm32_get_cpu_id(uint32_t id[3]) {
    // 参数有效性检查
    if (id == NULL) {
        printf("[STM32] 错误: CPU ID缓冲区为空\n");
        platform_record_error(PLATFORM_ERR_PARAM, __func__);
        return false;
    }
    
    // 检查CPU ID是否有效
    if (stm32_platform_info.cpu_id[0] == 0 && 
        stm32_platform_info.cpu_id[1] == 0 && 
        stm32_platform_info.cpu_id[2] == 0) {
        printf("[STM32] 警告: CPU ID可能未初始化\n");
    }
    
    // 在真实STM32中，这里会读取芯片的唯一ID寄存器
    // 例如：
    // id[0] = *(uint32_t *)(0x1FFF7A10);
    // id[1] = *(uint32_t *)(0x1FFF7A14);
    // id[2] = *(uint32_t *)(0x1FFF7A18);
    
    // 使用预定义的ID进行模拟
    memcpy(id, stm32_platform_info.cpu_id, 3 * sizeof(uint32_t));
    printf("[STM32] CPU ID: 0x%08X 0x%08X 0x%08X\n", 
           (unsigned int)id[0], (unsigned int)id[1], (unsigned int)id[2]);
    return true;
}

/**
 * @brief 更新栈使用情况统计
 * 使用栈指针跟踪当前栈使用量
 */
#if PLATFORM_ENABLE_MEMORY_MONITOR
static void platform_stm32_update_stack_usage(void) {
    uint32_t current_stack_ptr;
    
    // 读取当前栈指针
    __asm volatile ("mov %0, sp" : "=r" (current_stack_ptr));
    
    // 更新最后记录的栈指针
    mem_monitor.last_stack_ptr = current_stack_ptr;
    
    // 在实际应用中，我们需要知道栈的起始地址来计算使用量
    // 这里假设栈从某个地址开始向下增长
    // 对于真实应用，可以从链接脚本中获取栈的起始地址
    #if defined(__STACK_START) && defined(__STACK_SIZE)
        // 如果定义了栈的起始地址和大小
        uint32_t stack_usage = (uint32_t)__STACK_START - current_stack_ptr;
        if (stack_usage > mem_monitor.peak_stack_usage) {
            mem_monitor.peak_stack_usage = stack_usage;
        }
    #else
        // 模拟实现，仅记录栈指针位置
        // 在实际使用中，建议从链接脚本获取栈信息
        static uint32_t simulated_stack_start = 0x20000000 + 64 * 1024;  // 模拟栈起始地址
        uint32_t stack_usage = simulated_stack_start - current_stack_ptr;
        if (stack_usage > mem_monitor.peak_stack_usage) {
            mem_monitor.peak_stack_usage = stack_usage;
        }
    #endif
}

/**
 * @brief 模拟内存分配 - 用于测试内存监控功能
 * 在真实系统中，应替换为实际的内存分配函数钩子
 */
static void *platform_stm32_malloc_hook(size_t size) {
    // 调用实际的内存分配函数
    void *ptr = malloc(size);
    
    // 更新内存监控信息
    if (ptr != NULL) {
        mem_monitor.used_heap_size += size;
        mem_monitor.heap_alloc_count++;
        
        // 更新最大堆使用量
        if (mem_monitor.used_heap_size > mem_monitor.max_heap_usage) {
            mem_monitor.max_heap_usage = mem_monitor.used_heap_size;
        }
        
        // 简单计算碎片率（实际应用中需要更复杂的算法）
        mem_monitor.heap_fragmentation = (mem_monitor.heap_alloc_count * 2) % 100;
    }
    
    return ptr;
}

/**
 * @brief 模拟内存释放 - 用于测试内存监控功能
 * 在真实系统中，应替换为实际的内存释放函数钩子
 */
static void platform_stm32_free_hook(void *ptr, size_t size) {
    // 更新内存监控信息
    if (ptr != NULL) {
        if (mem_monitor.used_heap_size >= size) {
            mem_monitor.used_heap_size -= size;
        }
        mem_monitor.heap_free_count++;
    }
    
    // 调用实际的内存释放函数
    free(ptr);
}

/**
 * @brief 打印详细内存使用报告
 * 用于调试和监控内存使用情况
 */
static void platform_stm32_print_memory_report(void) {
    // 更新栈使用情况
    platform_stm32_update_stack_usage();
    
    // 计算基本内存信息
    uint32_t total_ram = stm32_platform_info.ram_size_kb * 1024;
    uint32_t used_ram = mem_monitor.used_heap_size;
    uint32_t free_ram = total_ram - used_ram;
    uint8_t ram_usage_percent = (total_ram > 0) ? (100 * used_ram / total_ram) : 0;
    
    printf("========== STM32 内存使用报告 ==========\n");
    printf("总 RAM: %u KB\n", total_ram / 1024);
    printf("可用 RAM: %u KB (%.1f%%)\n", free_ram / 1024, 100.0 - ram_usage_percent);
    printf("已用 RAM: %u KB (%.1f%%)\n", used_ram / 1024, ram_usage_percent);
    printf("\n堆内存信息:\n");
    printf("总堆大小: %u KB\n", mem_monitor.total_heap_size / 1024);
    printf("已用堆: %u KB\n", mem_monitor.used_heap_size / 1024);
    printf("峰值堆使用: %u KB\n", mem_monitor.max_heap_usage / 1024);
    printf("堆碎片率: %u%%\n", mem_monitor.heap_fragmentation);
    printf("\n栈内存信息:\n");
    printf("栈大小: %u KB\n", mem_monitor.max_stack_size / 1024);
    printf("峰值栈使用: %u bytes\n", mem_monitor.peak_stack_usage);
    printf("当前栈指针: 0x%08X\n", mem_monitor.last_stack_ptr);
    printf("\n内存分配统计:\n");
    printf("分配次数: %u\n", mem_monitor.heap_alloc_count);
    printf("释放次数: %u\n", mem_monitor.heap_free_count);
    printf("潜在泄漏: %u\n", mem_monitor.heap_alloc_count - mem_monitor.heap_free_count);
    printf("======================================\n");
}

/**
 * @brief 获取内存信息 - 增强版本
 */
static bool platform_stm32_get_memory_info(uint32_t *free_ram, uint32_t *total_ram) {
    // 参数有效性检查
    if (free_ram == NULL || total_ram == NULL) {
        printf("[STM32] 错误: 内存信息缓冲区为空\n");
        platform_record_error(PLATFORM_ERR_PARAM, __func__);
        return false;
    }
    
    // 验证平台信息中的内存大小是否有效
    if (stm32_platform_info.ram_size_kb == 0) {
        printf("[STM32] 错误: 平台RAM大小未初始化\n");
        platform_record_error(PLATFORM_ERR_INIT, __func__);
        return false;
    }
    
    // 更新栈使用情况
    platform_stm32_update_stack_usage();
    
    // 返回更新后的内存信息
    *total_ram = stm32_platform_info.ram_size_kb * 1024;
    *free_ram = *total_ram - mem_monitor.used_heap_size;  // 更准确的可用内存计算
    
    // 边界检查
    if (*free_ram > *total_ram) {
        printf("[STM32] 警告: 内存计算异常，重置可用内存\n");
        *free_ram = *total_ram - 1024;  // 保留1KB作为安全边界
    }
    
    printf("[STM32] 内存信息: 总RAM=%u KB, 可用RAM=%u KB\n", 
           (unsigned int)(*total_ram / 1024), (unsigned int)(*free_ram / 1024));
    
    // 可选：当内存使用率超过阈值时打印警告
    uint8_t usage_percent = (*total_ram > 0) ? (100 * (*total_ram - *free_ram) / *total_ram) : 0;
    if (usage_percent > 80) {
        printf("[STM32] 警告: 内存使用率过高 (%.1f%%)\n", usage_percent);
    }
    
    return true;
}

// 定义扩展的平台实现结构体类型
// 包含中断相关功能
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
    
    // 中断相关扩展功能 - 根据功能开关条件编译
    #if PLATFORM_ENABLE_IRQ
        bool (*irq_register)(uint32_t irq_num, platform_irq_handler_t handler, void *arg, uint8_t priority);
        void (*irq_set_priority)(uint32_t irq_num, uint8_t priority);
        void (*irq_enable)(uint32_t irq_num);
        void (*irq_disable)(uint32_t irq_num);
        void (*irq_clear_pending)(uint32_t irq_num);
        bool (*irq_is_active)(uint32_t irq_num);
        void (*systick_config)(uint32_t ticks);
    #endif
    
    // DMA功能扩展接口
    #if PLATFORM_ENABLE_DMA
        bool (*dma_init)(uint8_t channel, dma_config_t *config);
        bool (*dma_deinit)(uint8_t channel);
        bool (*dma_start)(uint8_t channel);
        bool (*dma_stop)(uint8_t channel);
        bool (*dma_is_complete)(uint8_t channel);
        bool (*dma_set_callback)(uint8_t channel, dma_callback_t callback, void *user_data);
        bool (*dma_get_status)(uint8_t channel, dma_channel_info_t *info);
        bool (*dma_configure_transfer)(uint8_t channel, uint32_t mem_addr, uint32_t periph_addr, uint32_t size);
    #endif
} platform_impl_t;

// STM32平台实现结构体
static const platform_impl_t platform_stm32_impl = {
    .init = platform_stm32_init,
    .get_time_ms = platform_stm32_get_time_ms,
    .get_time_us = platform_stm32_get_time_us,
    .delay_ms = platform_stm32_delay_ms,
    .delay_us = platform_stm32_delay_us,
    .enable_interrupts = platform_stm32_enable_interrupts,
    .disable_interrupts = platform_stm32_disable_interrupts,
    .set_clock_freq = platform_stm32_set_clock_freq,
    .get_clock_freq = platform_stm32_get_clock_freq,
    .reset = platform_stm32_reset,
    .get_cpu_id = platform_stm32_get_cpu_id,
    .get_memory_info = platform_stm32_get_memory_info,
    
    // 中断相关扩展功能 - 根据功能开关条件编译
    #if PLATFORM_ENABLE_IRQ
        .irq_register = platform_stm32_irq_register,
        .irq_set_priority = platform_stm32_irq_set_priority,
        .irq_enable = platform_stm32_irq_enable,
        .irq_disable = platform_stm32_irq_disable,
        .irq_clear_pending = platform_stm32_irq_clear_pending,
        .irq_is_active = platform_stm32_irq_is_active,
        .systick_config = platform_stm32_systick_config
    #endif
    
    // DMA功能扩展接口
    #if PLATFORM_ENABLE_DMA
        .dma_init = platform_stm32_dma_init,
        .dma_deinit = platform_stm32_dma_deinit,
        .dma_start = platform_stm32_dma_start,
        .dma_stop = platform_stm32_dma_stop,
        .dma_is_complete = platform_stm32_dma_is_complete,
        .dma_set_callback = platform_stm32_dma_set_callback,
        .dma_get_status = platform_stm32_dma_get_status,
        .dma_configure_transfer = platform_stm32_dma_configure_transfer,
    #endif
};

/**
 * @brief 中断处理示例函数 - 根据功能开关条件编译
 * @param custom_handler 用户自定义的SysTick回调函数
 * @param arg 回调函数参数
 * @return 是否配置成功
 */
#if PLATFORM_ENABLE_IRQ
bool platform_stm32_setup_systick_demo(platform_irq_handler_t custom_handler, void *arg) {
    // 配置SysTick，假设系统时钟为72MHz，每1ms中断一次
    uint32_t ticks = stm32_platform_info.clock_freq_hz / 1000;
    
    // 注册自定义回调函数
    if (custom_handler != NULL) {
        if (!platform_stm32_irq_register(SYSTICK_IRQn, custom_handler, arg, NVIC_IRQ_PRIO_1)) {
            printf("[STM32] 错误: 注册SysTick回调函数失败\n");
            return false;
        }
    }
    
    // 配置SysTick定时器
    platform_stm32_systick_config(ticks);
    
    // 使能SysTick中断
    platform_stm32_irq_enable(SYSTICK_IRQn);
    
    printf("[STM32] SysTick中断演示配置完成，时钟频率: %u Hz，中断周期: 1ms\n", 
           stm32_platform_info.clock_freq_hz);
    return true;
}

/**
 * @brief 获取平台错误状态信息
 * @return 错误状态结构体指针
 */
static const platform_impl_t* platform_stm32_get_impl(void) {
    return &platform_stm32_impl;
}

/**
 * @brief 注册STM32平台实现
 * @return 注册是否成功
 */
#ifdef PLATFORM_STM32
bool platform_register_stm32_impl(void) {
    bool success = true;
    
    // 验证平台信息是否有效
    if (stm32_platform_info.name == NULL || 
        stm32_platform_info.mcu_name == NULL ||
        stm32_platform_info.clock_freq_hz == 0) {
        printf("[STM32] 错误: 平台信息不完整或无效\n");
        platform_record_error(PLATFORM_ERR_INIT, __func__);
        success = false;
        return success;
    }
    
    // 更新平台信息为STM32F4系列
    extern platform_info_t platform_info;
    memcpy(&platform_info, &stm32_platform_info, sizeof(platform_info_t));
    
    printf("[STM32] 注册平台实现: %s\n", platform_info.name);
    
    // 调用平台抽象层提供的注册函数
    extern bool platform_register_impl(const platform_impl_t *impl);
    bool ret = platform_register_impl(&platform_stm32_impl);
    
    if (ret) {
        printf("[STM32] 平台实现注册成功!\n");
    } else {
        printf("[STM32] 平台实现注册失败!\n");
        platform_record_error(PLATFORM_ERR_INIT, __func__);
        success = false;
    }
    
    return success;
}