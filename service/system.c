/*
 * 系统服务实现
 */

#include "system.h"
#include "../config/config.h"
#include "../include/types.h"
#include "../rtos/rtos_adapter.h"
#include "../rtos/FreeRTOSConfig.h"
#include "../platform/platform.h"
#include "../hal/hal_uart.h"
#include "stm32f4xx_hal.h"
#include <stdarg.h>
#include <string.h>

// 系统错误码定义
#define SYSTEM_OK 0
#define SYSTEM_ERROR -1

// 最大任务数量
#define MAX_SYSTEM_TASKS     10

// 系统状态
static system_status_t system_status;
static system_task_t tasks[MAX_SYSTEM_TASKS];
static uint32_t tasks_count = 0;
static void* task_handles[MAX_SYSTEM_TASKS] = {NULL};
static bool system_initialized = false;
static uint32_t system_start_time = 0;

// 函数声明
const char *system_get_log_level_string(log_level_t level);

// 任务优先级处理将在调度器中直接实现

/**
 * @brief 初始化串口用于日志输出
 */
static bool system_init_log_uart(void) {
    // 配置UART用于日志输出
    uart_config_t log_uart_config = {
        .baudrate = 115200,
        .data_bits = UART_DATA_BITS_8,
        .stop_bits = UART_STOP_BITS_1,
        .parity = UART_PARITY_NONE,
        .flow_control = UART_FLOW_CONTROL_NONE,
        .dma_enable = false,
        .rx_interrupt_enable = false,
        .tx_interrupt_enable = false
    };
    
    // 初始化UART1作为日志输出
    if (!hal_uart_init(UART_1, &log_uart_config)) {
        // 串口初始化失败，使用STM32调试输出作为后备
        HAL_UART_Transmit(&huart1, (uint8_t*)"串口初始化失败，将使用STM32调试输出作为后备\n", 45, HAL_MAX_DELAY);
        return false;
    }
    
    return true;
}

/**
 * @brief 初始化系统服务
 */
bool system_init(void) {
    // 初始化系统状态
    system_status.state = SYSTEM_STATE_INIT;
    system_status.uptime = 0;
    system_status.cpu_usage = 0;
    system_status.free_heap = 0;
    system_status.temperature = 0;
    system_status.last_error = SYSTEM_ERROR_NONE;
    system_status.error_count = 0;
    
    // 初始化任务列表
    for (uint8_t i = 0; i < MAX_SYSTEM_TASKS; i++) {
        tasks[i].name = NULL;
        tasks[i].function = NULL;
        tasks[i].params = NULL;
        tasks[i].interval = 0;
        tasks[i].priority = TASK_PRIORITY_MEDIUM;
        tasks[i].is_active = false;
        tasks[i].last_run_time = 0;
        tasks[i].run_count = 0;
        // 初始化任务状态
        task_handles[i] = NULL;
    }
    
    // 初始化平台
    platform_init_config_t platform_config = {
        .clock_source = PLATFORM_CLOCK_SOURCE_PLL,
        .target_freq_hz = SYSTEM_CLOCK_FREQ,
        .use_cache = true,
        .use_fpu = true
    };
    
    if (!platform_init(&platform_config)) {
        // 平台初始化失败，使用printf输出错误（因为system_log可能还未就绪）
        printf("平台初始化失败\n");
        return false;
    }
    
    // 初始化日志串口
    system_init_log_uart();
    
    // 初始化内存管理
    system_memory_init();
    
    // 初始化RTOS
    if (!rtos_init()) {
        system_log(LOG_LEVEL_ERROR, "RTOS初始化失败\n");
        return false;
    }
    
    system_log(LOG_LEVEL_INFO, "系统服务初始化成功\n");
    
    system_initialized = true;
    system_start_time = system_get_time_ms();
    return true;
}

/**
 * @brief 系统延迟函数
 */
void system_delay_ms(uint32_t ms) {
    // 使用平台抽象层的延时函数
    platform_delay_ms(ms);
}

/**
 * @brief 系统延迟函数（毫秒）
 * @param ms 延迟时间(毫秒)
 */
void system_delay(uint32_t ms) {
    // 调用system_delay_ms以保持一致性
    system_delay_ms(ms);
}

/**
 * @brief 获取系统时间戳
 */
uint32_t system_get_time_ms(void) {
    if (!system_initialized) {
        return 0;
    }
    
    // 使用平台抽象层的获取时间函数
    return platform_get_time_ms();
}

/**
 * @brief 获取系统状态
 */
bool system_get_status(system_status_t *status) {
    if (!system_initialized || status == NULL) {
        return false;
    }
    
    // 更新运行时间
    system_status.uptime = system_get_time_ms();
    
    // 复制状态
    *status = system_status;
    return true;
}

/**
 * @brief 设置系统状态
 */
bool system_set_state(system_state_t state) {
    if (!system_initialized) {
        return false;
    }
    
    system_status.state = state;
    return true;
}

