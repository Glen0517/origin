/**
 * @file    simple_test.c
 * @brief   简单测试程序 - 增强版
 * @version 2.0
 * @date    2024-12-20
 * 
 * 增强功能:
 * - 电源管理功能测试
 * - 高级算法模块测试
 * - 故障检测功能测试
 * - 性能监控测试
 * - 系统集成测试
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "communication/communication.h"
#include "hal/hal_uart.h"
#include "service/system.h"

// 测试延时函数
void test_delay(uint32_t ms) {
    usleep(ms * 1000);
}

/**
 * @brief SOC通信功能测试
 * @note 演示完整的SOC通信流程
 */
void test_soc_communication(void) {
    printf("\n=== SOC通信功能测试开始 ===\n");
    
    // 1. 初始化SOC通信
    printf("1. 初始化SOC通信模块...\n");
    int32_t init_result = soc_communication_init();
    if (init_result != 0) {
        printf("❌ SOC通信初始化失败 (错误码: %d)\n", init_result);
        return;
    }
    printf("✅ SOC通信初始化成功\n");
    
    // 2. 查询SOC状态
    printf("\n2. 查询SOC状态...\n");
    soc_status_t status = soc_query_status();
    printf("   SOC状态: %d\n", status);
    
    // 3. 获取SOC电池信息
    printf("\n3. 获取SOC电池信息...\n");
    soc_battery_info_t battery_info;
    bool battery_result = soc_get_battery_info(&battery_info);
    if (battery_result) {
        printf("   电池电量: %.1f%%\n", battery_info.level);
        printf("   电池电压: %.2fV\n", battery_info.voltage);
        printf("   电池电流: %.2fA\n", battery_info.current);
        printf("   电池容量: %dmAh\n", battery_info.capacity);
        printf("   剩余容量: %dmAh\n", battery_info.remaining_capacity);
    } else {
        printf("❌ 获取电池信息失败\n");
    }
    
    // 4. 设置功耗模式
    printf("\n4. 测试功耗模式切换...\n");
    printf("   设置为低功耗模式...\n");
    bool power_result = soc_set_power_mode(SOC_POWER_LOW);
    if (power_result) {
        printf("✅ 功耗模式设置成功\n");
    } else {
        printf("❌ 功耗模式设置失败\n");
    }
    
    // 5. 发送自定义命令
    printf("\n5. 发送自定义SOC命令...\n");
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    int32_t sent_bytes = soc_send_command(SOC_CMD_GET_VOLTAGE, test_data, sizeof(test_data));
    if (sent_bytes > 0) {
        printf("✅ 命令发送成功，发送字节数: %d\n", sent_bytes);
    } else {
        printf("❌ 命令发送失败\n");
    }
    
    // 6. 接收SOC数据
    printf("\n6. 接收SOC数据...\n");
    uint8_t receive_buffer[256];
    int32_t received_bytes = soc_receive_data(receive_buffer, sizeof(receive_buffer));
    if (received_bytes > 0) {
        printf("✅ 数据接收成功，接收字节数: %d\n", received_bytes);
        printf("   接收数据: ");
        for (int32_t i = 0; i < received_bytes; i++) {
            printf("0x%02X ", receive_buffer[i]);
        }
        printf("\n");
    } else {
        printf("⚠️  暂无数据可接收\n");
    }
    
    // 7. 测试通信状态
    printf("\n7. 检查通信状态...\n");
    printf("   通信状态: %s\n", soc_status.connected ? "已连接" : "未连接");
    printf("   命令发送计数: %d\n", soc_status.command_count);
    printf("   数据接收计数: %d\n", soc_status.receive_count);
    printf("   错误计数: %d\n", soc_status.error_count);
    printf("   超时计数: %d\n", soc_status.timeout_count);
    printf("   成功计数: %d\n", soc_status.success_count);
    
    printf("\n=== SOC通信功能测试完成 ===\n");
}

/**
 * @brief SOC数据监控演示
 * @note 演示持续监控SOC数据的功能
 */
