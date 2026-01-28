#include "system.h"
#include "system_priv.h"
#include "logger.h"
#include "event.h"
#include "pal.h"  // 平台抽象层
#include "peripheral.h"  // 外设管理
#include "common_def.h"  // 通用定义
#include "audio_core.h"  // 音频核心
#ifdef _WIN32
// Windows平台不包含蓝牙头文件
#else
#include "bluetooth.h"
#endif  // 蓝牙
#include "wifi_media.h"  // WiFi媒体
#include "error_handling.h"  // 统一错误处理
#include <time.h>  // 时间函数

// 事件定义
#define EVENT_SYSTEM_RECOVERY_COMPLETED 1001

// 错误日志相关定义
#define MAX_ERROR_LOGS 10
static int g_error_log_count = 0;
static int g_error_log_index = 0;
static char *g_error_logs[MAX_ERROR_LOGS] = {NULL};

// LED索引定义
#define LED_SYSTEM            0     // 系统指示灯

static SystemCfg_t g_sys_cfg = {0};

// 系统资源使用阈值
#define CPU_USAGE_THRESHOLD_HIGH    80  // CPU使用率高阈值
#define CPU_USAGE_THRESHOLD_CRITICAL 90  // CPU使用率临界阈值
#define MEM_USAGE_THRESHOLD_HIGH    80  // 内存使用率高阈值
#define MEM_USAGE_THRESHOLD_CRITICAL 90  // 内存使用率临界阈值

// 系统状态监控结构
typedef struct {
    int cpu_usage;           // CPU使用率
    int mem_usage;           // 内存使用率
    int temp;                // 温度
    int voltage;             // 电压
    int uptime;              // 运行时间（秒）
    int error_count;         // 错误计数
    int recovery_count;      // 恢复计数
    SysState_e last_state;   // 上一次系统状态
    int state_change_count;  // 状态变化计数
    bool power_off_flag;     // 关机标志
} SystemMonitor_t;

// 系统监控数据
static SystemMonitor_t g_sys_monitor = {0};

// 看门狗实例
static Watchdog_t g_watchdog = {0};

// 系统状态备份
static SystemStateBackup_t g_system_state_backup = {0};

// 系统状态备份文件路径
#define SYSTEM_STATE_BACKUP_FILE_PATH "./config/system_state_backup.json"

// 系统状态持久化存储路径
#define SYSTEM_STATE_FILE_PATH "./config/system_state.json"

// 看门狗配置
#define WATCHDOG_TIMEOUT_SECONDS 30      // 看门狗超时时间（秒）
#define WATCHDOG_CHECK_INTERVAL_SECONDS 5  // 看门狗检查间隔（秒）
#define WATCHDOG_MAX_MISSED_FEEDS 3       // 最大未喂狗次数

// 看门狗状态结构体
typedef struct {
    int timeout_seconds;        // 超时时间（秒）
    int check_interval;         // 检查间隔（秒）
    int missed_feeds;           // 未喂狗次数
    int max_missed_feeds;       // 最大未喂狗次数
    time_t last_feed_time;      // 上次喂狗时间
    time_t last_check_time;     // 上次检查时间
    bool enabled;               // 是否启用
    bool running;               // 是否运行中
    pthread_t thread_id;        // 看门狗线程ID
    pthread_mutex_t mutex;       // 互斥锁
} Watchdog_t;

// 系统状态恢复结构体
typedef struct {
    SysState_e system_state;     // 系统状态
    int volume;                  // 音量
    int mute;                    // 静音状态
    int source;                  // 音频源
    int play_state;              // 播放状态
    char bluetooth_device[64];   // 蓝牙设备
    char wifi_ssid[64];          // WiFi SSID
    time_t save_time;            // 保存时间
} SystemStateBackup_t;

/**
 * @brief 初始化看门狗
 * @param timeout_seconds 超时时间（秒）
 * @param check_interval 检查间隔（秒）
 * @param max_missed_feeds 最大未喂狗次数
 * @return 初始化结果：0表示成功，非0表示失败
 */
