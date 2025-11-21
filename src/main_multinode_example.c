/*
 * main_multinode_example.c
 * 多节点CAN通信示例实现
 * 模拟在单个DSP上运行多个逻辑CAN节点
 */

#include "DSP2833x_Device.h"     // Device Header Include File
#include "DSP2833x_Examples.h"
#include "GPIO_Driver.h"
#include "usDelay.h"
#include "CanBus.h"
#include "timer.h"

// 节点运行状态定义
typedef struct {
    Uint8 node_enabled;
    Uint32 last_send_time;
    Uint16 heartbeat_count;
} NODE_STATUS;

// 节点状态数组
NODE_STATUS node_status[11]; // 对应11个预定义节点

// 模拟传感器数据
typedef struct {
    int16 temperature;
    int16 humidity;
    int16 light_level;
    int16 voltage;
    int16 current;
    int16 power;
} SENSOR_DATA;

// 模拟执行器状态
typedef struct {
    Uint8 relay_states;
    int16 motor_position;
    int16 motor_speed;
    Uint8 alarm_level;
} ACTUATOR_STATUS;

// 全局数据结构
SENSOR_DATA sensor_data = {0};
ACTUATOR_STATUS actuator_status = {0};

// 系统时间计数器
Uint32 system_counter = 0;

// 模拟随机数据变化
int16 random_variation(int16 base_value, int16 range) {
    return base_value + ((rand() % (2 * range)) - range);
}

// 初始化所有节点状态
void init_node_status(void) {
    for (int i = 0; i < 11; i++) {
        node_status[i].node_enabled = (i <= 5) ? 1 : 0; // 默认启用前6个原始节点
        node_status[i].last_send_time = 0;
        node_status[i].heartbeat_count = 0;
    }
    
    // 启用模拟的新节点
    node_status[TEMP_SENSOR_ID_INDEX].node_enabled = 1; // 温度传感器节点
    node_status[POWER_MONITOR_ID_INDEX].node_enabled = 1; // 电力监测节点
    node_status[MOTOR_CONTROL_ID_INDEX].node_enabled = 1; // 电机控制节点
}

// 发送心跳消息
void send_heartbeat(Uint32 node_id_index) {
    struct CAN_DATA heartbeat_msg;
    heartbeat_msg.id = (node_id_index == 0) ? BBB_ID : 
                      (node_id_index == 1) ? RES_ID :
                      (node_id_index == 2) ? BIC_ID :
                      (node_id_index == 3) ? PV_ID :
                      (node_id_index == 4) ? BAT_ID :
                      (node_id_index == 5) ? WIN_ID :
                      (node_id_index == 6) ? 0x9555AAA6 : // 温度传感器
                      (node_id_index == 7) ? 0x9555AAA7 : // 电力监测
                      (node_id_index == 8) ? 0x9555AAAB : // 电机控制
                      (node_id_index == 9) ? 0x9555AAAC : // 继电器控制
                      0x9555AAAE;                          // 报警节点
    
    heartbeat_msg.data0 = 0x01; // 健康状态码
    heartbeat_msg.data1 = 0x00; // 工作模式
    heartbeat_msg.data2 = node_status[node_id_index].heartbeat_count++;
    heartbeat_msg.index = HEART_BEAT_INDEX;
    
    send_data(node_id_index, heartbeat_msg);
    node_status[node_id_index].last_send_time = system_counter;
}

// 模拟温度传感器数据发送
void send_temperature_data(void) {
    // 模拟温度变化
    sensor_data.temperature = random_variation(250, 10); // 25.0°C ± 1.0°C
    
    struct CAN_DATA temp_msg;
    temp_msg.id = 0x9555AAA6; // 温度传感器节点ID
    temp_msg.data0 = (sensor_data.temperature << 4) | 0x01; // 传感器0的数据
    temp_msg.data1 = sensor_data.temperature;
    temp_msg.data2 = system_counter;
    temp_msg.index = TEMP_DATA_INDEX;
    
    send_data(6, temp_msg); // 使用邮箱6发送
    
    // 模拟多个传感器
    for (int i = 1; i < 4; i++) {
        int16 sensor_temp = random_variation(250 + i*10, 5); // 每个传感器温度略有不同
        struct CAN_DATA sensor_msg;
        sensor_msg.id = 0x9555AAA6;
        sensor_msg.data0 = (sensor_temp << 4) | (i+1); // 不同传感器ID
        sensor_msg.data1 = sensor_temp;
        sensor_msg.data2 = system_counter;
        sensor_msg.index = TEMP_DATA_INDEX;
        
        send_data(6 + i, sensor_msg); // 使用不同邮箱发送
    }
    
    node_status[6].last_send_time = system_counter;
}