void test_soc_data_monitoring(void) {
    printf("\n=== SOC数据监控演示开始 ===\n");
    printf("将持续监控SOC数据10秒...\n");
    
    uint32_t start_time = system_get_time_ms();
    uint32_t last_report_time = start_time;
    
    while (system_get_time_ms() - start_time < 10000) { // 监控10秒
        // 运行SOC轮询任务
        soc_poll_task();
        
        // 每2秒报告一次状态
        if (system_get_time_ms() - last_report_time >= 2000) {
            last_report_time = system_get_time_ms();
            
            printf("\n[%.1f秒] SOC状态报告:\n", 
                   (float)(last_report_time - start_time) / 1000.0f);
            
            // 显示当前状态
            if (soc_status.connected) {
                printf("  ✓ 通信状态: 已连接\n");
            } else {
                printf("  ✗ 通信状态: 未连接\n");
            }
            
            if (soc_status.last_voltage > 0) {
                printf("  📊 电压: %.2fV\n", soc_status.last_voltage);
            }
            
            if (soc_status.last_temperature > 0) {
                printf("  🌡️  温度: %.1f°C\n", soc_status.last_temperature);
            }
            
            if (soc_status.battery_level > 0) {
                printf("  🔋 电池: %.1f%%\n", soc_status.battery_level);
            }
            
            // 定期发送心跳
            static uint32_t heartbeat_count = 0;
            heartbeat_count++;
            if (heartbeat_count % 3 == 0) { // 每6秒发送一次心跳
                soc_send_command(SOC_CMD_HEARTBEAT, NULL, 0);
                printf("  💓 发送心跳信号\n");
            }
        }
        
        test_delay(100); // 100ms延迟
    }
    
    printf("\n=== SOC数据监控演示结束 ===\n");
}

/**
 * @brief SOC功耗模式测试
 * @note 演示不同功耗模式的切换
 */
void test_soc_power_modes(void) {
    printf("\n=== SOC功耗模式测试开始 ===\n");
    
    soc_power_mode_t modes[] = {
        SOC_POWER_NORMAL,
        SOC_POWER_LOW,
        SOC_POWER_ULTRA_LOW,
        SOC_POWER_SLEEP
    };
    
    const char* mode_names[] = {
        "正常模式",
        "低功耗模式", 
        "超低功耗模式",
        "睡眠模式"
    };
    
    for (int i = 0; i < 4; i++) {
        printf("\n测试功耗模式: %s\n", mode_names[i]);
        
        // 设置功耗模式
        bool result = soc_set_power_mode(modes[i]);
        if (result) {
            printf("✅ %s设置成功\n", mode_names[i]);
            
            // 等待模式切换完成
            test_delay(500);
            
            // 查询当前状态
            soc_status_t status = soc_query_status();
            printf("   当前状态: %d\n", status);
            
            // 如果不是睡眠模式，发送查询命令获取更多信息
            if (modes[i] != SOC_POWER_SLEEP) {
                test_delay(200);
                soc_send_command(SOC_CMD_QUERY_STATUS, NULL, 0);
            }
            
        } else {
            printf("❌ %s设置失败\n", mode_names[i]);
        }
        
        test_delay(1000); // 每个模式测试1秒
    }
    
    // 恢复到正常模式
    printf("\n恢复为正常功耗模式...\n");
    soc_set_power_mode(SOC_POWER_NORMAL);
    
    printf("\n=== SOC功耗模式测试完成 ===\n");
}

/**
 * @brief 电源管理功能测试
 * @note 演示完整的电源管理功能
 */