static int watchdog_init(int timeout_seconds, int check_interval, int max_missed_feeds) {
    pthread_mutex_init(&g_watchdog.mutex, NULL);
    
    pthread_mutex_lock(&g_watchdog.mutex);
    
    g_watchdog.timeout_seconds = timeout_seconds;
    g_watchdog.check_interval = check_interval;
    g_watchdog.max_missed_feeds = max_missed_feeds;
    g_watchdog.missed_feeds = 0;
    g_watchdog.last_feed_time = time(NULL);
    g_watchdog.last_check_time = time(NULL);
    g_watchdog.enabled = true;
    g_watchdog.running = true;
    
    pthread_mutex_unlock(&g_watchdog.mutex);
    
    LOG_INFO("Watchdog initialized: timeout=%d, check_interval=%d, max_missed_feeds=%d",
             timeout_seconds, check_interval, max_missed_feeds);
    
    return SUCCESS;
}

/**
 * @brief 反初始化看门狗
 * @return 反初始化结果：0表示成功，非0表示失败
 */
static int watchdog_deinit(void) {
    pthread_mutex_lock(&g_watchdog.mutex);
    g_watchdog.running = false;
    g_watchdog.enabled = false;
    pthread_mutex_unlock(&g_watchdog.mutex);
    
    // 等待看门狗线程退出
    if (g_watchdog.thread_id) {
        pthread_join(g_watchdog.thread_id, NULL);
    }
    
    pthread_mutex_destroy(&g_watchdog.mutex);
    
    LOG_INFO("Watchdog deinitialized");
    return SUCCESS;
}

/**
 * @brief 喂看门狗
 * @return 喂狗结果：0表示成功，非0表示失败
 */
static int watchdog_feed(void) {
    pthread_mutex_lock(&g_watchdog.mutex);
    
    if (!g_watchdog.enabled) {
        pthread_mutex_unlock(&g_watchdog.mutex);
        return FAILURE;
    }
    
    g_watchdog.last_feed_time = time(NULL);
    g_watchdog.missed_feeds = 0;
    
    pthread_mutex_unlock(&g_watchdog.mutex);
    
    LOG_DEBUG("Watchdog fed at %ld", g_watchdog.last_feed_time);
    return SUCCESS;
}

/**
 * @brief 检查看门狗状态
 * @return 检查结果：0表示正常，1表示超时，-1表示错误
 */
static int watchdog_check(void) {
    pthread_mutex_lock(&g_watchdog.mutex);
    
    if (!g_watchdog.enabled) {
        pthread_mutex_unlock(&g_watchdog.mutex);
        return -1;
    }
    
    time_t now = time(NULL);
    int time_since_last_feed = now - g_watchdog.last_feed_time;
    
    // 检查是否超时
    if (time_since_last_feed > g_watchdog.timeout_seconds) {
        g_watchdog.missed_feeds++;
        LOG_WARN("Watchdog timeout detected: %d seconds since last feed, missed feeds: %d",
                 time_since_last_feed, g_watchdog.missed_feeds);
        
        // 检查是否达到最大未喂狗次数
        if (g_watchdog.missed_feeds >= g_watchdog.max_missed_feeds) {
            LOG_ERROR("Watchdog max missed feeds reached: %d, system recovery required",
                      g_watchdog.missed_feeds);
            pthread_mutex_unlock(&g_watchdog.mutex);
            return 1; // 超时
        }
    } else {
        // 正常，重置未喂狗次数
        g_watchdog.missed_feeds = 0;
    }
    
    g_watchdog.last_check_time = now;
    
    pthread_mutex_unlock(&g_watchdog.mutex);
    return 0; // 正常
}

/**
 * @brief 看门狗监控线程
 * @param arg 线程参数
 * @return 线程返回值
 */
static void *watchdog_thread(void *arg) {
    LOG_INFO("Watchdog thread started");
    
    while (g_watchdog.running) {
        // 休眠检查间隔
        sleep(g_watchdog.check_interval);
        
        // 检查看门狗状态
        int result = watchdog_check();
        if (result == 1) {
            // 看门狗超时，触发系统恢复
            LOG_ERROR("Watchdog timeout, triggering system recovery");
            system_recovery();
        }
        
        // 喂狗
        watchdog_feed();
    }
    
    LOG_INFO("Watchdog thread exited");
    return NULL;
}

/**
 * @brief 启动看门狗线程
 * @return 启动结果：0表示成功，非0表示失败
 */