/**
 * @brief 添加系统任务
 * @param task 任务结构体
 * @return 任务ID或错误码
 */
int32_t system_add_task(system_task_t *task) {
    // 检查系统是否已初始化
    if (!system_initialized) {
        system_log(LOG_LEVEL_ERROR, "系统未初始化\n");
        return SYSTEM_ERROR;
    }

    // 检查参数是否有效
    if (task == NULL || task->function == NULL) {
        system_log(LOG_LEVEL_ERROR, "无效的任务参数\n");
        return SYSTEM_ERROR;
    }

    // 检查任务数量是否超过上限
    if (tasks_count >= MAX_SYSTEM_TASKS) {
        system_log(LOG_LEVEL_ERROR, "任务数量已达上限\n");
        return SYSTEM_ERROR;
    }

    // 复制任务信息
    uint32_t task_id = tasks_count;
    tasks[task_id].name = task->name;
    tasks[task_id].function = task->function;
    tasks[task_id].params = task->params;
    tasks[task_id].interval = task->interval;
    tasks[task_id].priority = task->priority;
    tasks[task_id].is_active = true; // 初始为活动状态
    tasks[task_id].last_run_time = system_get_time_ms();
    tasks[task_id].run_count = 0;
    
    // 调用RTOS适配器创建真正的任务
    void *handle = rtos_create_task(task->name, 
                                   (task_func_t)task->function, 
                                   configMINIMAL_STACK_SIZE, 
                                   task->params, 
                                   task->priority, 
                                   NULL);
    
    if (handle == NULL) {
        system_log(LOG_LEVEL_ERROR, "RTOS任务创建失败: %s\n", task->name);
        // 如果RTOS任务创建失败，清除任务信息
        memset(&tasks[task_id], 0, sizeof(system_task_t));
        return SYSTEM_ERROR;
    }
    
    // 保存任务句柄
    task_handles[task_id] = handle;
    tasks_count++;
    
    system_log(LOG_LEVEL_INFO, "任务已添加: %d, 名称: %s, 优先级: %d\n", 
               task_id, task->name, task->priority);
    return (int32_t)task_id; // 返回任务ID
}

/**
 * @brief 移除系统任务
 * @param task_id 任务ID
 * @return 是否移除成功
 */
bool system_remove_task(int32_t task_id) {
    // 检查参数有效性
    if (!system_initialized || task_id < 0 || task_id >= (int32_t)tasks_count) {
        system_log(LOG_LEVEL_ERROR, "无效的任务ID: %d\n", task_id);
        return false;
    }

    // 检查任务是否存在
    if (tasks[task_id].function == NULL) {
        system_log(LOG_LEVEL_WARNING, "任务不存在: %d\n", task_id);
        return false;
    }

    const char *task_name = tasks[task_id].name;
    
    // 调用RTOS适配器删除任务
    if (task_handles[task_id] != NULL) {
        rtos_delete_task(task_handles[task_id]);
        task_handles[task_id] = NULL;
    }
    
    // 清除任务信息
    memset(&tasks[task_id], 0, sizeof(system_task_t));
    
    // 如果是最后一个任务，可以减少tasks_count
    if (task_id == (int32_t)(tasks_count - 1)) {
        tasks_count--;
    }
    
    system_log(LOG_LEVEL_INFO, "任务已移除: %d, 名称: %s\n", task_id, task_name);
    return true;
}

/**
 * @brief 启动任务
 * @param task_id 任务ID
 * @return 是否启动成功
 */
bool system_start_task(int32_t task_id) {
    if (!system_initialized || task_id < 0 || task_id >= (int32_t)tasks_count) {
        return false;
    }
    
    // 使用RTOS恢复任务
    if (tasks[task_id].is_active && task_handles[task_id]) {
        rtos_resume_task(task_handles[task_id]);
    }
    
    tasks[task_id].is_active = true;
    return true;
}

/**
 * @brief 停止任务
 * @param task_id 任务ID
 * @return 是否停止成功
 */
bool system_stop_task(int32_t task_id) {
    if (!system_initialized || task_id < 0 || task_id >= (int32_t)tasks_count) {
        return false;
    }
    
    // 使用RTOS挂起任务
    if (tasks[task_id].is_active && task_handles[task_id]) {
        rtos_suspend_task(task_handles[task_id]);
    }
    
    tasks[task_id].is_active = false;
    return true;
}

/**
 * @brief 系统任务调度器
 * 在RTOS环境中，此函数不执行实际调度，调度由RTOS内核管理
 */
void system_task_scheduler(void) {
    // RTOS模式下此函数为空，调度由RTOS内核自动完成
    // 这里可以保留一些统计或监控功能
    system_update();
}

/**
 * @brief 记录系统错误
 */
