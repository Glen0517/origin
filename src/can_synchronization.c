/*
 * can_synchronization.c
 * 节点间数据同步和错误处理机制实现
 */

#include "DSP2833x_Device.h"     // Device Header Include File
#include "DSP2833x_Examples.h"
#include "CanBus.h"
#include "timer.h"

// 错误代码定义
#define ERROR_NONE               0x0000
#define ERROR_COMM_TIMEOUT       0x0001
#define ERROR_MESSAGE_CORRUPT    0x0002
#define ERROR_BUS_OFF            0x0004
#define ERROR_BUS_WARNING        0x0008
#define ERROR_NODE_TIMEOUT       0x0010
#define ERROR_DATA_INCONSISTENT  0x0020
#define ERROR_MEMORY_ALLOCATION  0x0040
#define ERROR_CONFIG_INVALID     0x0080

// 节点状态定义
#define NODE_STATE_UNKNOWN       0x00
#define NODE_STATE_ONLINE        0x01
#define NODE_STATE_DEGRADED      0x02
#define NODE_STATE_OFFLINE       0x03
#define NODE_STATE_ERROR         0x04
#define NODE_STATE_RECOVERING    0x05

// 同步参数定义
#define MAX_SYNC_OFFSET          100     // 最大允许的时间同步偏移 (ms)
#define HEARTBEAT_TIMEOUT        3000    // 心跳超时时间 (ms)
#define SYNC_MESSAGE_INTERVAL    1000    // 同步消息发送间隔 (ms)
#define RETRY_COUNT              3       // 消息发送重试次数
#define AUTO_RECOVER_ATTEMPTS    5       // 自动恢复尝试次数

// 网络诊断参数
#define MAX_BUS_LOAD             80      // 最大允许总线负载 (%)
#define MAX_ERROR_FRAMES         10      // 最大允许错误帧数
#define MAX_LATENCY              5       // 最大允许通信延迟 (ms)

// 节点诊断信息结构
typedef struct {
    Uint32 node_id;
    Uint8 node_state;
    Uint16 error_code;
    Uint32 last_heartbeat_time;
    Uint32 message_count;
    Uint32 error_count;
    Uint32 recovery_attempts;
    Uint32 last_sync_time;
    Uint32 time_offset;
} NODE_DIAGNOSTICS;

// 网络状态信息结构
typedef struct {
    Uint32 bus_load_percent;
    Uint32 total_messages;
    Uint32 total_errors;
    Uint32 total_retransmissions;
    Uint32 max_latency;
    Uint32 avg_latency;
    Uint32 sync_master_id;
    Uint8 active_nodes_count;
    Uint8 network_health;
} NETWORK_STATUS;

// 数据一致性检查结构
typedef struct {
    Uint32 message_id;
    Uint8 data_validity_flags;
    Uint16 data_timestamp;
    Uint8 data_version;
    Uint8 checksum;
} DATA_CONSISTENCY;

// 全局变量
NODE_DIAGNOSTICS node_diagnostics[32]; // 最多支持32个节点的诊断信息
NETWORK_STATUS network_status;
DATA_CONSISTENCY data_consistency[64]; // 最多支持64种消息类型的数据一致性检查
Uint32 system_time = 0; // 系统当前时间 (ms)
Uint32 sync_counter = 0; // 同步计数器
Uint8 is_sync_master = 0; // 是否为同步主节点
Uint32 last_sync_broadcast = 0; // 上次广播同步消息的时间

