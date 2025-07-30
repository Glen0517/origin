#ifndef _UART_H
#define _UART_H

//#include "stm32_hal_uart.h"
//#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define TX_BUFFER_SIZE  256  // USART发送缓冲区大小
#define RX_BUFFER_SIZE  256  // USART接收缓冲区大小

//定义串口命令码
typedef enum{
    CMD_LED_CTRL,
    CMD_MOTOR_CTRL,
    CMD_SENSOR_READ,
    CMD_SYSTEM_INFO,
    CMD_UPDATE_FW
} UART_STATE;

//协议常量定义
typedef enum{
    UART_HEADER_0 = 0xAA,
    UART_HEADER_1 = 0x55,
    UART_MAX_LEN = 64
} UART_COMM_CMD;

// 解析状态枚举
typedef enum {
    STATE_HEADER_0,
    STATE_HEADER_1,
    STATE_LENGTH,
    STATE_DATA,
    STATE_CHECKSUM
} ParseState;

// 解析上下文结构
typedef struct {
    ParseState state;            // 当前状态
    uint8_t buffer[UART_MAX_LEN]; // 接收缓冲区
    uint8_t index;               // 当前缓冲区索引
    uint8_t length;              // 帧数据长度
} ParseContext;

// USART发送缓冲区结构体
typedef struct {
    uint8_t tx_buffer[TX_BUFFER_SIZE];
    uint32_t tx_head;
    uint32_t tx_tail;
} TX_Buffer_Struct;

// USART接收缓冲区结构体
typedef struct {
    uint8_t rx_buffer[RX_BUFFER_SIZE];
    uint32_t rx_head;
    uint32_t rx_tail;
} RX_Buffer_Struct;

typedef struct {
    uint32_t g_baudrate;

    void (*uart_gpio_init)(void);
    void (*uart_init)(uint32_t baudrate);
    bool (*uart_send)(uint8_t *data, uint32_t length);
    uint32_t (*uart_receive)(uint8_t *buffer, uint32_t max_len);
    bool (*uart_send_blocking)(uint8_t *data, uint32_t length);
    uint32_t (*uart_receive_blocking)(uint8_t *buffer, uint32_t max_length, uint32_t timeout);

    uint8_t (*calculate_checksum)(uint8_t *data, uint32_t length);
    void (*uart_receive_unpackage)(uint8_t *rece_data);
    void (*uart_receive_state_machine)(uint8_t *rece_d, uint8_t len);
}UART_STRUCT;

//GPIO初始化函数
void stm32_uart_gpio_init(void);

// UART初始化函数
void stm32_uart_init(uint32_t baudrate);

// 非阻塞式UART发送（放入缓冲区，由中断发送）
bool stm32_uart_send(uint8_t *data, uint32_t length);

// 阻塞式UART发送（直接发送，直到完成）
bool stm32_uart_send_blocking(uint8_t *data, uint32_t length);

// 非阻塞式UART接收（从缓冲区读取）
uint32_t stm32_uart_receive(uint8_t *buffer, uint32_t max_length);

// 阻塞式UART接收（等待接收完成）
uint32_t stm32_uart_receive_blocking(uint8_t *buffer, uint32_t max_length, uint32_t timeout);

// UART中断处理函数
void UARTx_IRQHandler(void);

// 校验和计算函数
uint8_t calculate_checksum(uint8_t *data, uint32_t length);

// 数据解析函数
void uart_receive_unpackage(uint8_t *rece_data);

// UART解析函数
void uart_receive_state_machine(uint8_t *rece_d, uint8_t len);

#endif