void system_error(system_error_t error, const char *file, uint32_t line) {
    system_status.last_error = error;
    system_status.error_count++;
    
    // 输出错误日志
    system_log(LOG_LEVEL_ERROR, "ERROR: %s (%u) at %s:%u\n",
              system_get_error_string(error), 
              error, file, line);
    
    // 严重错误可以考虑重置系统
    if (error == SYSTEM_ERROR_SENSOR_ERROR || error == SYSTEM_ERROR_MEMORY_ALLOCATION) {
        system_log(LOG_LEVEL_FATAL, "Critical error detected, system reset in 1 second...\n");
        system_delay_ms(1000);
        system_reset(0);
    }
}

/**
 * @brief 系统重置
 */
void system_reset(uint32_t delay) {
    system_log(LOG_LEVEL_INFO, "系统将在%u毫秒后重置...\n", delay);
    
    if (delay > 0) {
        platform_delay_ms(delay);
    }
    
    // 使用平台抽象层的重置函数
    platform_reset(false);
}

/**
 * @brief 获取系统错误描述
 */
const char *system_get_error_string(system_error_t error) {
    static const char *error_strings[] = {
        "No error",
        "System uninitialized",
        "Null pointer",
        "Invalid parameter",
        "Memory error",
        "Task limit reached",
        "Task not found",
        "Resource busy",
        "Timeout",
        "Hardware error"
    };
    
    if (error >= sizeof(error_strings) / sizeof(error_strings[0])) {
        return "Unknown error";
    }
    
    return error_strings[error];
}

/**
 * @brief 系统内存管理
 */
void system_memory_init(void) {
    // 在嵌入式系统中，这里可能需要初始化自定义的内存分配器
    // 在PC仿真环境中，使用标准库的malloc/free即可
}

void *system_malloc(uint32_t size) {
    if (!system_initialized || size == 0) {
        return NULL;
    }
    
    // 使用STM32 HAL内存管理
    void *ptr = malloc(size);
    if (ptr == NULL) {
        system_error(SYSTEM_ERROR_MEMORY_ALLOCATION, __FILE__, __LINE__);
    }
    
    return ptr;
}

void system_free(void *ptr) {
    if (!system_initialized || ptr == NULL) {
        return;
    }
    
    // 使用STM32 HAL内存管理
    free(ptr);
}

/**
 * @brief 系统日志函数
 */
void system_log(log_level_t level, const char *format, ...) {
    if (!system_initialized || format == NULL) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    system_log_level(level, format, args);
    va_end(args);
}

void system_log_level(log_level_t level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    // 确保日志级别有效
    if (level < LOG_LEVEL_DEBUG || level > LOG_LEVEL_ERROR) {
        level = LOG_LEVEL_INFO;
    }
    
    // 准备日志缓冲区
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    
    // 获取日志级别字符串
    const char *level_str = system_get_log_level_string(level);
    
    // 构造完整日志消息
    char log_buffer[300];
    snprintf(log_buffer, sizeof(log_buffer), "[%s] %s", level_str, buffer);
    
    // 尝试使用UART输出日志
    bool uart_success = false;
    if (system_initialized) {
        // 使用hal_uart_send_data发送日志数据
        uint16_t length = (uint16_t)strlen(log_buffer);
        if (hal_uart_send_data(UART_1, (uint8_t *)log_buffer, length) > 0) {
            uart_success = true;
        }
    }
    
    // 如果UART输出失败，使用printf作为后备
    if (!uart_success) {
        printf("%s", log_buffer);
    }
    
    va_end(args);
}

/**
 * @brief 更新系统状态
 */
void system_update(void) {
    // 更新系统运行时间
    system_status.uptime = system_get_time_ms() - system_start_time;
    
    // 模拟CPU使用率计算
    system_status.cpu_usage = 50;  // 实际应用中应该基于实际运行时间计算
    
    // 模拟内存使用情况
    system_status.free_heap = 8 * 1024;  // 8KB空闲内存
    system_status.used_heap = 2 * 1024;  // 2KB已用内存
    system_status.total_heap = 10 * 1024;  // 10KB总内存
}

/**
 * @brief 检查系统是否有错误
 */
bool system_has_error(void) {
    // 简单实现：检查是否有错误记录
    return system_status.error_count > 0;
}

/**
 * @brief 获取系统错误信息
 */
bool system_get_error(error_info_t *error) {
    // 如果提供了错误指针，设置错误信息并返回true
    if (error != NULL) {
        error->code = system_status.last_error;
        error->message = system_get_error_string(system_status.last_error);
    }
    return true;
}

/**
 * @brief 获取日志级别字符串
 */
const char *system_get_log_level_string(log_level_t level) {
    // 根据日志级别返回对应的字符串
    switch (level) {
        case LOG_LEVEL_DEBUG:
            return "DEBUG";
        case LOG_LEVEL_INFO:
            return "INFO";
        case LOG_LEVEL_WARNING:
            return "WARNING";
        case LOG_LEVEL_ERROR:
            return "ERROR";
        case LOG_LEVEL_FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
    }
}

// ========================== 电源管理模块 ==========================