// 初始化诊断系统
void init_diagnostics_system(void) {
    // 初始化所有节点诊断信息
    for (int i = 0; i < 32; i++) {
        node_diagnostics[i].node_id = 0;
        node_diagnostics[i].node_state = NODE_STATE_UNKNOWN;
        node_diagnostics[i].error_code = ERROR_NONE;
        node_diagnostics[i].last_heartbeat_time = 0;
        node_diagnostics[i].message_count = 0;
        node_diagnostics[i].error_count = 0;
        node_diagnostics[i].recovery_attempts = 0;
        node_diagnostics[i].last_sync_time = 0;
        node_diagnostics[i].time_offset = 0;
    }
    
    // 初始化网络状态
    network_status.bus_load_percent = 0;
    network_status.total_messages = 0;
    network_status.total_errors = 0;
    network_status.total_retransmissions = 0;
    network_status.max_latency = 0;
    network_status.avg_latency = 0;
    network_status.sync_master_id = 0;
    network_status.active_nodes_count = 0;
    network_status.network_health = 100;
    
    // 初始化数据一致性检查
    for (int i = 0; i < 64; i++) {
        data_consistency[i].message_id = 0;
        data_consistency[i].data_validity_flags = 0;
        data_consistency[i].data_timestamp = 0;
        data_consistency[i].data_version = 0;
        data_consistency[i].checksum = 0;
    }
    
    // 默认设置为同步主节点（实际应用中应通过优先级选举决定）
    is_sync_master = 1;
    network_status.sync_master_id = BIC_ID; // 假设当前节点是双向转换器
    
    system_time = 0;
    sync_counter = 0;
    last_sync_broadcast = 0;
}

// 更新系统时间（在定时器中断中调用）
void update_system_time(void) {
    system_time++;
    
    // 如果是同步主节点，定期广播同步消息
    if (is_sync_master && (system_time - last_sync_broadcast >= SYNC_MESSAGE_INTERVAL)) {
        broadcast_sync_message();
        last_sync_broadcast = system_time;
    }
    
    // 定期检查节点健康状态
    if (system_time % 500 == 0) { // 每500ms检查一次
        check_nodes_health();
    }
    
    // 更新网络状态信息
    if (system_time % 1000 == 0) { // 每秒更新一次
        update_network_status();
    }
}

// 广播同步消息（由同步主节点调用）
void broadcast_sync_message(void) {
    struct CAN_DATA sync_msg;
    sync_msg.id = 0x95550000; // 同步消息专用ID
    sync_msg.data0 = (Uint16)(system_time & 0xFFFF); // 低16位时间
    sync_msg.data1 = (Uint16)((system_time >> 16) & 0xFFFF); // 高16位时间
    sync_msg.data2 = sync_counter++; // 同步计数器
    sync_msg.index = 0xFF; // 同步消息索引
    
    // 使用优先级最高的邮箱发送同步消息
    send_data_with_retry(0, sync_msg, RETRY_COUNT);
    
    // 更新网络状态
    network_status.total_messages++;
}

// 处理接收到的同步消息
void process_sync_message(struct CAN_DATA sync_msg) {
    // 计算接收到的系统时间
    Uint32 received_time = ((Uint32)sync_msg.data1 << 16) | sync_msg.data0;
    
    // 如果是同步主节点，则忽略其他同步消息
    if (is_sync_master) {
        return;
    }
    
    // 计算时间偏移
    Uint32 time_offset = (received_time > system_time) ? 
                         (received_time - system_time) : 
                         (system_time - received_time);
    
    // 如果偏移过大，进行时间校准
    if (time_offset > MAX_SYNC_OFFSET) {
        system_time = received_time; // 直接校准
    } else {
        // 渐进式校准，避免时间跳变
        if (received_time > system_time) {
            system_time += 1; // 每次增加1ms
        }
    }
    
    // 更新同步状态
    Uint8 node_index = get_node_index_by_id(sync_msg.id);
    if (node_index < 32) {
        node_diagnostics[node_index].last_sync_time = system_time;
        node_diagnostics[node_index].time_offset = time_offset;
    }
}

// 获取节点索引（根据节点ID）
Uint8 get_node_index_by_id(Uint32 node_id) {
    for (int i = 0; i < 32; i++) {
        if (node_diagnostics[i].node_id == node_id) {
            return i;
        }
    }
    
    // 如果找不到，尝试分配一个新的索引
    for (int i = 0; i < 32; i++) {
        if (node_diagnostics[i].node_id == 0) {
            node_diagnostics[i].node_id = node_id;
            return i;
        }
    }
    
    return 32; // 索引超出范围
}