// 模拟电流电压监测数据发送
void send_power_monitor_data(void) {
    // 模拟电力数据
    sensor_data.voltage = random_variation(23000, 1000); // 230.0V ± 10.0V
    sensor_data.current = random_variation(5000, 500);   // 5.0A ± 0.5A
    sensor_data.power = (sensor_data.voltage * sensor_data.current) / 10000; // 计算功率
    
    struct CAN_DATA power_msg;
    power_msg.id = 0x9555AAA7; // 电力监测节点ID
    power_msg.data0 = sensor_data.voltage;     // 电压值 (0.01V)
    power_msg.data1 = sensor_data.current;     // 电流值 (0.001A)
    power_msg.data2 = sensor_data.power;       // 功率值 (0.1W)
    power_msg.index = POWER_DATA_INDEX;
    
    send_data(7, power_msg); // 使用邮箱7发送
    node_status[7].last_send_time = system_counter;
}

// 模拟电机控制命令处理
void process_motor_command(struct CAN_DATA cmd) {
    switch(cmd.data0) {
        case 0x01: // 位置控制
            actuator_status.motor_position = cmd.data1;
            break;
        case 0x02: // 速度控制
            actuator_status.motor_speed = cmd.data1;
            break;
        case 0x03: // 停止电机
            actuator_status.motor_position = 0;
            actuator_status.motor_speed = 0;
            break;
    }
    
    // 发送电机状态反馈
    struct CAN_DATA motor_status;
    motor_status.id = 0x9555AAAB;
    motor_status.data0 = actuator_status.motor_position;
    motor_status.data1 = actuator_status.motor_speed;
    motor_status.data2 = 0; // 模拟电流
    motor_status.index = MOTOR_STATUS_INDEX;
    
    send_data(8, motor_status); // 使用邮箱8发送
}

// 模拟继电器控制命令处理
void process_relay_command(struct CAN_DATA cmd) {
    Uint8 relay_mask = (Uint8)(cmd.data0 & 0xFF);
    Uint8 action = (Uint8)(cmd.data1 & 0xFF);
    
    switch(action) {
        case 0x01: // 打开继电器
            actuator_status.relay_states |= relay_mask;
            break;
        case 0x02: // 关闭继电器
            actuator_status.relay_states &= ~relay_mask;
            break;
        case 0x03: // 切换继电器
            actuator_status.relay_states ^= relay_mask;
            break;
    }
    
    // 发送继电器状态
    struct CAN_DATA relay_status;
    relay_status.id = 0x9555AAAC;
    relay_status.data0 = actuator_status.relay_states;
    relay_status.data1 = 0; // 无错误
    relay_status.data2 = 0xAA; // 电源状态正常
    relay_status.index = RELAY_STATUS_INDEX;
    
    send_data(9, relay_status); // 使用邮箱9发送
}

// 模拟报警处理
void process_alarm_command(struct CAN_DATA cmd) {
    Uint8 alarm_level = (Uint8)(cmd.data0 & 0xFF);
    Uint8 alarm_type = (Uint8)(cmd.data1 & 0xFF);
    
    // 记录报警级别
    actuator_status.alarm_level = alarm_level;
    
    // 闪烁LED模拟报警
    if (alarm_level > 0) {
        for (int i = 0; i < alarm_level; i++) {
            BLINK_LED();
            DELAY_US(200000);
        }
    }
    
    // 发送报警确认
    struct CAN_DATA alarm_ack;
    alarm_ack.id = 0x9555AAAE;
    alarm_ack.data0 = alarm_level;
    alarm_ack.data1 = alarm_type;
    alarm_ack.data2 = system_counter;
    alarm_ack.index = ALARM_ACK_INDEX;
    
    send_data(10, alarm_ack); // 使用邮箱10发送
}