/*
 * 电源管理模块实现
 */

#include "system.h"
#include "../communication/communication.h"
#include "../hal/hal_gpio.h"

// 电源管理状态变量
static power_status_t g_power_status = {0};
static power_config_t g_power_config = {0};
static battery_management_t g_battery_mgmt = {0};
static power_protection_t g_power_protection = {0};
static bool g_power_initialized = false;

/**
 * @brief 初始化电源管理模块
 */
bool power_management_init(void) {
    // 初始化电源配置
    g_power_config.mode = POWER_MODE_NORMAL;
    g_power_config.voltage_threshold = 10.5f;
    g_power_config.current_limit = 2.0f;
    g_power_config.temperature_limit = 60.0f;
    g_power_config.sleep_timeout = 300000; // 5分钟
    g_power_config.auto_power_off = true;
    g_power_config.low_power_alert = true;
    g_power_config.low_power_threshold = 20.0f;
    
    // 初始化电源保护
    g_power_protection.over_voltage_protection = true;
    g_power_protection.under_voltage_protection = true;
    g_power_protection.over_current_protection = true;
    g_power_protection.over_temperature_protection = true;
    g_power_protection.short_circuit_protection = true;
    g_power_protection.reverse_polarity_protection = true;
    
    // 初始化电池管理
    g_battery_mgmt.main_battery.voltage = 12.0f;
    g_battery_mgmt.main_battery.current = 0.0f;
    g_battery_mgmt.main_battery.temperature = 25.0f;
    g_battery_mgmt.main_battery.remaining = 100.0f;
    g_battery_mgmt.main_battery.capacity = 3000;
    g_battery_mgmt.main_battery.low_power = false;
    
    // 初始化电源状态
    g_power_status.state = POWER_STATE_ON;
    g_power_status.input_voltage = 12.0f;
    g_power_status.output_voltage = 5.0f;
    g_power_status.input_current = 1.0f;
    g_power_status.output_current = 0.8f;
    g_power_status.power_consumption = 4.0f;
    g_power_status.efficiency = 80.0f;
    g_power_status.temperature = 25.0f;
    g_power_status.charging = false;
    g_power_status.low_power_warning = false;
    
    g_power_initialized = true;
    
    system_log(LOG_LEVEL_INFO, "电源管理模块初始化成功");
    return true;
}

/**
 * @brief 设置电源模式
 */
bool power_set_mode(power_mode_t mode) {
    if (!g_power_initialized) {
        return false;
    }
    
    power_mode_t old_mode = g_power_config.mode;
    g_power_config.mode = mode;
    
    switch (mode) {
        case POWER_MODE_NORMAL:
            // 正常模式：全性能运行
            system_log(LOG_LEVEL_INFO, "设置电源模式: 正常模式");
            break;
            
        case POWER_MODE_ECO:
            // 经济模式：降低性能，节省功耗
            system_log(LOG_LEVEL_INFO, "设置电源模式: 经济模式");
            break;
            
        case POWER_MODE_LOW:
            // 低功耗模式：最低性能，最大节能
            system_log(LOG_LEVEL_INFO, "设置电源模式: 低功耗模式");
            break;
            
        case POWER_MODE_SLEEP:
            // 睡眠模式：大部分功能停止
            system_log(LOG_LEVEL_INFO, "设置电源模式: 睡眠模式");
            break;
            
        case POWER_MODE_BOOST:
            // 增强模式：最高性能
            system_log(LOG_LEVEL_INFO, "设置电源模式: 增强模式");
            break;
    }
    
    system_log(LOG_LEVEL_INFO, "电源模式切换: %d -> %d", old_mode, mode);
    return true;
}

/**
 * @brief 获取电源状态
 */
bool power_get_status(power_status_t *status) {
    if (!status || !g_power_initialized) {
        return false;
    }
    
    memcpy(status, &g_power_status, sizeof(power_status_t));
    return true;
}

/**
 * @brief 获取电池信息
 */
bool power_get_battery_info(battery_management_t *battery_info) {
    if (!battery_info || !g_power_initialized) {
        return false;
    }
    
    memcpy(battery_info, &g_battery_mgmt, sizeof(battery_management_t));
    return true;
}

/**
 * @brief 强制关机
 */
void power_shutdown(void) {
    system_log(LOG_LEVEL_WARNING, "执行强制关机");
    
    // 保存重要数据
    system_log(LOG_LEVEL_INFO, "保存关键数据");
    
    // 关闭所有外设
    system_log(LOG_LEVEL_INFO, "禁用所有外设");
    
    // 设置电源状态
    g_power_status.state = POWER_STATE_OFF;
    
    // 进入关机模式
    system_log(LOG_LEVEL_INFO, "进入关机模式");
}

/**
 * @brief 重启系统
 */
void power_reboot(void) {
    system_log(LOG_LEVEL_WARNING, "执行系统重启");
    
    // 保存状态数据
    system_log(LOG_LEVEL_INFO, "保存关键数据");
    
    // 重启延迟
    system_delay(1000);
    
    // 触发软件复位
    system_reset(0);
}

