# UART通信协议文档

## 1. 协议概述

本文档定义了SOC与MCU之间的UART通信协议，用于实现按键/红外事件、温度湿度数据、固件版本信息和LED状态控制等功能的交互。

### 1.1 协议目的
- 标准化SOC与MCU之间的通信接口
- 确保数据传输的可靠性和完整性
- 支持多种功能的扩展和兼容

### 1.2 应用场景
- 按键/红外事件的采集和处理
- 温度湿度数据的获取
- 固件版本信息的查询
- LED状态的控制

## 2. 物理层

### 2.1 硬件参数
| 参数 | 配置值 | 说明 |
|------|--------|------|
| 设备路径 | /dev/ttyS0 | UART设备文件路径 |
| 波特率 | 9600 | 通信速率 |
| 数据位 | 8 | 每字节数据位数 |
| 停止位 | 1 | 每帧数据停止位数 |
| 校验位 | 0 | 无校验 |

### 2.2 电气特性
- 采用TTL电平标准
- 支持双向通信
- 建议使用屏蔽线缆减少干扰

## 3. 数据链路层

### 3.1 数据包格式

```
+----------+----------+----------+----------+----------+----------+
| 头部     | 命令码   | 数据长度 | 数据      | 校验和   | 尾部     |
+----------+----------+----------+----------+----------+----------+
| 1字节    | 1字节    | 1字节    | N字节     | 1字节    | 1字节    |
+----------+----------+----------+----------+----------+----------+
| 0xAA     | 命令码   | 0-64     | 数据内容  | 校验和   | 0x55     |
+----------+----------+----------+----------+----------+----------+
```

### 3.2 字段说明

| 字段 | 长度 | 说明 |
|------|------|------|
| 头部 | 1字节 | 固定值0xAA，标识数据包开始 |
| 命令码 | 1字节 | 定义操作类型，见应用层命令码定义 |
| 数据长度 | 1字节 | 数据字段的长度，范围0-64 |
| 数据 | N字节 | 具体的命令数据，长度由数据长度字段指定 |
| 校验和 | 1字节 | 从命令码开始到数据结束的累加和 |
| 尾部 | 1字节 | 固定值0x55，标识数据包结束 |

### 3.3 校验和计算

校验和为从命令码开始到数据结束的所有字节的累加和：

```c
uint8_t uart_calculate_checksum(uint8_t *data, int len) {
    uint8_t checksum = 0;
    for (int i = 0; i < len; i++) {
        checksum += data[i];
    }
    return checksum;
}
```

### 3.4 错误处理

| 错误码 | 值 | 说明 |
|--------|-----|------|
| UART_ERR_NONE | 0 | 无错误 |
| UART_ERR_TIMEOUT | 1 | 超时错误 |
| UART_ERR_CHECKSUM | 2 | 校验和错误 |
| UART_ERR_INVALID_PACKET | 3 | 无效数据包 |
| UART_ERR_UNKNOWN_CMD | 4 | 未知命令 |

## 4. 应用层

### 4.1 命令码定义

| 命令码 | 命令名称 | 方向 | 功能描述 |
|--------|----------|------|----------|
| 0x01 | CMD_QUERY_KEY_STATUS | SOC→MCU | 查询按键/红外状态 |
| 0x03 | CMD_KEY_STATUS_RESP | MCU→SOC | 按键/红外状态响应 |
| 0x02 | CMD_QUERY_TEMP_HUMID | SOC→MCU | 查询温度湿度 |
| 0x04 | CMD_TEMP_HUMID_RESP | MCU→SOC | 温度湿度响应 |
| 0x05 | CMD_QUERY_VERSION | SOC→MCU | 查询固件版本 |
| 0x06 | CMD_VERSION_RESP | MCU→SOC | 固件版本响应 |
| 0x07 | CMD_SET_LED_STATE | SOC→MCU | 设置LED状态 |
| 0x08 | CMD_SET_LED_RESP | MCU→SOC | LED状态设置响应 |

### 4.2 按键状态位定义

| 位 | 宏定义 | 说明 |
|----|--------|------|
| 0 | KEY_BIT_PLAY_PAUSE | 播放/暂停键 |
| 1 | KEY_BIT_VOL_UP | 音量+键 |
| 2 | KEY_BIT_VOL_DOWN | 音量-键 |
| 3 | KEY_BIT_SOURCE_SWITCH | 音源切换键 |
| 4 | KEY_BIT_SOUND_MODE | 音效模式键 |
| 5 | KEY_BIT_BASS_UP |  bass+键 |
| 6 | KEY_BIT_TREBLE_UP | 高音+键 |
| 7 | KEY_BIT_IR_LEARN | 红外学习键 |