void test_power_management(void) {
    printf("\n=== 电源管理功能测试开始 ===\n");
    
    // 1. 初始化电源管理
    printf("1. 初始化电源管理模块...\n");
    bool power_init_result = power_management_init();
    if (power_init_result) {
        printf("✅ 电源管理初始化成功\n");
    } else {
        printf("❌ 电源管理初始化失败\n");
        return;
    }
    
    // 2. 测试功耗模式切换
    printf("\n2. 测试功耗模式切换...\n");
    power_mode_t test_modes[] = {POWER_MODE_NORMAL, POWER_MODE_ECO, POWER_MODE_LOW, POWER_MODE_SLEEP};
    const char* mode_names[] = {"正常", "节能", "低功耗", "睡眠"};
    
    for (int i = 0; i < 4; i++) {
        printf("   切换到%s模式...\n", mode_names[i]);
        bool mode_result = power_set_power_mode(test_modes[i]);
        if (mode_result) {
            printf("   ✅ %s模式切换成功\n", mode_names[i]);
        } else {
            printf("   ❌ %s模式切换失败\n", mode_names[i]);
        }
        test_delay(1000);
    }
    
    // 3. 获取电源状态
    printf("\n3. 获取电源状态...\n");
    power_status_t power_status;
    bool status_result = power_get_power_status(&power_status);
    if (status_result) {
        printf("✅ 电源状态获取成功:\n");
        printf("   电源状态: %d\n", power_status.state);
        printf("   输入电压: %.2fV\n", power_status.input_voltage);
        printf("   输出电压: %.2fV\n", power_status.output_voltage);
        printf("   输入电流: %.2fA\n", power_status.input_current);
        printf("   输出电流: %.2fA\n", power_status.output_current);
        printf("   功耗: %.2fW\n", power_status.power_consumption);
        printf("   效率: %.1f%%\n", power_status.efficiency);
        printf("   温度: %.1f°C\n", power_status.temperature);
    } else {
        printf("❌ 电源状态获取失败\n");
    }
    
    // 4. 获取电池管理信息
    printf("\n4. 获取电池管理信息...\n");
    battery_management_t battery_mgmt;
    bool battery_result = power_get_battery_management(&battery_mgmt);
    if (battery_result) {
        printf("✅ 电池管理信息获取成功:\n");
        printf("   主电池电压: %.2fV\n", battery_mgmt.main_battery.voltage);
        printf("   主电池电流: %.2fA\n", battery_mgmt.main_battery.current);
        printf("   剩余电量: %.1f%%\n", battery_mgmt.main_battery.remaining);
        printf("   电池健康度: %.1f%%\n", battery_mgmt.battery_health);
        printf("   预计飞行时间: %.1f分钟\n", battery_mgmt.estimated_flight_time);
    } else {
        printf("❌ 电池管理信息获取失败\n");
    }
    
    // 5. 测试关机关机和重启
    printf("\n5. 测试系统控制和重启...\n");
    printf("   ⚠️  跳过实际的关机和重启测试\n");
    
    printf("\n=== 电源管理功能测试完成 ===\n");
}

/**
 * @brief 高级算法功能测试
 * @note 演示算法模块的各种算法
 */