static int watchdog_start(void) {
    int ret = pthread_create(&g_watchdog.thread_id, NULL, watchdog_thread, NULL);
    if (ret != 0) {
        LOG_ERROR("Failed to create watchdog thread: %d", ret);
        return FAILURE;
    }
    
    LOG_INFO("Watchdog thread started");
    return SUCCESS;
}

/**
 * @brief 保存系统状态备份
 * @return 保存结果：0表示成功，非0表示失败
 */
static int save_system_state_backup(void) {
    // 创建配置目录
    system("mkdir -p ./config");
    
    // 打开文件
    FILE *fp = fopen(SYSTEM_STATE_BACKUP_FILE_PATH, "w");
    if (!fp) {
        LOG_ERROR("Failed to open system state backup file for writing");
        return FAILURE;
    }
    
    // 更新系统状态备份
    g_system_state_backup.system_state = system_api_get_state();
    g_system_state_backup.volume = volume_ctrl_get_master_volume();
    g_system_state_backup.mute = volume_ctrl_get_mute_state();
    g_system_state_backup.source = audio_source_get_current();
    g_system_state_backup.play_state = play_ctrl_get_state();
    g_system_state_backup.save_time = time(NULL);
    
    // 获取蓝牙设备
    bluetooth_get_connected_device(g_system_state_backup.bluetooth_device, sizeof(g_system_state_backup.bluetooth_device));
    
    // 获取WiFi SSID
    wifi_media_get_current_ssid(g_system_state_backup.wifi_ssid, sizeof(g_system_state_backup.wifi_ssid));
    
    // 写入系统状态备份
    fprintf(fp, "{");
    fprintf(fp, "\"system_state\": %d, ", g_system_state_backup.system_state);
    fprintf(fp, "\"volume\": %d, ", g_system_state_backup.volume);
    fprintf(fp, "\"mute\": %d, ", g_system_state_backup.mute);
    fprintf(fp, "\"source\": %d, ", g_system_state_backup.source);
    fprintf(fp, "\"play_state\": %d, ", g_system_state_backup.play_state);
    fprintf(fp, "\"bluetooth_device\": \"%s\", ", g_system_state_backup.bluetooth_device);
    fprintf(fp, "\"wifi_ssid\": \"%s\", ", g_system_state_backup.wifi_ssid);
    fprintf(fp, "\"save_time\": %ld", g_system_state_backup.save_time);
    fprintf(fp, "}");
    
    fclose(fp);
    
    LOG_INFO("System state backup saved to %s", SYSTEM_STATE_BACKUP_FILE_PATH);
    return SUCCESS;
}

/**
 * @brief 从文件恢复系统状态
 * @return 恢复结果：0表示成功，非0表示失败
 */
static int restore_system_state_backup(void) {
    // 打开文件
    FILE *fp = fopen(SYSTEM_STATE_BACKUP_FILE_PATH, "r");
    if (!fp) {
        LOG_DEBUG("System state backup file not found, using default values");
        return FAILURE;
    }
    
    // 读取文件内容
    char buffer[512] = {0};
    size_t read_len = fread(buffer, 1, sizeof(buffer) - 1, fp);
    fclose(fp);
    
    if (read_len == 0) {
        LOG_DEBUG("System state backup file is empty, using default values");
        return FAILURE;
    }
    
    // 简单解析JSON格式的系统状态备份
    int system_state = 0, volume = 0, mute = 0, source = 0, play_state = 0;
    char bluetooth_device[64] = {0}, wifi_ssid[64] = {0};
    long save_time = 0;
    
    sscanf(buffer, "{\"system_state\": %d, \"volume\": %d, \"mute\": %d, \"source\": %d, \"play_state\": %d, \"bluetooth_device\": \"%63[^\"]\", \"wifi_ssid\": \"%63[^\"]\", \"save_time\": %ld}",
           &system_state, &volume, &mute, &source, &play_state, bluetooth_device, wifi_ssid, &save_time);
    
    // 恢复系统状态
    g_system_state_backup.system_state = (SysState_e)system_state;
    g_system_state_backup.volume = volume;
    g_system_state_backup.mute = mute;
    g_system_state_backup.source = source;
    g_system_state_backup.play_state = play_state;
    strcpy(g_system_state_backup.bluetooth_device, bluetooth_device);
    strcpy(g_system_state_backup.wifi_ssid, wifi_ssid);
    g_system_state_backup.save_time = save_time;
    
    // 应用系统状态
    system_api_set_state((SysState_e)system_state);
    volume_ctrl_set_master_volume(volume);
    volume_ctrl_set_mute_state(mute);
    audio_source_set_current(source);
    play_ctrl_set_state(play_state);
    
    // 尝试重新连接蓝牙设备
    if (strlen(bluetooth_device) > 0) {
        bluetooth_connect(bluetooth_device);
    }
    
    // 尝试重新连接WiFi
    if (strlen(wifi_ssid) > 0) {
        wifi_media_connect(wifi_ssid, NULL);
    }
    
    LOG_INFO("System state restored from backup");
    return SUCCESS;
}

