#include "usart.h"
//#include "stm32_driver.h"需要包含官方驱动头文件
#if SYSTEM_SUPPORT_OS
#include "FreeRTOS.h"      //os ʹ��	  
#endif

#pragma import(__use_no_semihosting)      

#define UART_SWITCH 0

//初始化发送缓冲区
TX_Buffer_Struct Tx_Buffer_t ={
	.tx_buffer = {0},
	.tx_head = 0,
	.tx_tail = 0,
};

//初始化接收缓冲区
RX_Buffer_Struct Rx_Buffer_t ={
	.rx_buffer = {0},
	.rx_head = 0,
	.rx_tail =0,
};

/*
exmaple:uart_send("ABC", 3);

发送流程：
数据复制到缓冲区：tx_buffer[0]='A', tx_buffer[1]='B', tx_buffer[2]='C'
tx_head 移动到 3（0 → 1 → 2 → 3）
触发中断后，ISR 从 tx_tail=0 开始发送数据：
发送 tx_buffer[0]='A'，tx_tail 移动到 1
发送 tx_buffer[1]='B'，tx_tail 移动到 2
发送 tx_buffer[2]='C'，tx_tail 移动到 3
此时 tx_head = tx_tail = 3，缓冲区为空，ISR 关闭中断。

缓冲区状态变化：
初始：tx_head=0, tx_tail=0  →  缓冲区空
写入后：tx_head=3, tx_tail=0  →  缓冲区有3个数据
发送后：tx_head=3, tx_tail=3  →  缓冲区空
*/
// 非阻塞式UART发送（放入缓冲区，由中断发送）
bool uart_send(uint8_t *data, uint32_t length) {
    // 检查缓冲区是否有足够空间
    uint32_t space_num = (Tx_Buffer_t.tx_tail > Tx_Buffer_t.tx_head) ? 
                     (Tx_Buffer_t.tx_tail - Tx_Buffer_t.tx_head - 1) : 
                     (TX_BUFFER_SIZE - Tx_Buffer_t.tx_head + Tx_Buffer_t.tx_tail - 1);
    
    if (space_num < length) {
		//printf("UART Error : Buffer full!\n");
		return false; // 缓冲区已满
	}

    // 将数据复制到发送缓冲区
    for (uint32_t i = 0; i < length; i++) {
        Tx_Buffer_t.tx_buffer[Tx_Buffer_t.tx_head] = data[i];
        Tx_Buffer_t.tx_head = (Tx_Buffer_t.tx_head + 1) % TX_BUFFER_SIZE;
    }
    
    // 使能发送中断（触发第一次发送）,开关为关闭
	#if UART_SWITCH
    UARTx->CR1 |= UART_CR1_TXEIE;
	#endif

    return true;
}

/*
example：
uart_receive(partial_valu,max_len);
工作流程：
UART硬件 → RDR寄存器 → 中断服务程序 → 环形缓冲区 → uart_receive() → 应用程序
*/
// 非阻塞式UART接收（从缓冲区读取）
uint32_t uart_receive(uint8_t *buffer, uint32_t max_length) {
    uint32_t count = 0;
    // 从接收缓冲区读取数据
    while (Rx_Buffer_t.rx_head != Rx_Buffer_t.rx_tail && count < max_length) {
        buffer[count++] = Rx_Buffer_t.rx_buffer[Rx_Buffer_t.rx_tail];
        Rx_Buffer_t.rx_tail = (Rx_Buffer_t.rx_tail + 1) % RX_BUFFER_SIZE;
    }
    return count;
}

// UART接收中断处理函数
void UARTx_RX_IRQHandler(void) {
    //if (UARTx->SR & UART_SR_RXNE) 
	{
        // 读取接收到的数据
        //uint8_t data = UARTx->DR;
        // 将数据存入接收缓冲区（注意处理溢出）
        uint32_t next_head = (Rx_Buffer_t.rx_head + 1) % RX_BUFFER_SIZE;
        if (next_head != Rx_Buffer_t.rx_tail) { // 缓冲区未满
            //Rx_Buffer_t.rx_buffer[Rx_Buffer_t.rx_head] = data;
            Rx_Buffer_t.rx_head = next_head;
        } else {
            // 缓冲区溢出处理（可记录错误或丢弃数据）
			//printf("Error!\n");
        }
    }
}