// 电源管理内部函数
static void power_update_status(void) {
    // 模拟电源状态更新 (实际项目中需要读取硬件寄存器)
    g_power_status.uptime += 100;
    
    // 更新温度
    g_power_status.temperature += (float)(rand() % 100 - 50) / 1000.0f;
    if (g_power_status.temperature > 80.0f) {
        system_log(LOG_LEVEL_WARNING, "电源温度过高: %.1f°C", g_power_status.temperature);
    }
    
    // 更新功耗
    g_power_status.power_consumption = g_power_status.output_voltage * g_power_status.output_current;
    
    // 计算效率
    if (g_power_status.input_current > 0.1f) {
        g_power_status.efficiency = (g_power_status.output_voltage * g_power_status.output_current) / 
                                   (g_power_status.input_voltage * g_power_status.input_current) * 100.0f;
    }
}

static void power_check_battery_status(void) {
    // 检查主电池状态
    if (g_battery_mgmt.main_battery.voltage < g_power_config.voltage_threshold) {
        g_battery_mgmt.main_battery.low_power = true;
        g_power_status.low_power_warning = true;
        system_log(LOG_LEVEL_WARNING, "电池电压低: %.2fV", g_battery_mgmt.main_battery.voltage);
        
        // 自动切换到低功耗模式
        if (g_power_config.mode != POWER_MODE_LOW && g_power_config.mode != POWER_MODE_SLEEP) {
            power_set_mode(POWER_MODE_LOW);
        }
    }
}

// 公开API函数实现
bool power_init(void) {
    return power_management_init();
}

bool power_set_power_mode(power_mode_t mode) {
    return power_set_mode(mode);
}

bool power_get_power_status(power_status_t *status) {
    return power_get_status(status);
}

bool power_get_battery_management(battery_management_t *battery) {
    return power_get_battery_info(battery);
}

void power_emergency_shutdown(void) {
    power_shutdown();
}

void power_system_reboot(void) {
    power_reboot();
}

// ========================== 完整系统服务模块实现 ==========================

/*
 * 系统服务模块完整实现
 */

#include "system.h"
#include "../communication/communication.h"
#include "../algorithm/attitude.h"
#include "../algorithm/pid.h"

// ========================== 系统状态管理 ==========================

// 系统状态变量
static system_state_t g_system_state = SYSTEM_STATE_INIT;
static system_status_t g_system_status = {0};
static error_info_t g_system_errors[10] = {0};
static uint32_t g_error_count = 0;
static uint32_t g_system_start_time = 0;

// 系统任务管理
static system_task_t g_system_tasks[20] = {0};
static int32_t g_task_count = 0;

// ========================== 系统初始化函数 ==========================

/**
 * @brief 初始化系统服务
 */
bool system_init(void) {
    system_log_level(LOG_LEVEL_INFO, "开始初始化系统服务...");
    
    // 1. 初始化硬件抽象层
    if (!hal_init()) {
        system_log_level(LOG_LEVEL_ERROR, "HAL初始化失败");
        return false;
    }
    
    // 2. 初始化系统定时器
    if (!timer_init_all()) {
        system_log_level(LOG_LEVEL_ERROR, "定时器初始化失败");
        return false;
    }
    
    // 3. 初始化通信模块
    if (!communication_init()) {
        system_log_level(LOG_LEVEL_ERROR, "通信模块初始化失败");
        return false;
    }
    
    // 4. 初始化算法模块
    if (!algorithm_module_init()) {
        system_log_level(LOG_LEVEL_ERROR, "算法模块初始化失败");
        return false;
    }
    
    // 5. 初始化电源管理
    if (!power_management_init()) {
        system_log_level(LOG_LEVEL_ERROR, "电源管理初始化失败");
        return false;
    }
    
    // 6. 初始化系统日志
    system_memory_init();
    g_system_start_time = system_get_time_ms();
    
    // 7. 设置系统状态
    g_system_state = SYSTEM_STATE_READY;
    g_system_status.version = "v1.0.0";
    g_system_status.build_date = __DATE__;
    g_system_status.build_time = __TIME__;
    g_system_status.uptime = 0;
    
    // 8. 注册系统任务
    system_register_task("System Monitor", system_monitoring_task, NULL, 100); // 100ms
    system_register_task("Power Management", power_management_task, NULL, 100); // 100ms
    system_register_task("Algorithm Processing", algorithm_processing_task, NULL, 50); // 50ms
    system_register_task("Performance Monitor", performance_monitoring_task, NULL, 500); // 500ms
    system_register_task("Fault Detection", fault_detection_task, NULL, 200); // 200ms
    
    system_log_level(LOG_LEVEL_INFO, "系统服务初始化完成！");
    return true;
}

/**
 * @brief 初始化电源管理模块
 */