// 系统错误处理回调函数
static void system_error_callback(ErrorInfo_t *error_info)
{
    LOG_ERROR("System error callback: type=%d, level=%d, module=%s, message=%s, code=%d",
             error_info->type, error_info->level, error_info->module, error_info->message, error_info->code);
    
    // 更新系统监控数据
    g_sys_monitor.error_count++;
    
    // 处理不同级别的错误
    switch (error_info->level) {
        case ERROR_LEVEL_CRITICAL:
        case ERROR_LEVEL_FATAL:
            // 严重错误，触发系统恢复
            system_recovery();
            break;
        case ERROR_LEVEL_ERROR:
            // 记录错误
            break;
        default:
            break;
    }
}

// 系统错误恢复函数
static int system_error_recovery(ErrorInfo_t *error_info)
{
    LOG_INFO("System error recovery: type=%d, message=%s", error_info->type, error_info->message);
    
    // 增加恢复计数
    g_sys_monitor.recovery_count++;
    
    // 根据错误类型执行不同的恢复策略
    switch (error_info->type) {
        case ERROR_SYSTEM_RESOURCE:
            // 资源错误，释放资源
            system_release_resources();
            break;
        case ERROR_SYSTEM_MEMORY:
            // 内存错误，尝试内存回收
            system_memory_recovery();
            break;
        case ERROR_SYSTEM_TEMPERATURE:
            // 温度错误，降低系统负载
            system_reduce_load();
            break;
        default:
            break;
    }
    
    return SUCCESS;
}

/**
 * @brief 系统恢复函数
 * @return 恢复结果：0表示成功，非0表示失败
 */
static int system_recovery(void) {
    LOG_INFO("System recovery started");
    
    // 增加恢复计数
    g_sys_monitor.recovery_count++;
    
    // 1. 尝试从备份恢复系统状态
    int ret = restore_system_state_backup();
    if (ret == SUCCESS) {
        LOG_INFO("System state restored from backup");
    } else {
        LOG_WARN("Failed to restore system state from backup, using default values");
    }
    
    // 2. 执行自动恢复
    system_auto_recover();
    
    // 3. 喂看门狗
    watchdog_feed();
    
    // 4. 发送系统恢复完成事件
    // event_notify(1001, NULL); // 暂时注释掉，等待事件系统完善
    
    LOG_INFO("System recovery completed");
    return SUCCESS;
}
// 记录系统错误
static void log_system_error(int error_type, const char *error_msg) {
    // 使用统一错误处理机制
    error_report(error_type, ERROR_LEVEL_ERROR, "system", error_msg, 0, NULL);
}

// 获取系统温度（模拟实现）
static int get_system_temperature(void) {
    // 实际实现中应该从硬件获取温度
    // 这里返回模拟值
    return 45; // 假设温度为45度
}

// 获取系统电压（模拟实现）
static int get_system_voltage(void) {
    // 实际实现中应该从硬件获取电压
    // 这里返回模拟值
    return 5000; // 假设电压为5V
}

// 获取系统运行时间
static int get_system_uptime(void) {
    return (int)time(NULL);
}