### 4.3 LED状态定义

| 值 | 宏定义 | 说明 |
|-----|--------|------|
| 0 | LED_STATE_OFF | 关闭 |
| 1 | LED_STATE_ON | 打开 |
| 2 | LED_STATE_BLINK | 闪烁 |
| 3 | LED_STATE_BREATH | 呼吸灯 |

### 4.4 数据结构

#### 4.4.1 温度湿度数据

```c
typedef struct {
    int temperature;        // 温度值（摄氏度）
    int humidity;           // 湿度值（百分比）
} TempHumidData_t;
```

#### 4.4.2 固件版本数据

```c
typedef struct {
    int major;              // 主版本号
    int minor;              // 次版本号
    int patch;              // 补丁版本号
} VersionData_t;
```

#### 4.4.3 LED状态设置

```c
typedef struct {
    int led_idx;            // LED索引
    int led_state;          // LED状态
} LedState_t;
```

## 5. 通信流程

### 5.1 基本通信流程

1. SOC发送查询命令到MCU
2. MCU处理命令并准备响应数据
3. MCU发送响应命令到SOC
4. SOC接收并解析响应数据
5. SOC根据响应数据执行相应操作

### 5.2 按键/红外事件流程

1. MCU检测到按键/红外事件
2. MCU打包按键状态数据
3. MCU发送CMD_KEY_STATUS_RESP命令到SOC
4. SOC接收并解析按键状态
5. SOC将按键状态转换为内部事件
6. SOC处理按键/红外事件

### 5.3 温度湿度查询流程

1. SOC发送CMD_QUERY_TEMP_HUMID命令到MCU
2. MCU读取温度湿度传感器数据
3. MCU打包温度湿度数据
4. MCU发送CMD_TEMP_HUMID_RESP命令到SOC
5. SOC接收并解析温度湿度数据
6. SOC处理温度湿度数据（如显示或存储）

### 5.4 固件版本查询流程

1. SOC发送CMD_QUERY_VERSION命令到MCU
2. MCU读取固件版本信息
3. MCU打包固件版本数据
4. MCU发送CMD_VERSION_RESP命令到SOC
5. SOC接收并解析固件版本数据
6. SOC显示或记录固件版本信息

### 5.5 LED状态设置流程

1. SOC打包LED状态设置数据
2. SOC发送CMD_SET_LED_STATE命令到MCU
3. MCU接收并解析LED状态设置
4. MCU执行LED状态设置
5. MCU发送CMD_SET_LED_RESP命令到SOC
6. SOC接收并解析LED状态设置响应
7. SOC处理设置结果（如显示成功/失败）

## 6. 协议实现

### 6.1 数据包打包

```c
int uart_pack_data(uint8_t cmd, uint8_t *data, int data_len, uint8_t *packet, int *packet_len) {
    if (!packet || !packet_len || data_len > MAX_PACKET_LEN - 6) {
        return -1;
    }
    
    // 构建数据包
    int index = 0;
    packet[index++] = PACKET_HEADER;    // 头部
    packet[index++] = cmd;              // 命令码
    packet[index++] = (uint8_t)data_len;// 数据长度
    
    // 数据部分
    if (data_len > 0 && data) {
        memcpy(&packet[index], data, data_len);
        index += data_len;
    }
    
    // 校验和
    uint8_t checksum = uart_calculate_checksum(&packet[1], index - 1);
    packet[index++] = checksum;
    
    // 尾部
    packet[index++] = PACKET_TAIL;
    
    *packet_len = index;
    return 0;
}
```

### 6.2 数据包解析

```c
int uart_unpack_data(uint8_t *packet, int packet_len, uint8_t *cmd, uint8_t *data, int *data_len) {
    if (!packet || !cmd || !data_len) {
        return -1;
    }
    
    // 检查数据包长度
    if (packet_len < 5) {
        return UART_ERR_INVALID_PACKET;
    }
    
    // 检查头部和尾部
    if (packet[0] != PACKET_HEADER || packet[packet_len - 1] != PACKET_TAIL) {
        return UART_ERR_INVALID_PACKET;
    }
    
    // 提取命令码和数据长度
    *cmd = packet[1];
    uint8_t expected_data_len = packet[2];
    
    // 检查数据包长度是否匹配
    if (packet_len != expected_data_len + 5) {
        return UART_ERR_INVALID_PACKET;
    }
    
    // 验证校验和
    uint8_t expected_checksum = packet[packet_len - 2];
    uint8_t actual_checksum = uart_calculate_checksum(&packet[1], packet_len - 3);
    if (expected_checksum != actual_checksum) {
        return UART_ERR_CHECKSUM;
    }
    
    // 提取数据
    if (expected_data_len > 0 && data) {
        memcpy(data, &packet[3], expected_data_len);
    }
    *data_len = expected_data_len;
    
    return UART_ERR_NONE;
}
```