bool power_management_init(void) {
    // 初始化电源配置
    g_power_config.mode = POWER_MODE_NORMAL;
    g_power_config.voltage_threshold = 10.5f;
    g_power_config.current_limit = 2.0f;
    g_power_config.temperature_limit = 60.0f;
    g_power_config.sleep_timeout = 300000; // 5分钟
    g_power_config.auto_power_off = true;
    g_power_config.low_power_alert = true;
    g_power_config.low_power_threshold = 20.0f;
    
    // 初始化电源保护
    g_power_protection.over_voltage_protection = true;
    g_power_protection.under_voltage_protection = true;
    g_power_protection.over_current_protection = true;
    g_power_protection.over_temperature_protection = true;
    g_power_protection.short_circuit_protection = true;
    g_power_protection.reverse_polarity_protection = true;
    
    // 初始化电池管理
    g_battery_mgmt.main_battery.voltage = 12.0f;
    g_battery_mgmt.main_battery.current = 0.0f;
    g_battery_mgmt.main_battery.temperature = 25.0f;
    g_battery_mgmt.main_battery.remaining = 100.0f;
    g_battery_mgmt.main_battery.capacity = 3000;
    g_battery_mgmt.main_battery.low_power = false;
    
    // 初始化电源状态
    g_power_status.state = POWER_STATE_ON;
    g_power_status.input_voltage = 12.0f;
    g_power_status.output_voltage = 5.0f;
    g_power_status.input_current = 1.0f;
    g_power_status.output_current = 0.8f;
    g_power_status.power_consumption = 4.0f;
    g_power_status.efficiency = 80.0f;
    g_power_status.temperature = 25.0f;
    g_power_status.charging = false;
    g_power_status.low_power_warning = false;
    
    g_power_initialized = true;
    
    system_log(LOG_LEVEL_INFO, "电源管理模块初始化成功");
    return true;
}

/**
 * @brief 算法模块初始化
 */
bool algorithm_module_init(void) {
    // 初始化姿态算法
    attitude_algorithm_init();
    
    // 初始化PID控制器
    pid_controller_init(&g_pid_controllers[0], 1.0f, 0.1f, 0.05f, -100.0f, 100.0f);
    g_pid_count = 1;
    
    // 初始化故障检测
    fault_detection_init();
    
    // 初始化性能监控
    performance_monitor_init();
    
    return true;
}

/**
 * @brief 互补滤波器姿态解算
 */
void attitude_complementary_filter_update(float *gyro, float *accel, float *mag) {
    // 将陀螺仪数据转换为弧度/秒
    float gx = gyro[0] * PI / 180.0f;
    float gy = gyro[1] * PI / 180.0f;
    float gz = gyro[2] * PI / 180.0f;
    
    // 加速度计数据归一化
    float accel_norm = sqrtf(accel[0]*accel[0] + accel[1]*accel[1] + accel[2]*accel[2]);
    if (accel_norm > 0) {
        float ax = accel[0] / accel_norm;
        float ay = accel[1] / accel_norm;
        float az = accel[2] / accel_norm;
        
        // 计算参考俯仰角和横滚角
        float pitch_ref = atan2f(-ax, sqrtf(ay*ay + az*az));
        float roll_ref = atan2f(ay, az);
        
        // 互补滤波：融合陀螺仪和加速度计数据
        float dt = g_attitude_data.complementary_filter.dt;
        float alpha = g_attitude_data.complementary_filter.kp;
        
        g_attitude_data.pitch = g_attitude_data.pitch + gx * dt + alpha * (pitch_ref - g_attitude_data.pitch);
        g_attitude_data.roll = g_attitude_data.roll + gy * dt + alpha * (roll_ref - g_attitude_data.roll);
        
        // 简单的偏航角更新 (磁力计补偿)
        if (mag != NULL) {
            float mag_norm = sqrtf(mag[0]*mag[0] + mag[1]*mag[1] + mag[2]*mag[2]);
            if (mag_norm > 0) {
                float mx = mag[0] / mag_norm;
                float my = mag[1] / mag_norm;
                
                // 计算参考偏航角
                float yaw_ref = atan2f(my * cosf(g_attitude_data.roll) - mx * sinf(g_attitude_data.roll),
                                      mx * cosf(g_attitude_data.pitch) + my * sinf(g_attitude_data.roll) * sinf(g_attitude_data.pitch));
                
                // 更新偏航角
                g_attitude_data.yaw = g_attitude_data.yaw + gz * dt + alpha * (yaw_ref - g_attitude_data.yaw);
            }
        }
    }
}

/**
 * @brief Madgwick算法姿态解算
 */