// 系统自动恢复函数
static int system_auto_recover(void) {
    LOG_INFO("System auto recovery started (Attempt %d)", ++g_sys_monitor.recovery_count);
    
    int recovery_actions = 0;
    
    // 1. 检查音频核心
    if (audio_core_get_init_status() != SUCCESS) {
        LOG_WARN("Audio core not initialized, attempting recovery");
        if (audio_core_err_recover() == SUCCESS) {
            LOG_INFO("Audio core recovery successful");
            recovery_actions++;
        }
    }
    
    // 2. 检查蓝牙 - 实现自动重连
    if (!bluetooth_get_connect_state()) {
        LOG_WARN("Bluetooth not connected, attempting to reconnect");
        // 调用蓝牙重连函数
        if (bluetooth_reconnect() == SUCCESS) {
            LOG_INFO("Bluetooth reconnection initiated");
            recovery_actions++;
        }
    }
    
    // 3. 检查WiFi媒体 - 实现自动重连
    if (!wifi_media_get_connect_state()) {
        LOG_WARN("WiFi not connected, attempting to reconnect");
        // 调用WiFi重连函数
        if (wifi_media_reconnect() == SUCCESS) {
            LOG_INFO("WiFi reconnection initiated");
            recovery_actions++;
        }
    }
    
    // 4. 检查系统资源
    if (g_sys_monitor.cpu_usage > CPU_USAGE_THRESHOLD_CRITICAL) {
        LOG_WARN("High CPU usage, attempting to reduce load");
        // 降低CPU负载：关闭不必要的功能，降低音频质量等
        audio_core_set_low_power_mode(true);
        // 停止非关键进程
        process_manager_stop_non_critical_processes();
        recovery_actions++;
    }
    
    if (g_sys_monitor.mem_usage > MEM_USAGE_THRESHOLD_CRITICAL) {
        LOG_WARN("High memory usage, attempting to free memory");
        // 释放内存：清理缓存，关闭不必要的功能等
        memory_cleanup();
        // 减少音频缓冲区大小
        audio_core_set_buffer_size(50);
        recovery_actions++;
    }
    
    // 5. 检查系统温度
    if (g_sys_monitor.temp > 60) {
        LOG_WARN("High system temperature, attempting to cool down");
        // 降低温度：降低CPU频率，关闭不必要的功能等
        pal_system_set_cpu_frequency("low");
        audio_core_set_low_power_mode(true);
        recovery_actions++;
    }
    
    if (recovery_actions > 0) {
        LOG_INFO("System auto recovery completed, %d actions taken", recovery_actions);
        // 发送系统恢复事件
        // event_notify(1001, (void *)&recovery_actions); // 暂时注释掉，等待事件系统完善
        return SUCCESS;
    } else {
        LOG_INFO("System auto recovery completed, no actions needed");
        return SUCCESS;
    }
}

// 保存系统状态到文件
static void save_system_state(void) {
    // 创建配置目录
    system("mkdir -p ./config");
    
    // 打开文件
    FILE *fp = fopen(SYSTEM_STATE_FILE_PATH, "w");
    if (!fp) {
        LOG_ERROR("Failed to open system state file for writing");
        return;
    }
    
    // 写入系统状态
    fprintf(fp, "{");
    fprintf(fp, "\"cpu_usage\": %d, ", g_sys_monitor.cpu_usage);
    fprintf(fp, "\"mem_usage\": %d, ", g_sys_monitor.mem_usage);
    fprintf(fp, "\"temp\": %d, ", g_sys_monitor.temp);
    fprintf(fp, "\"voltage\": %d, ", g_sys_monitor.voltage);
    fprintf(fp, "\"uptime\": %d, ", g_sys_monitor.uptime);
    fprintf(fp, "\"error_count\": %d, ", g_sys_monitor.error_count);
    fprintf(fp, "\"recovery_count\": %d, ", g_sys_monitor.recovery_count);
    fprintf(fp, "\"last_state\": %d, ", g_sys_monitor.last_state);
    fprintf(fp, "\"state_change_count\": %d", g_sys_monitor.state_change_count);
    fprintf(fp, "}");
    
    fclose(fp);
    LOG_DEBUG("System state saved to %s", SYSTEM_STATE_FILE_PATH);
}