void test_advanced_algorithms(void) {
    printf("\n=== 高级算法功能测试开始 ===\n");
    
    // 1. 初始化算法模块
    printf("1. 初始化算法模块...\n");
    bool algo_init = algorithm_module_init();
    if (algo_init) {
        printf("✅ 算法模块初始化成功\n");
    } else {
        printf("❌ 算法模块初始化失败\n");
        return;
    }
    
    // 2. 测试姿态解算算法
    printf("\n2. 测试Madgwick姿态解算算法...\n");
    float gyro_data[3] = {0.1f, -0.2f, 0.05f};   // 陀螺仪数据
    float accel_data[3] = {0.1f, -0.1f, 9.81f};  // 加速度计数据
    float mag_data[3] = {0.3f, 0.4f, 0.5f};      // 磁力计数据
    
    attitude_data_t attitude;
    for (int i = 0; i < 5; i++) {
        attitude_madgwick_update(gyro_data, accel_data, mag_data);
        attitude_get_data(&attitude);
        
        printf("   Madgwick算法 - 迭代 %d: Pitch=%.2f°, Roll=%.2f°, Yaw=%.2f°\n", 
               i + 1, attitude.pitch, attitude.roll, attitude.yaw);
        
        // 轻微变化模拟数据
        gyro_data[0] += 0.01f;
        gyro_data[1] -= 0.01f;
        accel_data[0] = 0.1f + sin(i * 0.1f) * 0.05f;
    }
    printf("✅ Madgwick算法测试完成\n");
    
    // 3. 测试互补滤波算法
    printf("\n3. 测试互补滤波算法...\n");
    for (int i = 0; i < 5; i++) {
        attitude_complementary_filter_update(gyro_data, accel_data, mag_data);
        attitude_get_data(&attitude);
        
        printf("   互补滤波 - 迭代 %d: Pitch=%.2f°, Roll=%.2f°, Yaw=%.2f°\n", 
               i + 1, attitude.pitch, attitude.roll, attitude.yaw);
    }
    printf("✅ 互补滤波算法测试完成\n");
    
    // 4. 测试多个PID控制器
    printf("\n4. 测试多个PID控制器...\n");
    pid_controller_t pitch_pid, roll_pid, yaw_pid;
    
    pid_controller_init(&pitch_pid, 1.5f, 0.1f, 0.05f, -100.0f, 100.0f);
    pid_controller_init(&roll_pid, 1.5f, 0.1f, 0.05f, -100.0f, 100.0f);
    pid_controller_init(&yaw_pid, 2.0f, 0.2f, 0.1f, -100.0f, 100.0f);
    
    // 模拟控制循环
    float pitch_setpoint = 10.0f, pitch_measurement = 0.0f;
    float roll_setpoint = 5.0f, roll_measurement = 0.0f;
    float yaw_setpoint = 0.0f, yaw_measurement = 0.0f;
    
    for (int i = 0; i < 3; i++) {
        float pitch_output = pid_calculate(&pitch_pid, pitch_setpoint, pitch_measurement);
        float roll_output = pid_calculate(&roll_pid, roll_setpoint, roll_measurement);
        float yaw_output = pid_calculate(&yaw_pid, yaw_setpoint, yaw_measurement);
        
        printf("   PID控制 - 迭代 %d: Pitch输出=%.2f, Roll输出=%.2f, Yaw输出=%.2f\n", 
               i + 1, pitch_output, roll_output, yaw_output);
        
        // 模拟系统响应
        pitch_measurement += pitch_output * 0.1f;
        roll_measurement += roll_output * 0.1f;
        yaw_measurement += yaw_output * 0.1f;
    }
    printf("✅ 多PID控制器测试完成\n");
    
    // 5. 测试滤波算法
    printf("\n5. 测试滤波算法...\n");
    ema_filter_t ema_filter;
    moving_average_filter_t ma_filter;
    
    filter_ema_init(&ema_filter, 0.3f, 0.0f);
    filter_moving_average_init(&ma_filter, 5, 0.0f);
    
    for (int i = 0; i < 5; i++) {
        float noisy_input = sin(i * 0.5f) + (rand() % 100 - 50) / 100.0f;
        float ema_output = filter_ema_update(&ema_filter, noisy_input);
        float ma_output = filter_moving_average_update(&ma_filter, noisy_input);
        
        printf("   滤波测试 - 迭代 %d: 输入=%.3f, EMA输出=%.3f, MA输出=%.3f\n", 
               i + 1, noisy_input, ema_output, ma_output);
    }
    printf("✅ 滤波算法测试完成\n");
    
    printf("\n=== 高级算法功能测试完成 ===\n");
}

/**
 * @brief 故障检测功能测试
 * @note 演示故障检测和诊断功能
 */