// 处理心跳消息，更新节点状态
void update_node_heartbeat(Uint32 node_id) {
    Uint8 node_index = get_node_index_by_id(node_id);
    if (node_index < 32) {
        // 更新节点心跳时间
        node_diagnostics[node_index].last_heartbeat_time = system_time;
        
        // 如果节点之前是离线或错误状态，现在恢复在线
        if (node_diagnostics[node_index].node_state == NODE_STATE_OFFLINE || 
            node_diagnostics[node_index].node_state == NODE_STATE_ERROR) {
            node_diagnostics[node_index].node_state = NODE_STATE_ONLINE;
            node_diagnostics[node_index].error_code = ERROR_NONE;
            node_diagnostics[node_index].recovery_attempts = 0;
            
            // 发送节点恢复通知
            send_node_status_notification(node_id, NODE_STATE_ONLINE);
        }
        
        // 增加消息计数
        node_diagnostics[node_index].message_count++;
    }
}

// 检查所有节点健康状态
void check_nodes_health(void) {
    Uint8 active_count = 0;
    Uint8 error_count = 0;
    
    for (int i = 0; i < 32; i++) {
        if (node_diagnostics[i].node_id == 0) continue; // 跳过未使用的节点
        
        // 检查心跳超时
        Uint32 time_since_heartbeat = system_time - node_diagnostics[i].last_heartbeat_time;
        
        if (time_since_heartbeat > HEARTBEAT_TIMEOUT) {
            // 节点心跳超时
            if (node_diagnostics[i].node_state != NODE_STATE_OFFLINE) {
                node_diagnostics[i].node_state = NODE_STATE_OFFLINE;
                node_diagnostics[i].error_code |= ERROR_NODE_TIMEOUT;
                error_count++;
                
                // 发送节点离线通知
                send_node_status_notification(node_diagnostics[i].node_id, NODE_STATE_OFFLINE);
                
                // 尝试自动恢复
                attempt_node_recovery(i);
            }
        } else {
            active_count++;
        }
    }
    
    // 更新网络健康状态
    network_status.active_nodes_count = active_count;
    
    // 简单计算网络健康度
    if (network_status.active_nodes_count > 0) {
        network_status.network_health = (active_count * 100) / 32;
    } else {
        network_status.network_health = 0;
    }
    
    // 如果网络健康度低于阈值，尝试网络诊断
    if (network_status.network_health < 50) {
        perform_network_diagnostics();
    }
}

// 尝试恢复离线节点
void attempt_node_recovery(Uint8 node_index) {
    // 限制恢复尝试次数
    if (node_diagnostics[node_index].recovery_attempts >= AUTO_RECOVER_ATTEMPTS) {
        return;
    }
    
    // 设置节点为恢复状态
    node_diagnostics[node_index].node_state = NODE_STATE_RECOVERING;
    node_diagnostics[node_index].recovery_attempts++;
    
    // 发送恢复请求消息
    struct CAN_DATA recover_msg;
    recover_msg.id = 0x95550001; // 恢复请求专用ID
    recover_msg.data0 = (Uint16)(node_diagnostics[node_index].node_id & 0xFFFF);
    recover_msg.data1 = (Uint16)((node_diagnostics[node_index].node_id >> 16) & 0xFFFF);
    recover_msg.data2 = node_diagnostics[node_index].recovery_attempts;
    recover_msg.index = 0xFE; // 恢复请求索引
    
    send_data_with_retry(1, recover_msg, RETRY_COUNT);
}

// 发送节点状态通知
void send_node_status_notification(Uint32 node_id, Uint8 node_state) {
    struct CAN_DATA status_msg;
    status_msg.id = 0x95550002; // 状态通知专用ID
    status_msg.data0 = (Uint16)(node_id & 0xFFFF);
    status_msg.data1 = (Uint16)((node_id >> 16) & 0xFFFF);
    status_msg.data2 = node_state;
    status_msg.index = 0xFD; // 状态通知索引
    
    send_data_with_retry(2, status_msg, 1);
}

// 执行网络诊断
void perform_network_diagnostics(void) {
    // 这里可以实现更复杂的网络诊断逻辑
    // 例如：测试网络负载、检测错误帧、测量通信延迟等
    
    // 发送网络诊断请求
    struct CAN_DATA diag_msg;
    diag_msg.id = 0x95550003; // 网络诊断专用ID
    diag_msg.data0 = network_status.bus_load_percent;
    diag_msg.data1 = network_status.total_errors;
    diag_msg.data2 = network_status.network_health;
    diag_msg.index = 0xFC; // 网络诊断索引
    
    send_data_with_retry(3, diag_msg, 1);
    
    // 检查CAN控制器错误状态
    check_can_controller_status();
}