// 校验和计算函数
uint8_t calculate_checksum(uint8_t *data, uint32_t length) {
    uint8_t sum = 0;
    for (uint32_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
    //或其他校验方式
}

// 数据解析函数
void uart_receive_unpackage(uint8_t *rece_data) {
    uint8_t ptr = 0;
    
    // 验证帧头
    if (rece_data[ptr++] != UART_HEADER_0 || 
        rece_data[ptr++] != UART_HEADER_1) {
        return; // 帧头错误，丢弃
    }
    
    // 获取命令码
    uint8_t cmd = rece_data[ptr++];
    
    // 获取数据长度
    uint8_t len = rece_data[ptr++];
    
    // 验证数据长度（确保不越界）
    if (ptr + len + 1 > 4) {
        return; // 数据过长，丢弃
    }
    
    // 指向数据区
    uint8_t *data = &rece_data[ptr];
    ptr += len;
    
    // 获取校验和
    uint8_t checksum = rece_data[ptr];
    
    // 验证校验和
    if (calculate_checksum(&rece_data[2], ptr - 2) != checksum) {
        return; // 校验和错误，丢弃
    }
    
    // 根据命令码处理数据
    switch (cmd) {
        case CMD_LED_CTRL:
            // 处理LED控制命令
            // data[0]: LED编号
            // data[1]: 亮度值(0-255)
            //handle_led_control(data[0], data[1]);
            break;
            
        case CMD_MOTOR_CTRL:
            // 处理电机控制命令
            // data[0]: 电机编号
            // data[1]: 速度值(-100~100)
            //handle_motor_control(data[0], (int8_t)data[1]);
            break;
            
        case CMD_SENSOR_READ:
            // 处理传感器读取命令
            // data[0]: 传感器类型
            //read_sensor(data[0]);
            break;
            
        case CMD_SYSTEM_INFO:
            // 处理系统信息请求
            //send_system_info();
            break;
            
        case CMD_FIRMWARE_UPGRADE:
            // 处理固件升级命令
            // data: 固件数据块
            //handle_firmware_upgrade(data, len);
            break;
            
        default:
            // 未知命令处理
            break;
    }
}
/*               解包状态机                    */
// 全局解析上下文
static ParseContext ctx = {STATE_HEADER_0, {0}, 0, 0};

// 帧解析完成回调函数
void frame_received_callback(uint8_t *data, uint8_t length) {
    // 在这里处理完整的帧数据
    // 例如解析命令、分发处理等

    for(uint8_t i = 0; i < length; i++){
     // 根据命令码处理数据
    switch (data[i]) {
        case CMD_LED_CTRL:
            // 处理LED控制命令
            // data[0]: LED编号
            // data[1]: 亮度值(0-255)
            //handle_led_control(data[0], data[1]);
            break;
            
        case CMD_MOTOR_CTRL:
            // 处理电机控制命令
            // data[0]: 电机编号
            // data[1]: 速度值(-100~100)
            //handle_motor_control(data[0], (int8_t)data[1]);
            break;
            
        case CMD_SENSOR_READ:
            // 处理传感器读取命令
            // data[0]: 传感器类型
            //read_sensor(data[0]);
            break;
            
        case CMD_SYSTEM_INFO:
            // 处理系统信息请求
            //send_system_info();
            break;
            
        case CMD_FIRMWARE_UPGRADE:
            // 处理固件升级命令
            // data: 固件数据块
            //handle_firmware_upgrade(data, len);
            break;
            
        default:
            // 未知命令处理
            break;
    }
  }
}

// UART解析函数
void uart_rece_unp(uint8_t *rece_d, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        uint8_t byte = rece_d[i];
        
        switch (ctx.state) {
            case STATE_HEADER_0:
                if (byte == UART_HEADER_0) {
                    ctx.buffer[ctx.index++] = byte;
                    ctx.state = STATE_HEADER_1;
                }
                break;
                
            case STATE_HEADER_1:
                if (byte == UART_HEADER_1) {
                    ctx.buffer[ctx.index++] = byte;
                    ctx.state = STATE_LENGTH;
                } else {
                    // 帧头不匹配，重置解析器
                    ctx.state = STATE_HEADER_0;
                    ctx.index = 0;
                }
                break;
                
            case STATE_LENGTH:
                if (byte <= UART_MAX_LENGTH - 3) { // 确保有足够空间
                    ctx.length = byte;
                    ctx.buffer[ctx.index++] = byte;
                    ctx.state = STATE_DATA;
                } else {
                    // 长度无效，重置解析器
                    ctx.state = STATE_HEADER_0;
                    ctx.index = 0;
                }
                break;
                
            case STATE_DATA:
                ctx.buffer[ctx.index++] = byte;
                if (ctx.index >= ctx.length + 3) { // 数据+帧头+长度
                    ctx.state = STATE_CHECKSUM;
                }
                break;
                
            case STATE_CHECKSUM:
                // 验证校验和
                uint8_t checksum = calculate_checksum(ctx.buffer, ctx.index);
                if (checksum == byte) {
                    // 校验和正确，回调处理完整帧
                    frame_received_callback(ctx.buffer, ctx.index);
                }
                // 无论校验和是否正确，都重置解析器
                ctx.state = STATE_HEADER_0;
                ctx.index = 0;
                break;
        }
    }
}