// 从文件恢复系统状态
static void restore_system_state(void) {
    // 打开文件
    FILE *fp = fopen(SYSTEM_STATE_FILE_PATH, "r");
    if (!fp) {
        LOG_DEBUG("System state file not found, using default values");
        return;
    }
    
    // 读取文件内容
    char buffer[256] = {0};
    size_t read_len = fread(buffer, 1, sizeof(buffer) - 1, fp);
    fclose(fp);
    
    if (read_len == 0) {
        LOG_DEBUG("System state file is empty, using default values");
        return;
    }
    
    // 简单解析JSON格式的系统状态
    // 这里使用简化的解析方式，实际项目中应该使用JSON库
    int cpu_usage = 0, mem_usage = 0, temp = 0, voltage = 0;
    int uptime = 0, error_count = 0, recovery_count = 0;
    int last_state = 0, state_change_count = 0;
    
    sscanf(buffer, "{\"cpu_usage\": %d, \"mem_usage\": %d, \"temp\": %d, \"voltage\": %d, \"uptime\": %d, \"error_count\": %d, \"recovery_count\": %d, \"last_state\": %d, \"state_change_count\": %d}",
           &cpu_usage, &mem_usage, &temp, &voltage, &uptime, &error_count, &recovery_count, &last_state, &state_change_count);
    
    // 恢复系统状态
    g_sys_monitor.cpu_usage = cpu_usage;
    g_sys_monitor.mem_usage = mem_usage;
    g_sys_monitor.temp = temp;
    g_sys_monitor.voltage = voltage;
    g_sys_monitor.uptime = uptime;
    g_sys_monitor.error_count = error_count;
    g_sys_monitor.recovery_count = recovery_count;
    g_sys_monitor.last_state = (SysState_e)last_state;
    g_sys_monitor.state_change_count = state_change_count;
    
    LOG_INFO("System state restored from %s", SYSTEM_STATE_FILE_PATH);
}

// 系统状态检查函数
static void check_system_status(void) {
    // 获取系统资源使用情况
    pal_system_get_cpu_usage(&g_sys_monitor.cpu_usage);
    pal_system_get_memory_usage(&g_sys_monitor.mem_usage);
    
    // 获取系统温度和电压
    g_sys_monitor.temp = get_system_temperature();
    g_sys_monitor.voltage = get_system_voltage();
    
    // 更新系统运行时间
    g_sys_monitor.uptime = get_system_uptime();
    
    // 检查CPU使用率
    if (g_sys_monitor.cpu_usage > CPU_USAGE_THRESHOLD_CRITICAL) {
        LOG_WARN("CRITICAL: CPU usage too high: %d%%", g_sys_monitor.cpu_usage);
        log_system_error(1, "CPU usage critical");
        // 触发自动恢复
        system_auto_recover();
    } else if (g_sys_monitor.cpu_usage > CPU_USAGE_THRESHOLD_HIGH) {
        LOG_WARN("WARNING: CPU usage high: %d%%", g_sys_monitor.cpu_usage);
    }
    
    // 检查内存使用率
    if (g_sys_monitor.mem_usage > MEM_USAGE_THRESHOLD_CRITICAL) {
        LOG_WARN("CRITICAL: Memory usage too high: %d%%", g_sys_monitor.mem_usage);
        log_system_error(2, "Memory usage critical");
        // 触发自动恢复
        system_auto_recover();
    } else if (g_sys_monitor.mem_usage > MEM_USAGE_THRESHOLD_HIGH) {
        LOG_WARN("WARNING: Memory usage high: %d%%", g_sys_monitor.mem_usage);
    }
    
    // 检查系统温度
    if (g_sys_monitor.temp > 70) {
        LOG_WARN("CRITICAL: System temperature too high: %dC", g_sys_monitor.temp);
        log_system_error(3, "System temperature critical");
        // 触发自动恢复
        system_auto_recover();
    } else if (g_sys_monitor.temp > 60) {
        LOG_WARN("WARNING: System temperature high: %dC", g_sys_monitor.temp);
    }
    
    // 检查系统电压
    if (g_sys_monitor.voltage < 4500 || g_sys_monitor.voltage > 5500) {
        LOG_WARN("WARNING: System voltage out of range: %d mV", g_sys_monitor.voltage);
        log_system_error(4, "System voltage out of range");
    }
    
    // 检查系统状态变化
    SysState_e current_state = system_api_get_state();
    if (current_state != g_sys_monitor.last_state) {
        g_sys_monitor.state_change_count++;
        LOG_INFO("System state changed: %d -> %d (Change count: %d)", 
                 g_sys_monitor.last_state, current_state, g_sys_monitor.state_change_count);
        g_sys_monitor.last_state = current_state;
        
        // 保存系统状态
        save_system_state();
    }
    
    // 定期保存系统状态（每30秒）
    static time_t last_save_time = 0;
    time_t now = time(NULL);
    if (now - last_save_time > 30) {
        save_system_state();
        last_save_time = now;
    }
}

