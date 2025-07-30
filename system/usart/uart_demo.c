/*
* @file    usart_demo.c
* @brief   UART驱动使用示例
* @author  AI Assistant
* @date    2023-11-10
*/
#include "uart_demo.h"

// 全局变量
uint8_t tx_buffer[DEMO_TX_BUFFER_SIZE];
uint8_t rx_buffer[DEMO_RX_BUFFER_SIZE];

#if STM32_UART_FLAG
UART_STRUCT UART_STM32_INSTANCE = {
    .g_baudrate = 115200,

    .uart_gpio_init = stm32_uart_gpio_init,
    .uart_init = stm32_uart_init,
    .uart_send = stm32_uart_send,
    .uart_send_blocking = stm32_uart_send_blocking,
    .uart_receive = stm32_uart_receive,
    .uart_receive_blocking = stm32_uart_receive_blocking
};
#endif

/*
* @brief  UART MSP初始化函数
* @param  huart: UART句柄
* @retval 无
*/
void hal_uart_mspInit(void) {
    UART_STM32_INSTANCE.uart_gpio_init();
    UART_STM32_INSTANCE.uart_init(UART_STM32_INSTANCE.g_baudrate);
}

/*
* @brief  UART发送测试函数
* @param  无
* @retval 无
*/
void uart_send_test(void) {
    // 准备发送数据
    sprintf((char *)tx_buffer, "Hello, UART Demo!\r\n");
    uint32_t len = strlen((char *)tx_buffer);
    
    // 使用非阻塞式发送
    if (UART_STM32_INSTANCE.uart_send(tx_buffer, len)) {
        printf("非阻塞式发送成功\r\n");
    } else {
        printf("非阻塞式发送失败\r\n");
    }
    
    // 延时一段时间
    //HAL_Delay(1000);
    
    // 准备发送数据
    sprintf((char *)tx_buffer, "这是阻塞式发送测试\r\n");
    len = strlen((char *)tx_buffer);
    
    // 使用阻塞式发送
    if (UART_STM32_INSTANCE.uart_send_blocking(tx_buffer, len) == true) {
        printf("阻塞式发送成功\r\n");
    } else {
        printf("阻塞式发送失败\r\n");
    }
}

/*
* @brief  UART接收测试函数
* @param  无
* @retval 无
*/
void uart_receive_test(void) {
    uint32_t len;
    
    // 非阻塞式接收
    len = UART_STM32_INSTANCE.uart_receive(rx_buffer, DEMO_RX_BUFFER_SIZE - 1);
    if (len > 0) {
        rx_buffer[len] = '\0'; // 添加字符串结束符
        printf("非阻塞式接收: %s\r\n", rx_buffer);
    }
    
    // 阻塞式接收（等待5秒）
    printf("等待接收数据...\r\n");
    len = UART_STM32_INSTANCE.uart_receive_blocking(rx_buffer, DEMO_RX_BUFFER_SIZE - 1, 5000);
    if (len > 0) {
        rx_buffer[len] = '\0'; // 添加字符串结束符
        printf("阻塞式接收: %s\r\n", rx_buffer);
    } else {
        printf("阻塞式接收超时\r\n");
    }
}

/*
* @brief  UART主测试函数
* @param  无
* @retval 无
*/
void uart_demo(void) {
    // 初始化UART，波特率115200
    hal_uart_mspInit();
    printf("UART初始化成功\r\n");
    
    // 循环测试
    while (1) {
        // 发送测试
        uart_send_test();
        
        // 接收测试
        uart_receive_test();
        
        // 延时2秒
        //HAL_Delay(2000);
    }
}

/*
* @brief  重定向printf函数到UART
* @param  ch: 要输出的字符
* @param  f: 文件指针
* @retval 输出的字符
*/
int fputc(int ch, FILE *f) {
    uint8_t temp[1] = {ch};
    HAL_UART_Transmit(&huart, temp, 1, HAL_MAX_DELAY);
    return ch;
}

/*
* @brief  重定向getchar函数到UART
* @param  无
* @retval 接收到的字符
*/
int fgetc(FILE *f) {
    uint8_t temp = 0;
    HAL_UART_Receive(&huart, &temp, 1, HAL_MAX_DELAY);
    return temp;
}