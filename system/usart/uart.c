#include "uart.h"
#include <stdbool.h>

// 定义UART实例（根据实际硬件配置修改）
#define USARTx USART1

// 定义UART句柄
UART_HandleTypeDef huart;

// 缓冲区定义
#define UART_SWITCH 1  // 启用UART中断

// 初始化发送缓冲区
TX_Buffer_Struct Tx_Buffer_t ={
	.tx_buffer = {0},
	.tx_head = 0,
	.tx_tail = 0,
};

// 初始化接收缓冲区
RX_Buffer_Struct Rx_Buffer_t ={
	.rx_buffer = {0},
	.rx_head = 0,
	.rx_tail =0,
};

// 全局解析上下文
static ParseContext ctx = {STATE_HEADER_0, {0}, 0, 0};

void stm32_uart_gpio_init(void)
{
    // 启用GPIO时钟
    __HAL_RCC_GPIOA_CLK_ENABLE();

        // 配置TX引脚
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // 配置RX引脚
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_INPUT;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/*
* @brief  UART初始化函数
* @param  baudrate: 波特率
*/
void stm32_uart_init(uint32_t baudrate)
{
    // 启用UART时钟
    __HAL_RCC_USART1_CLK_ENABLE();
    
    // 初始化UART句柄
    huart.Instance = USARTx;  // 替换为实际的USART实例
    huart.Init.BaudRate = baudrate;
    huart.Init.WordLength = UART_WORDLENGTH_8B;
    huart.Init.StopBits = UART_STOPBITS_1;
    huart.Init.Parity = UART_PARITY_NONE;
    huart.Init.Mode = UART_MODE_TX_RX;
    huart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart.Init.OverSampling = UART_OVERSAMPLING_16;
        
    // 配置UART中断
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/*
* @brief  非阻塞式UART发送（放入缓冲区，由中断发送）
* @param  data: 要发送的数据
* @param  length: 数据长度
* @retval 是否成功
*/
bool stm32_uart_send(uint8_t *data, uint32_t length)
{
    // 检查缓冲区是否有足够空间
    uint32_t space_num = (Tx_Buffer_t.tx_tail > Tx_Buffer_t.tx_head) ? 
                     (Tx_Buffer_t.tx_tail - Tx_Buffer_t.tx_head - 1) : 
                     (TX_BUFFER_SIZE - Tx_Buffer_t.tx_head + Tx_Buffer_t.tx_tail - 1);
    
    if (space_num < length) {
        return false; // 缓冲区已满
    }

    // 关中断保护缓冲区操作
    __HAL_UART_DISABLE_IT(&huart, UART_IT_TXE);

    // 将数据复制到发送缓冲区
    for (uint32_t i = 0; i < length; i++) {
        Tx_Buffer_t.tx_buffer[Tx_Buffer_t.tx_head] = data[i];
        Tx_Buffer_t.tx_head = (Tx_Buffer_t.tx_head + 1) % TX_BUFFER_SIZE;
    }
    
    // 使能发送中断
    __HAL_UART_ENABLE_IT(&huart, UART_IT_TXE);

    return true;
}

/*
* @brief  阻塞式UART发送（直接发送，直到完成）
* @param  data: 要发送的数据
* @param  length: 数据长度
* @retval HAL状态
*/
bool stm32_uart_send_blocking(uint8_t *data, uint32_t length)
{
    return HAL_UART_Transmit(&huart, data, length, HAL_MAX_DELAY);
}

/*
* @brief  非阻塞式UART接收（从缓冲区读取）
* @param  buffer: 接收缓冲区
* @param  max_length: 最大接收长度
* @retval 实际接收长度
*/
uint32_t stm32_uart_receive(uint8_t *buffer, uint32_t max_length) {
    uint32_t count = 0;
    
    // 关中断保护缓冲区操作
    __HAL_UART_DISABLE_IT(&huart, UART_IT_RXNE);
    
    // 从接收缓冲区读取数据
    while (Rx_Buffer_t.rx_head != Rx_Buffer_t.rx_tail && count < max_length) {
        buffer[count++] = Rx_Buffer_t.rx_buffer[Rx_Buffer_t.rx_tail];
        Rx_Buffer_t.rx_tail = (Rx_Buffer_t.rx_tail + 1) % RX_BUFFER_SIZE;
    }
    
    // 使能接收中断
    __HAL_UART_ENABLE_IT(&huart, UART_IT_RXNE);
    
    return count;
}

/*
* @brief  阻塞式UART接收（等待接收完成）
* @param  buffer: 接收缓冲区
* @param  max_length: 最大接收长度
* @param  timeout: 超时时间（毫秒）
* @retval 实际接收长度
*/
uint32_t stm32_uart_receive_blocking(uint8_t *buffer, uint32_t max_length, uint32_t timeout) {
    uint32_t received_length = 0;
    hal_status_t status = HAL_UART_Receive(&huart, buffer, max_length, timeout);
    
    if (status == HAL_OK) {
        received_length = max_length;
    } else if (status == HAL_TIMEOUT) {
        // 超时，但可能已接收部分数据
        received_length = huart.RxXferSize - huart.RxXferCount;
    }
    
    return received_length;
}

/*
* @brief  UART中断处理函数
* @retval 无
*/
void UARTx_IRQHandler(void) {
    uint32_t isrflags = READ_REG(huart.Instance->SR);
    uint32_t cr1its = READ_REG(huart.Instance->CR1);
    uint32_t cr3its = READ_REG(huart.Instance->CR3);
    uint32_t errorflags = 0x00U;
    
    // 处理接收中断
    if (((isrflags & USART_SR_RXNE) != RESET) && ((cr1its & USART_CR1_RXNEIE) != RESET)) {
        // 读取接收到的数据
        uint8_t data = (uint8_t)(huart.Instance->DR & (uint8_t)0x00FF);
        
        // 将数据存入接收缓冲区（注意处理溢出）
        uint32_t next_head = (Rx_Buffer_t.rx_head + 1) % RX_BUFFER_SIZE;
        if (next_head != Rx_Buffer_t.rx_tail) { // 缓冲区未满
            Rx_Buffer_t.rx_buffer[Rx_Buffer_t.rx_head] = data;
            Rx_Buffer_t.rx_head = next_head;
        } else {
            // 缓冲区溢出处理
            __HAL_UART_CLEAR_OREFLAG(&huart);
        }
        
        // 重新启用接收中断
        HAL_UART_Receive_IT(&huart, &Rx_Buffer_t.rx_buffer[Rx_Buffer_t.rx_head], 1);
    }
    
    // 处理发送中断
    if (((isrflags & USART_SR_TXE) != RESET) && ((cr1its & USART_CR1_TXEIE) != RESET)) {
        if (Tx_Buffer_t.tx_head != Tx_Buffer_t.tx_tail) {
            // 发送缓冲区有数据
            huart.Instance->DR = (uint8_t)Tx_Buffer_t.tx_buffer[Tx_Buffer_t.tx_tail];
            Tx_Buffer_t.tx_tail = (Tx_Buffer_t.tx_tail + 1) % TX_BUFFER_SIZE;
        } else {
            // 发送缓冲区为空，关闭发送中断
            __HAL_UART_DISABLE_IT(&huart, UART_IT_TXE);
        }
    }
    
    // 处理错误中断
    errorflags = (isrflags & (uint32_t)(USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE));
    if (errorflags != RESET) {
        // 清除错误标志
        huart.Instance->SR = (uint16_t)~errorflags;
    }
}

/*
* @brief  校验和计算函数
* @param  data: 数据指针
* @param  length: 数据长度
* @retval 校验和
*/
uint8_t calculate_checksum(uint8_t *data, uint32_t length) {
    uint8_t sum = 0;
    for (uint32_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
    //或其他校验方式
}

/*
* @brief  数据解析函数
* @param  rece_data: 接收到的数据
* @retval 无
*/
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

/*
* @brief  UART解析函数
* @param  rece_d: 接收到的数据
* @param  len: 数据长度
* @retval 无
*/
void uart_reveive_state_machine(uint8_t *rece_d, uint8_t len) {
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