// 检查CAN控制器状态
void check_can_controller_status(void) {
    // 读取CAN错误计数和状态
    EALLOW;
    Uint16 error_count = ECanbRegs.CANES.all;
    EDIS;
    
    // 检查总线错误状态
    if (error_count & 0x0001) { // 检查错误主动标志
        network_status.total_errors++;
        
        // 发送总线警告
        struct CAN_DATA error_msg;
        error_msg.id = 0x95550004; // 错误通知专用ID
        error_msg.data0 = ERROR_BUS_WARNING;
        error_msg.data1 = error_count;
        error_msg.data2 = system_time;
        error_msg.index = 0xFB; // 错误通知索引
        
        send_data_with_retry(4, error_msg, 1);
    }
}

// 更新网络状态信息
void update_network_status(void) {
    // 简单估算总线负载（实际应用中应根据实际发送/接收的位数量计算）
    // 这里只是一个模拟示例
    static Uint32 last_message_count = 0;
    Uint32 messages_in_interval = network_status.total_messages - last_message_count;
    last_message_count = network_status.total_messages;
    
    // 假设每秒最多可以传输1000条消息，计算百分比
    network_status.bus_load_percent = (messages_in_interval * 100) / 1000;
    
    // 限制在合理范围内
    if (network_status.bus_load_percent > 100) {
        network_status.bus_load_percent = 100;
    }
    
    // 如果总线负载过高，发送警告
    if (network_status.bus_load_percent > MAX_BUS_LOAD) {
        struct CAN_DATA warning_msg;
        warning_msg.id = 0x95550005; // 警告通知专用ID
        warning_msg.data0 = 0x01; // 总线负载过高
        warning_msg.data1 = network_status.bus_load_percent;
        warning_msg.data2 = system_time;
        warning_msg.index = 0xFA; // 警告通知索引
        
        send_data_with_retry(5, warning_msg, 1);
    }
}

// 计算数据校验和（用于数据一致性检查）
Uint8 calculate_checksum(struct CAN_DATA data) {
    Uint8 checksum = 0;
    
    // 对所有数据字段计算简单的异或校验
    checksum ^= (Uint8)((data.data0 >> 8) & 0xFF);
    checksum ^= (Uint8)(data.data0 & 0xFF);
    checksum ^= (Uint8)((data.data1 >> 8) & 0xFF);
    checksum ^= (Uint8)(data.data1 & 0xFF);
    checksum ^= (Uint8)((data.data2 >> 8) & 0xFF);
    checksum ^= (Uint8)(data.data2 & 0xFF);
    checksum ^= (Uint8)data.index;
    
    return checksum;
}

// 数据一致性检查
Uint8 check_data_consistency(struct CAN_DATA data) {
    // 查找该消息ID的一致性记录
    for (int i = 0; i < 64; i++) {
        if (data_consistency[i].message_id == data.id) {
            // 计算当前数据的校验和
            Uint8 current_checksum = calculate_checksum(data);
            
            // 检查数据版本是否更新
            if (data_consistency[i].data_version >= data_consistency[i].data_version) {
                // 数据可能重复或过期
                return 0;
            }
            
            // 更新一致性记录
            data_consistency[i].data_validity_flags = 0x01; // 数据有效
            data_consistency[i].data_timestamp = (Uint16)(system_time & 0xFFFF);
            data_consistency[i].data_version++;
            data_consistency[i].checksum = current_checksum;
            
            return 1;
        }
    }
    
    // 找不到记录，创建新的一致性记录
    for (int i = 0; i < 64; i++) {
        if (data_consistency[i].message_id == 0) {
            data_consistency[i].message_id = data.id;
            data_consistency[i].data_validity_flags = 0x01; // 数据有效
            data_consistency[i].data_timestamp = (Uint16)(system_time & 0xFFFF);
            data_consistency[i].data_version = 1;
            data_consistency[i].checksum = calculate_checksum(data);
            
            return 1;
        }
    }
    
    return 0; // 无法创建新记录
}