// 打印系统状态摘要
static void print_system_status_summary(void) {
    LOG_INFO("System Status Summary:");
    LOG_INFO("  CPU Usage: %d%%", g_sys_monitor.cpu_usage);
    LOG_INFO("  Memory Usage: %d%%", g_sys_monitor.mem_usage);
    LOG_INFO("  Temperature: %dC", g_sys_monitor.temp);
    LOG_INFO("  Voltage: %d mV", g_sys_monitor.voltage);
    LOG_INFO("  Uptime: %d seconds", g_sys_monitor.uptime);
    LOG_INFO("  Error Count: %d", g_sys_monitor.error_count);
    LOG_INFO("  Recovery Count: %d", g_sys_monitor.recovery_count);
    LOG_INFO("  State Change Count: %d", g_sys_monitor.state_change_count);
    LOG_INFO("  Current State: %d", system_api_get_state());
    
    // 打印最近的错误
    if (g_sys_monitor.error_count > 0) {
        LOG_INFO("  Recent Errors: %d", g_sys_monitor.error_count);
        // 这里可以添加错误日志的详细信息
    }
}

int system_init(void)
{
    memset(&g_sys_cfg, 0, sizeof(SystemCfg_t));
    // 必加载：系统基础初始化 所有产品通用
    sys_init_core();
    // 宏控加载：OTA升级功能 分级裁剪
    sys_ota_init();
    // 初始化系统API
    system_api_init();
    
    // 初始化系统监控数据
    memset(&g_sys_monitor, 0, sizeof(SystemMonitor_t));
    g_sys_monitor.last_state = SYS_STATE_IDLE;
    
    // 从文件恢复系统状态
    restore_system_state();
    
    // 初始化统一错误处理模块
    error_handling_init();
    
    // 注册系统错误回调函数
    error_register_callback(ERROR_SYSTEM_BASE, system_error_callback);
    error_register_recovery(ERROR_SYSTEM_BASE, system_error_recovery);
    
    // 初始化看门狗
    watchdog_init(WATCHDOG_TIMEOUT_SECONDS, WATCHDOG_CHECK_INTERVAL_SECONDS, WATCHDOG_MAX_MISSED_FEEDS);
    watchdog_start();
    
    // 初始化系统状态备份
    memset(&g_system_state_backup, 0, sizeof(SystemStateBackup_t));
    save_system_state_backup();
    
    g_sys_cfg.init_ok = 1;
    LOG_INFO("System module init success (OTA: %d)", CONFIG_ENABLE_DUAL_OTA);
    return 0;
}

void system_deinit(void)
{
    if (g_sys_cfg.init_ok)
    {
        // 保存系统状态备份
        save_system_state_backup();
        
        // 反初始化看门狗
        watchdog_deinit();
        
        sys_ota_deinit();
        sys_init_deinit();
        // 反初始化系统API
        system_api_deinit();
        // 反初始化错误处理模块
        error_handling_deinit();
        g_sys_cfg.init_ok = 0;
        LOG_INFO("System module deinit success");
    }
}

/**
 * @brief 设置系统状态
 * @param state 系统状态
 * @return SUCCESS表示成功，FAILURE表示失败
 */
int system_set_state(SysState_e state) {
    LOG_INFO("Setting system state to: %d", state);
    
    // 验证状态值
    if (state < SYS_STATE_IDLE || state > SYS_STATE_STANDBY) {
        LOG_ERROR("Invalid system state: %d", state);
        return FAILURE;
    }
    
    // 处理特殊状态 - 待机状态
    if (state == SYS_STATE_STANDBY) {
        LOG_INFO("System entering standby state");
        // 执行待机操作
        led_ctrl_set_state(LED_SYSTEM, LED_STATE_OFF);
        lcd_display_text(0, 0, "System: Standby");
        event_notify(EVENT_SYSTEM_STANDBY, NULL);
        return SUCCESS;
    }
    
    // 调用系统API设置状态
    int ret = system_api_set_state(state);
    if (ret != SUCCESS) {
        LOG_ERROR("Failed to set system state: %d", ret);
        return FAILURE;
    }
    
    LOG_INFO("System state set to: %d successfully", state);
    return SUCCESS;
}