void attitude_madgwick_update(float *gyro, float *accel, float *mag) {
    float q0 = g_attitude_data.madgwick_filter.q0;
    float q1 = g_attitude_data.madgwick_filter.q1;
    float q2 = g_attitude_data.madgwick_filter.q2;
    float q3 = g_attitude_data.madgwick_filter.q3;
    float beta = g_attitude_data.madgwick_filter.beta;
    float dt = 0.01f; // 100Hz
    
    // 陀螺仪数据转换
    float gx = gyro[0] * PI / 180.0f;
    float gy = gyro[1] * PI / 180.0f;
    float gz = gyro[2] * PI / 180.0f;
    
    // 加速度计数据归一化
    float accel_norm = sqrtf(accel[0]*accel[0] + accel[1]*accel[1] + accel[2]*accel[2]);
    float ax = accel[0] / accel_norm;
    float ay = accel[1] / accel_norm;
    float az = accel[2] / accel_norm;
    
    // 磁力计数据归一化 (如果有)
    float mx = 0, my = 0, mz = 0;
    bool has_magnetometer = false;
    if (mag != NULL) {
        float mag_norm = sqrtf(mag[0]*mag[0] + mag[1]*mag[1] + mag[2]*mag[2]);
        if (mag_norm > 0) {
            mx = mag[0] / mag_norm;
            my = mag[1] / mag_norm;
            mz = mag[2] / mag_norm;
            has_magnetometer = true;
        }
    }
    
    // 目标函数梯度
    float s0, s1, s2, s3;
    
    // 姿态评估函数对重力向量的梯度
    float f0 = 2*(q1*q3 - q0*q2) - ax;
    float f1 = 2*(q0*q1 + q2*q3) - ay;
    float f2 = 2*(0.5f - q1*q1 - q2*q2) - az;
    
    // 目标函数的雅可比矩阵 (重力向量部分)
    s0 = -2*q2*f0 + 2*q1*f1 - 2*az*q2;
    s1 = 2*q3*f0 + 2*q0*f1 - 2*ay*q3;
    s2 = -2*q0*f0 + 2*q3*f1 - 2*az*q0;
    s3 = 2*q1*f0 + 2*q2*f1;
    
    // 磁力计校正 (如果有)
    if (has_magnetometer) {
        float bz = mx*(-2*(q1*q3 - q0*q2)) + my*(2*q0*q1 + 2*q2*q3) + mz*(2*0.5f - q1*q1 - q2*q2);
        float f3 = 2*mx*(0.5f - q2*q2 - q3*q3) + 2*my*(q0*q3 - q1*q2) + 2*mz*(q0*q2 + q1*q3) - bz;
        
        // 磁力计雅可比矩阵
        s0 += 2*mx*(0.5f - q2*q2 - q3*q3) + 2*my*(q0*q3 - q1*q2) + 2*mz*(q0*q2 + q1*q3);
        s1 += 2*mx*(q0*q2 + q1*q3) + 2*my*(0.5f - q1*q1 - q3*q3) + 2*mz*(q0*q1 - q2*q3);
        s2 += -2*mx*(q0*q3 - q1*q2) + 2*my*(q0*q2 + q1*q3) + 2*mz*(0.5f - q1*q1 - q2*q2);
        s3 += -2*mx*(q0*q2 + q1*q3) + 2*my*(q0*q1 - q2*q3) + 2*mz*(0.5f - q1*q1 - q2*q2);
    }
    
    // 归一化梯度
    float norm = sqrtf(s0*s0 + s1*s1 + s2*s2 + s3*s3);
    if (norm > 0) {
        s0 /= norm;
        s1 /= norm;
        s2 /= norm;
        s3 /= norm;
    }
    
    // 四元数导数
    float qdot0 = 0.5f*(-q1*gx - q2*gy - q3*gz) - beta * s0;
    float qdot1 = 0.5f*(q0*gx + q2*gz - q3*gy) - beta * s1;
    float qdot2 = 0.5f*(q0*gy - q1*gz + q3*gx) - beta * s2;
    float qdot3 = 0.5f*(q0*gz + q1*gy - q2*gx) - beta * s3;
    
    // 积分更新四元数
    q0 += qdot0 * dt;
    q1 += qdot1 * dt;
    q2 += qdot2 * dt;
    q3 += qdot3 * dt;
    
    // 四元数归一化
    norm = sqrtf(q0*q0 + q1*q1 + q2*q2 + q3*q3);
    q0 /= norm;
    q1 /= norm;
    q2 /= norm;
    q3 /= norm;
    
    // 保存更新后的四元数
    g_attitude_data.madgwick_filter.q0 = q0;
    g_attitude_data.madgwick_filter.q1 = q1;
    g_attitude_data.madgwick_filter.q2 = q2;
    g_attitude_data.madgwick_filter.q3 = q3;
    
    // 转换为欧拉角
    g_attitude_data.pitch = asin(2*(q0*q2 - q1*q3)) * 180.0f / PI;
    g_attitude_data.roll = atan2(2*(q0*q1 + q2*q3), 1 - 2*(q1*q1 + q2*q2)) * 180.0f / PI;
    g_attitude_data.yaw = atan2(2*(q0*q3 + q1*q2), 1 - 2*(q2*q2 + q3*q3)) * 180.0f / PI;
}