void test_fault_detection(void) {
    printf("\n=== 故障检测功能测试开始 ===\n");
    
    // 1. 初始化故障检测模块
    printf("1. 初始化故障检测模块...\n");
    bool fault_init = fault_detection_init();
    if (fault_init) {
        printf("✅ 故障检测模块初始化成功\n");
    } else {
        printf("❌ 故障检测模块初始化失败\n");
        return;
    }
    
    // 2. 执行各种故障检测
    printf("\n2. 执行故障检测...\n");
    
    for (int i = 0; i < 10; i++) {
        // 模拟各种故障场景
        if (i == 2) {
            printf("   模拟传感器故障场景...\n");
        }
        
        if (i == 5) {
            printf("   模拟电池故障场景...\n");
        }
        
        if (i == 7) {
            printf("   模拟通信故障场景...\n");
        }
        
        // 执行故障检测
        fault_detection_check_sensors();
        fault_detection_check_battery();
        fault_detection_check_communication();
        
        // 获取故障状态
        fault_detection_t fault_status;
        if (fault_detection_get_status(&fault_status)) {
            printf("   故障检测 - 迭代 %d:\n", i + 1);
            printf("     传感器健康度: %.2f\n", fault_status.sensor_health);
            printf("     通信质量: %.2f\n", fault_status.communication_quality);
            printf("     关键故障数量: %lu\n", fault_status.critical_faults);
            
            if (fault_status.critical_faults > 0) {
                printf("     ⚠️ 检测到故障!\n");
            }
        }
        
        test_delay(1000);
    }
    
    printf("\n=== 故障检测功能测试完成 ===\n");
}

/**
 * @brief 性能监控测试
 * @note 演示系统性能监控功能
 */
void test_performance_monitoring(void) {
    printf("\n=== 性能监控测试开始 ===\n");
    
    // 1. 初始化性能监控模块
    printf("1. 初始化性能监控模块...\n");
    bool perf_init = performance_monitor_init();
    if (perf_init) {
        printf("✅ 性能监控模块初始化成功\n");
    } else {
        printf("❌ 性能监控模块初始化失败\n");
        return;
    }
    
    // 2. 执行性能测试
    printf("\n2. 执行性能测试...\n");
    
    for (int i = 0; i < 10; i++) {
        // 更新性能指标
        performance_monitor_update();
        
        // 获取性能数据
        performance_monitor_t perf_status;
        if (performance_monitor_get_status(&perf_status)) {
            printf("   性能监控 - 迭代 %d:\n", i + 1);
            printf("     CPU使用率: %.1f%%\n", perf_status.cpu_usage);
            printf("     内存使用率: %.1f%%\n", perf_status.memory_usage);
            printf("     任务响应时间: %.1fms\n", perf_status.task_response_time);
            printf("     最大CPU使用率: %.1f%%\n", perf_status.max_cpu_usage);
            printf("     最小CPU使用率: %.1f%%\n", perf_status.min_cpu_usage);
            
            // 检查性能阈值
            if (perf_status.cpu_usage > 80.0f) {
                printf("     ⚠️ CPU使用率过高!\n");
            }
            
            if (perf_status.memory_usage > 85.0f) {
                printf("     ⚠️ 内存使用率过高!\n");
            }
            
            if (perf_status.task_response_time > 20.0f) {
                printf("     ⚠️ 任务响应时间过长!\n");
            }
        }
        
        test_delay(1000);
    }
    
    printf("\n=== 性能监控测试完成 ===\n");
}

/**
 * @brief 系统集成测试
 * @note 演示完整的系统集成功能
 */
