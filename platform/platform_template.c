/*
 * 平台抽象层移植示例
 * 此文件作为移植到特定MCU的模板
 */

#include "platform.h"
#include <stdint.h>
#include <string.h>

// 添加特定MCU的头文件
// 例如：
// #include "stm32f4xx.h"
// 或者 #include "esp32/rom/ets_sys.h"

// 平台特定的实现
static bool platform_impl_init(const platform_init_config_t *config) {
    // TODO: 实现特定MCU的初始化代码
    
    // 1. 初始化时钟系统
    // 2. 配置缓存（如果有）
    // 3. 配置FPU（如果有）
    // 4. 初始化系统定时器
    
    return true;
}

static uint32_t platform_impl_get_time_ms(void) {
    // TODO: 实现获取系统时间（毫秒）
    // 例如：
    // return HAL_GetTick();
    // 或者 return xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    return 0;
}

static uint32_t platform_impl_get_time_us(void) {
    // TODO: 实现获取系统时间（微秒）
    // 例如：
    // return DWT_GetCycleCount() / (SystemCoreClock / 1000000);
    
    return 0;
}

static void platform_impl_delay_ms(uint32_t ms) {
    // TODO: 实现毫秒级延时
    // 例如：
    // HAL_Delay(ms);
    // 或者 vTaskDelay(ms / portTICK_PERIOD_MS);
    
    // 简化的软件延时实现（不推荐用于实际应用）
    uint32_t i;
    for (i = 0; i < ms * 1000; i++) {
        // 空操作
    }
}

static void platform_impl_delay_us(uint32_t us) {
    // TODO: 实现微秒级延时
    // 例如：
    // uint32_t start = DWT_GetCycleCount();
    // uint32_t cycles = us * (SystemCoreClock / 1000000);
    // while ((DWT_GetCycleCount() - start) < cycles);
    
    // 简化的软件延时实现（不推荐用于实际应用）
    uint32_t i;
    for (i = 0; i < us; i++) {
        // 空操作
    }
}

static void platform_impl_enable_interrupts(void) {
    // TODO: 实现使能全局中断
    // 例如：
    // __enable_irq();
    // 或者 portENABLE_INTERRUPTS();
}

static void platform_impl_disable_interrupts(void) {
    // TODO: 实现禁用全局中断
    // 例如：
    // __disable_irq();
    // 或者 portDISABLE_INTERRUPTS();
}

static uint32_t platform_impl_set_clock_freq(platform_clock_source_t source, uint32_t freq_hz) {
    // TODO: 实现设置系统时钟频率
    // 例如：
    // if (source == PLATFORM_CLOCK_SOURCE_PLL) {
    //     // 配置PLL达到目标频率
    //     return configured_freq;
    // }
    
    // 默认返回当前频率
    return PLATFORM_CONFIG_CLOCK_FREQ_HZ;
}

static uint32_t platform_impl_get_clock_freq(void) {
    // TODO: 实现获取系统时钟频率
    // 例如：
    // return SystemCoreClock;
    
    return PLATFORM_CONFIG_CLOCK_FREQ_HZ;
}

static void platform_impl_reset(bool to_bootloader) {
    // TODO: 实现系统重置
    // 例如：
    // NVIC_SystemReset();
    // 或者 esp_restart();
}

static bool platform_impl_get_cpu_id(uint32_t id[3]) {
    if (id == NULL) {
        return false;
    }
    
    // TODO: 实现获取CPU ID
    // 例如：
    // id[0] = *(__IO uint32_t*)(0xE0042000);
    // id[1] = *(__IO uint32_t*)(0xE0042004);
    // id[2] = *(__IO uint32_t*)(0xE0042008);
    
    // 默认为零
    memset(id, 0, 3 * sizeof(uint32_t));
    return true;
}

static bool platform_impl_get_memory_info(uint32_t *free_ram, uint32_t *total_ram) {
    if (free_ram == NULL || total_ram == NULL) {
        return false;
    }
    
    // TODO: 实现获取内存信息
    // 例如：
    // *total_ram = (uint32_t)configTOTAL_HEAP_SIZE;
    // *free_ram = xPortGetFreeHeapSize();
    
    // 默认为零
    *total_ram = 0;
    *free_ram = 0;
    return true;
}

// 平台实现结构体
static const platform_impl_t platform_impl = {
    .init = platform_impl_init,
    .get_time_ms = platform_impl_get_time_ms,
    .get_time_us = platform_impl_get_time_us,
    .delay_ms = platform_impl_delay_ms,
    .delay_us = platform_impl_delay_us,
    .enable_interrupts = platform_impl_enable_interrupts,
    .disable_interrupts = platform_impl_disable_interrupts,
    .set_clock_freq = platform_impl_set_clock_freq,
    .get_clock_freq = platform_impl_get_clock_freq,
    .reset = platform_impl_reset,
    .get_cpu_id = platform_impl_get_cpu_id,
    .get_memory_info = platform_impl_get_memory_info
};

/**
 * @brief 注册此平台的实现
 * 在main函数中调用此函数注册平台实现
 */
bool platform_register_this_impl(void) {
    // 调用平台抽象层提供的注册函数
    extern bool platform_register_impl(const platform_impl_t *impl);
    return platform_register_impl(&platform_impl);
}