// 获取当前姿态数据
bool attitude_get_data(attitude_data_t *data) {
    if (!data) return false;
    memcpy(data, &g_attitude_data, sizeof(attitude_data_t));
    return true;
}

// ========================== PID控制算法模块 ==========================

// PID控制器状态
static pid_controller_t g_pid_controllers[10] = {0};
static uint32_t g_pid_count = 0;

/**
 * @brief 初始化PID控制器
 */
bool pid_controller_init(pid_controller_t *pid, float kp, float ki, float kd, float min_output, float max_output) {
    if (!pid) return false;
    
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->min_output = min_output;
    pid->max_output = max_output;
    pid->integral = 0.0f;
    pid->previous_error = 0.0f;
    pid->last_update_time = system_get_time_ms();
    pid->is_enabled = true;
    
    return true;
}

/**
 * @brief 位置式PID控制计算
 */
float pid_calculate(pid_controller_t *pid, float setpoint, float measurement) {
    if (!pid || !pid->is_enabled) return 0.0f;
    
    // 计算当前时间
    uint32_t current_time = system_get_time_ms();
    float dt = (current_time - pid->last_update_time) / 1000.0f; // 转换为秒
    pid->last_update_time = current_time;
    
    // 计算误差
    float error = setpoint - measurement;
    
    // 比例项
    float p_term = pid->kp * error;
    
    // 积分项
    pid->integral += error * dt;
    // 积分限幅
    if (pid->integral > pid->max_output / pid->ki) {
        pid->integral = pid->max_output / pid->ki;
    } else if (pid->integral < pid->min_output / pid->ki) {
        pid->integral = pid->min_output / pid->ki;
    }
    float i_term = pid->ki * pid->integral;
    
    // 微分项
    float d_term = 0.0f;
    if (dt > 0) {
        float derivative = (error - pid->previous_error) / dt;
        d_term = pid->kd * derivative;
    }
    pid->previous_error = error;
    
    // 总输出
    float output = p_term + i_term + d_term;
    
    // 输出限幅
    if (output > pid->max_output) {
        output = pid->max_output;
        // 反向积分 (防止积分饱和)
        pid->integral -= error * dt;
    } else if (output < pid->min_output) {
        output = pid->min_output;
        // 反向积分
        pid->integral -= error * dt;
    }
    
    pid->last_output = output;
    pid->last_error = error;
    
    return output;
}

/**
 * @brief 重置PID控制器
 */
void pid_reset(pid_controller_t *pid) {
    if (!pid) return;
    
    pid->integral = 0.0f;
    pid->previous_error = 0.0f;
    pid->last_output = 0.0f;
    pid->last_error = 0.0f;
    pid->last_update_time = system_get_time_ms();
}

// ========================== 滤波算法模块 ==========================

/**
 * @brief 指数移动平均滤波器
 */
float filter_ema_update(ema_filter_t *filter, float input) {
    if (!filter) return input;
    
    float alpha = filter->alpha;
    float output = alpha * input + (1.0f - alpha) * filter->previous_output;
    
    filter->previous_output = output;
    filter->sample_count++;
    
    return output;
}

/**
 * @brief 初始化指数移动平均滤波器
 */
void filter_ema_init(ema_filter_t *filter, float alpha, float initial_value) {
    if (!filter) return;
    
    filter->alpha = alpha;
    filter->previous_output = initial_value;
    filter->sample_count = 0;
}

// ========================== 故障检测算法模块 ==========================

// 故障检测状态
static fault_detection_t g_fault_detection = {0};

/**
 * @brief 初始化故障检测模块
 */
bool fault_detection_init(void) {
    memset(&g_fault_detection, 0, sizeof(fault_detection_t));
    
    // 初始化各个滤波器和阈值
    filter_ema_init(&g_fault_detection.sensor_health_filter, 0.1f, 1.0f);
    filter_ema_init(&g_fault_detection.communication_quality_filter, 0.05f, 1.0f);
    
    // 设置故障阈值
    g_fault_detection.thresholds.voltage_low = 10.5f;
    g_fault_detection.thresholds.voltage_high = 15.0f;
    g_fault_detection.thresholds.current_high = 3.0f;
    g_fault_detection.thresholds.temperature_high = 70.0f;
    g_fault_detection.thresholds.imu_failure_threshold = 0.3f;
    g_fault_detection.thresholds.gps_loss_threshold = 10.0f;
    
    system_log(LOG_LEVEL_INFO, "故障检测模块初始化成功");
    return true;
}

// ========================== 性能监控模块 ==========================

// 性能监控状态
static performance_monitor_t g_performance_monitor = {0};

/**
 * @brief 初始化性能监控模块
 */
bool performance_monitor_init(void) {
    memset(&g_performance_monitor, 0, sizeof(performance_monitor_t));
    
    system_log(LOG_LEVEL_INFO, "性能监控模块初始化成功");
    return true;
}