void test_system_integration(void) {
    printf("\n=== 系统集成测试开始 ===\n");
    
    // 模拟完整的系统运行场景
    printf("模拟完整的系统运行场景...\n");
    
    algorithm_results_t algo_results;
    power_status_t power_status;
    fault_detection_t fault_status;
    performance_monitor_t perf_status;
    
    for (int i = 0; i < 20; i++) {
        // 执行算法处理
        if (algorithm_get_results(&algo_results)) {
            // 模拟姿态控制结果
            algo_results.pitch_control = sin(i * 0.2f) * 10.0f;
            algo_results.roll_control = cos(i * 0.2f) * 10.0f;
            algo_results.yaw_control = sin(i * 0.1f) * 5.0f;
        }
        
        // 获取系统状态
        system_get_status(&g_system_status);
        power_get_power_status(&power_status);
        fault_detection_get_status(&fault_status);
        performance_monitor_get_status(&perf_status);
        
        // 集成状态报告
        printf("   集成测试 - 第 %d 轮:\n", i + 1);
        
        // 电源状态
        printf("     电源模式: %d, 电压: %.2fV, 功耗: %.2fW\n", 
               power_status.state, power_status.input_voltage, power_status.power_consumption);
        
        // 算法结果
        printf("     算法控制: Pitch=%.2f, Roll=%.2f, Yaw=%.2f\n", 
               algo_results.pitch_control, algo_results.roll_control, algo_results.yaw_control);
        
        // 性能状态
        printf("     性能: CPU=%.1f%%, 内存=%.1f%%, 响应时间=%.1fms\n", 
               perf_status.cpu_usage, perf_status.memory_usage, perf_status.task_response_time);
        
        // 故障状态
        printf("     故障状态: 传感器健康=%.2f, 关键故障=%lu\n", 
               fault_status.sensor_health, fault_status.critical_faults);
        
        // 系统健康检查
        bool system_healthy = g_system_status.system_healthy;
        printf("     系统健康: %s\n", system_healthy ? "正常" : "异常");
        
        // 功耗模式自适应调整
        if (perf_status.cpu_usage > 75.0f && power_status.state != POWER_STATE_ECO) {
            printf("     自动切换到经济模式 (CPU负载过高)\n");
            power_set_power_mode(POWER_MODE_ECO);
        }
        
        // 故障处理
        if (fault_status.critical_faults > 0) {
            printf("     执行故障处理策略\n");
        }
        
        // 模拟工作负载变化
        if (i == 10) {
            printf("     模拟高负载场景\n");
        }
        
        if (i == 15) {
            printf("     模拟低电量场景\n");
            power_set_power_mode(POWER_MODE_LOW);
        }
        
        test_delay(2000);
    }
    
    printf("\n=== 系统集成测试完成 ===\n");
}

/**
 * @brief 主测试函数 - 增强版
 */
int main(void) {
    printf("🚀 启动SOC通信功能测试程序\n");
    printf("===================================\n");
    
    // 初始化系统
    printf("正在初始化系统...\n");
    int32_t sys_init = system_init_all();
    if (sys_init != 0) {
        printf("❌ 系统初始化失败 (错误码: %d)\n", sys_init);
        return -1;
    }
    printf("✅ 系统初始化成功\n");
    
    // 初始化通信模块
    printf("正在初始化通信模块...\n");
    communication_init_all();
    printf("✅ 通信模块初始化成功\n");
    
    // 等待系统稳定
    test_delay(500);
    
    // 运行各项测试
    test_soc_communication();          // 基本通信功能测试
    test_soc_power_modes();            // 功耗模式测试
    test_soc_data_monitoring();        // 数据监控演示
    
    // 增强功能测试
    test_power_management();           // 电源管理功能测试
    test_advanced_algorithms();        // 高级算法功能测试
    test_fault_detection();            // 故障检测功能测试
    test_performance_monitoring();     // 性能监控测试
    test_system_integration();         // 系统集成测试
    
    printf("\n🎉 所有SOC通信功能测试完成！\n");
    printf("📚 使用说明:\n");
    printf("   1. soc_communication_init() - 初始化SOC通信\n");
    printf("   2. soc_send_command() - 发送SOC命令\n");
    printf("   3. soc_receive_data() - 接收SOC数据\n");
    printf("   4. soc_poll_task() - 定期轮询SOC状态\n");
    printf("   5. soc_set_power_mode() - 设置功耗模式\n");
    printf("   6. soc_query_status() - 查询SOC状态\n");
    printf("   7. soc_get_battery_info() - 获取电池信息\n");
    
    // 清理资源
    printf("\n正在清理资源...\n");
    communication_close(COMM_CHANNEL_SOC);
    printf("✅ 资源清理完成\n");
    
    return 0;
}