void system_event_poll(void)
{
    if (!g_sys_cfg.init_ok) return;
    
    // 轮询系统状态变化
    static SysState_e last_sys_state = SYS_STATE_IDLE;
    SysState_e current_sys_state = system_api_get_state();
    
    if (current_sys_state != last_sys_state) {
        LOG_INFO("System state changed: %d -> %d", last_sys_state, current_sys_state);
        last_sys_state = current_sys_state;
        
        // 根据系统状态执行相应操作
        switch (current_sys_state) {
            case SYS_STATE_WORKING:
                // 处理系统工作状态
                LOG_INFO("System entering working state");
                led_ctrl_set_state(LED_SYSTEM, LED_STATE_ON);
                lcd_display_text(0, 0, "System: Working");
                event_notify(EVENT_SYSTEM_WORKING, NULL);
                break;
            case SYS_STATE_OTA:
                // 处理系统OTA升级状态
                LOG_INFO("System entering OTA state");
                led_ctrl_set_state(LED_SYSTEM, LED_STATE_BREATH);
                lcd_display_text(0, 0, "System: OTA");
                event_notify(EVENT_SYSTEM_OTA_START, NULL);
                break;
            case SYS_STATE_CALIB:
                // 处理系统校准状态
                LOG_INFO("System entering calibration state");
                led_ctrl_set_state(LED_SYSTEM, LED_STATE_FLASH_SLOW);
                lcd_display_text(0, 0, "System: Calib");
                event_notify(EVENT_SYSTEM_CALIB_START, NULL);
                break;
            case SYS_STATE_AGE_TEST:
                // 处理系统老化测试状态
                LOG_INFO("System entering age test state");
                led_ctrl_set_state(LED_SYSTEM, LED_STATE_FLASH_FAST);
                lcd_display_text(0, 0, "System: AgeTest");
                event_notify(EVENT_SYSTEM_AGE_TEST_START, NULL);
                break;
            case SYS_STATE_ERROR:
                // 处理系统错误状态
                LOG_INFO("System entering error state");
                led_ctrl_set_state(LED_SYSTEM, LED_STATE_FLASH_FAST);
                lcd_display_text(0, 0, "System: Error");
                event_notify(EVENT_SYSTEM_ERROR, NULL);
                break;
            default:
                break;
        }
    }
    
    // 轮询系统资源使用情况
    static uint32_t last_check_time = 0;
    static uint32_t last_summary_time = 0;
    static uint32_t last_backup_time = 0;
    static uint32_t last_watchdog_feed = 0;
    uint32_t current_time = (uint32_t)time(NULL) * 1000;
    
    if (current_time - last_check_time > 2000) { // 每2秒检查一次
        last_check_time = current_time;
        
        // 检查系统状态
        check_system_status();
        
        // 发送系统状态事件
        event_notify(EVENT_SYSTEM_STATUS_UPDATE, NULL);
    }
    
    // 每30秒打印一次系统状态摘要
    if (current_time - last_summary_time > 30000) {
        last_summary_time = current_time;
        print_system_status_summary();
    }
    
    // 每60秒保存一次系统状态备份
    if (current_time - last_backup_time > 60000) {
        last_backup_time = current_time;
        save_system_state_backup();
    }
    
    // 每10秒喂一次看门狗
    if (current_time - last_watchdog_feed > 10000) {
        last_watchdog_feed = current_time;
        watchdog_feed();
    }
    
    // 轮询系统错误日志
    static uint32_t last_error_check = 0;
    if (current_time - last_error_check > 10000) { // 每10秒检查一次
        last_error_check = current_time;
        
        // 系统错误日志检查
        if (g_sys_monitor.error_count > 0) {
            LOG_INFO("System error log check - %d errors recorded", g_sys_monitor.error_count);
        } else {
            LOG_DEBUG("System error log check - no errors");
        }
        
        // 发送系统错误日志检查事件
        // event_notify(1002, NULL); // 暂时注释掉，等待事件系统完善
    }
}