## 7. 错误处理机制

### 7.1 传输错误处理

| 错误类型 | 检测方式 | 处理方法 |
|----------|----------|----------|
| 超时错误 | 接收超时 | 重发命令或放弃本次操作 |
| 校验和错误 | 校验和不匹配 | 丢弃数据包，等待重发 |
| 无效数据包 | 头部/尾部不正确 | 丢弃数据包，等待重发 |
| 未知命令 | 命令码未定义 | 发送错误响应，忽略命令 |

### 7.2 应用错误处理

| 错误类型 | 处理方法 |
|----------|----------|
| 传感器错误 | 返回默认值或错误状态 |
| 执行失败 | 返回失败响应，包含错误原因 |
| 参数错误 | 返回参数错误响应 |

## 8. 协议扩展

### 8.1 命令码扩展

当需要添加新功能时，可以在现有命令码基础上扩展：

1. 命令码范围：0x09-0xFF
2. 响应码规则：命令码 + 0x02（保持与现有命令的响应码规则一致）
3. 数据结构：根据新功能定义相应的数据结构

### 8.2 数据长度扩展

当前协议支持最大64字节的数据长度，如需扩展：

1. 修改MAX_PACKET_LEN宏定义
2. 确保数据缓冲区足够大
3. 考虑传输时间和内存占用

### 8.3 功能扩展示例

#### 8.3.1 添加电机控制功能

| 命令码 | 命令名称 | 方向 | 功能描述 |
|--------|----------|------|----------|
| 0x09 | CMD_SET_MOTOR_STATE | SOC→MCU | 设置电机状态 |
| 0x0B | CMD_SET_MOTOR_RESP | MCU→SOC | 电机状态设置响应 |

#### 8.3.2 添加传感器数据采集功能

| 命令码 | 命令名称 | 方向 | 功能描述 |
|--------|----------|------|----------|
| 0x0A | CMD_QUERY_SENSOR_DATA | SOC→MCU | 查询传感器数据 |
| 0x0C | CMD_SENSOR_DATA_RESP | MCU→SOC | 传感器数据响应 |

## 9. 安全考虑

### 9.1 数据安全

- 校验和机制确保数据完整性
- 数据包格式验证防止恶意数据
- 命令码验证防止非法命令

### 9.2 通信安全

- 建议在关键命令中添加认证机制
- 考虑添加数据加密以提高安全性
- 实现命令速率限制防止DOS攻击

## 10. 测试建议

### 10.1 功能测试

- 按键/红外事件测试
- 温度湿度数据测试
- 固件版本查询测试
- LED状态控制测试

### 10.2 可靠性测试

- 通信中断恢复测试
- 噪声环境测试
- 长时间运行稳定性测试
- 边界条件测试

### 10.3 性能测试

- 命令响应时间测试
- 数据传输速率测试
- 并发命令处理测试

## 11. 协议版本管理

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| 1.0 | 2026-01-16 | 初始版本，定义基本通信协议 |
| 1.1 | YYYY-MM-DD | 添加电机控制功能 |
| 1.2 | YYYY-MM-DD | 添加传感器数据采集功能 |

## 12. 附录

### 12.1 术语表

| 术语 | 解释 |
|------|------|
| SOC | 系统级芯片，主控制器 |
| MCU | 微控制器，负责外设控制 |
| UART | 通用异步收发传输器 |
| CMD | 命令（Command） |
| RESP | 响应（Response） |
| TTL | 晶体管-晶体管逻辑电平 |

### 12.2 参考资料

- [UART通信原理](https://en.wikipedia.org/wiki/Universal_asynchronous_receiver-transmitter)
- [串行通信协议设计指南](https://www.ti.com/lit/an/slaa704/slaa704.pdf)
- [嵌入式系统通信协议设计](https://www.embedded.com/serial-communication-protocols-for-embedded-systems/)

### 12.3 联系信息

| 角色 | 联系方式 |
|------|----------|
| 协议设计 | SOC团队 |
| 实现团队 | MCU团队 |
| 维护团队 | 系统集成团队 |
