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