// 接收消息处理函数
void process_received_message(struct CAN_DATA received) {
    // 根据消息ID确定节点类型
    Uint32 node_base_id = received.id & 0xFFFFFFF0; // 提取基础节点ID
    
    switch (node_base_id) {
        case 0x9555AAA0: // BBB节点
            // 处理BBB节点消息
            break;
            
        case 0x9555AAA1: // 资源管理器
            // 处理资源管理器消息
            break;
            
        case 0x9555AAA2: // 双向转换器
            // 处理双向转换器消息
            break;
            
        case 0x9555AAA6: // 温度传感器
            // 处理温度传感器数据
            if (received.data1 > 300) { // 如果温度超过30°C，触发报警
                struct CAN_DATA alarm_cmd;
                alarm_cmd.data0 = 0x02; // 警告级别
                alarm_cmd.data1 = 0x01; // 温度过高
                process_alarm_command(alarm_cmd);
            }
            break;
            
        case 0x9555AAA7: // 电力监测
            // 处理电力监测数据
            if (received.data0 > 25000) { // 过压警告
                struct CAN_DATA alarm_cmd;
                alarm_cmd.data0 = 0x02; // 警告级别
                alarm_cmd.data1 = 0x02; // 电压过高
                process_alarm_command(alarm_cmd);
            }
            break;
            
        case 0x00000000: // 通用命令
            // 根据索引处理不同命令
            switch (received.index) {
                case 0x80: // 电机控制命令
                    process_motor_command(received);
                    break;
                    
                case 0x81: // 继电器控制命令
                    process_relay_command(received);
                    break;
                    
                case 0x82: // 报警控制命令
                    process_alarm_command(received);
                    break;
            }
            break;
    }
}

void main(void) {
    // 1. 系统初始化
    InitSysCtrl();
    InitPieCtrl();
    
    // 2. 中断配置
    DINT;
    IER = 0x0000;
    IFR = 0x0000;
    
    // 3. 内存和Flash初始化
    MemCopy(&RamfuncsLoadStart, &RamfuncsLoadEnd, &RamfuncsRunStart);
    InitFlash();
    InitPieVectTable();
    
    // 4. 启用全局中断
    EINT;
    ERTM;
    
    // 5. 用户模块初始化
    configureLED();
    configureTimer0();
    configureEcanB();
    
    // 6. 初始化节点状态
    init_node_status();
    
    // 7. 主循环
    Uint32 last_heartbeat_time = 0;
    Uint32 last_sensor_update = 0;
    Uint32 last_power_update = 0;
    
    for (;;) {
        // 系统时钟计数
        system_counter++;
        
        // 每秒发送心跳消息
        if (system_counter - last_heartbeat_time >= 10) { // 假设循环每秒执行10次
            for (int i = 0; i < 11; i++) {
                if (node_status[i].node_enabled) {
                    send_heartbeat(i);
                }
            }
            last_heartbeat_time = system_counter;
        }
        
        // 每0.5秒更新传感器数据
        if (system_counter - last_sensor_update >= 5) {
            if (node_status[6].node_enabled) { // 温度传感器节点
                send_temperature_data();
            }
            last_sensor_update = system_counter;
        }
        
        // 每0.2秒更新电力监测数据
        if (system_counter - last_power_update >= 2) {
            if (node_status[7].node_enabled) { // 电力监测节点
                send_power_monitor_data();
            }
            last_power_update = system_counter;
        }
        
        // 模拟发送电机控制命令
        if (system_counter % 20 == 0) { // 每2秒发送一次
            struct CAN_DATA motor_cmd;
            motor_cmd.id = 0x00000000;
            motor_cmd.data0 = 0x01; // 位置控制
            motor_cmd.data1 = (system_counter % 100) * 10; // 0-990的位置值
            motor_cmd.data2 = 0;
            motor_cmd.index = 0x80; // 电机控制命令索引
            process_motor_command(motor_cmd);
        }
        
        // 模拟发送继电器控制命令
        if (system_counter % 30 == 0) { // 每3秒切换一次
            struct CAN_DATA relay_cmd;
            relay_cmd.id = 0x00000000;
            relay_cmd.data0 = 0x01; // 继电器1
            relay_cmd.data1 = 0x03; // 切换动作
            relay_cmd.data2 = 0;
            relay_cmd.index = 0x81; // 继电器控制命令索引
            process_relay_command(relay_cmd);
        }
        
        // 接收数据处理
        if (new_data) { // 检查是否有新数据
            process_received_message(can_data);
            new_data = FALSE;
        }
        
        // 延时保持系统稳定
        DELAY_US(100000); // 100ms延时
    }
}