// 带重试机制的消息发送函数
Uint8 send_data_with_retry(int16 MBXnbr, struct CAN_DATA data, Uint8 retry_count) {
    Uint8 success = 0;
    Uint8 attempts = 0;
    
    while (!success && attempts <= retry_count) {
        // 尝试发送数据
        volatile struct MBOX *Mailbox;
        Mailbox = &ECanbMboxes.MBOX0 + MBXnbr;
        
        Mailbox->MDL.word.HI_WORD = data.data0;
        Mailbox->MDL.word.LOW_WORD = data.data1;
        Mailbox->MDH.word.HI_WORD = data.data2;
        Mailbox->MDH.byte.BYTE6 = data.index;
        
        // 设置发送请求
        ECanbRegs.CANTRS.all = 0x1 << MBXnbr;
        
        // 等待发送完成（带超时）
        Uint32 timeout = 1000; // 1ms超时
        Uint32 start_time = system_time;
        
        while ((ECanbRegs.CANTA.all & (0x1 << MBXnbr)) == 0) {
            if ((system_time - start_time) > timeout) {
                break; // 超时退出
            }
        }
        
        // 检查是否发送成功
        if (ECanbRegs.CANTA.all & (0x1 << MBXnbr)) {
            ECanbRegs.CANTA.all = 0x1 << MBXnbr; // 清除发送确认标志
            success = 1;
            
            // 更新网络状态统计
            network_status.total_messages++;
            if (attempts > 0) {
                network_status.total_retransmissions++;
            }
        } else {
            attempts++;
            network_status.total_errors++;
            
            // 简单延时后重试
            DELAY_US(100);
        }
    }
    
    return success;
}

// 错误处理函数
void handle_can_error(Uint16 error_code, Uint32 message_id) {
    // 记录错误
    network_status.total_errors++;
    
    // 发送错误报告
    struct CAN_DATA error_msg;
    error_msg.id = 0x95550006; // 错误报告专用ID
    error_msg.data0 = error_code;
    error_msg.data1 = (Uint16)(message_id & 0xFFFF);
    error_msg.data2 = (Uint16)((message_id >> 16) & 0xFFFF);
    error_msg.index = 0xF9; // 错误报告索引
    
    send_data_with_retry(6, error_msg, 1);
    
    // 根据错误类型执行不同的恢复操作
    switch (error_code) {
        case ERROR_BUS_OFF:
            // 尝试恢复总线
            recover_can_bus();
            break;
            
        case ERROR_MESSAGE_CORRUPT:
            // 记录并忽略损坏的消息
            break;
            
        case ERROR_NODE_TIMEOUT:
            // 已在节点健康检查中处理
            break;
    }
}

// 尝试恢复CAN总线
void recover_can_bus(void) {
    EALLOW;
    
    // 重置CAN控制器
    ECanbShadow.CANMC.all = ECanbRegs.CANMC.all;
    ECanbShadow.CANMC.bit.SCB = 1; // Set Software Clear Bus-Off
    ECanbRegs.CANMC.all = ECanbShadow.CANMC.all;
    
    // 等待总线恢复
    DELAY_US(100000); // 100ms
    
    // 重新使能CAN
    ECanbShadow.CANMC.bit.SCB = 0;
    ECanbRegs.CANMC.all = ECanbShadow.CANMC.all;
    
    EDIS;
    
    // 发送总线恢复通知
    struct CAN_DATA recover_msg;
    recover_msg.id = 0x95550007; // 总线恢复通知专用ID
    recover_msg.data0 = 0x01; // 恢复成功
    recover_msg.data1 = system_time;
    recover_msg.data2 = 0;
    recover_msg.index = 0xF8; // 总线恢复索引
    
    send_data_with_retry(7, recover_msg, 1);
}

// 获取网络健康报告
NETWORK_STATUS get_network_health_report(void) {
    return network_status;
}

// 获取特定节点的诊断信息
NODE_DIAGNOSTICS get_node_diagnostics(Uint32 node_id) {
    Uint8 node_index = get_node_index_by_id(node_id);
    if (node_index < 32) {
        return node_diagnostics[node_index];
    }
    
    // 返回空的诊断信息
    NODE_DIAGNOSTICS empty_diag;
    memset(&empty_diag, 0, sizeof(NODE_DIAGNOSTICS));
    return empty_diag;
}