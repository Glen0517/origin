#ifndef _USART_H
#define _USART_H
#include <stdint.h>
#include <stdbool.h>
//////////////////////////////////////////////////////////////////////////////////
//������ֻ��ѧϰʹ�ã�δ���������ɣ��������������κ���;
//ALIENTEK STM32F429������
//����1��ʼ��		   
//����ԭ��@ALIENTEK
//������̳:www.openedv.csom
//�޸�����:2015/6/23
//�汾��V1.0
//��Ȩ���У�����ؾ���
//Copyright(C) ����ԭ�� 2009-2019
//All rights reserved
//********************************************************************************
//V1.0�޸�˵�� 
////////////////////////////////////////////////////////////////////////////////// 	
#define USART_REC_LEN  			200  	//�����������ֽ��� 200
#define EN_USART1_RX 			1		//ʹ�ܣ�1��/��ֹ��0������1����
	  	
// 无符号整数（8位到64位）
typedef unsigned char       uint8_t;    // 8位无符号（范围：0~255）
typedef unsigned short      uint16_t;   // 16位无符号（范围：0~65535）
typedef unsigned int        uint32_t;   // 32位无符号（范围：0~4294967295）
typedef unsigned long long  uint64_t;   // 64位无符号

// 有符号整数（8位到64位）
typedef signed char         int8_t;     // 8位有符号（范围：-128~127）
typedef short               int16_t;    // 16位有符号（范围：-32768~32767）
typedef int                 int32_t;    // 32位有符号
typedef long long           int64_t;    // 64位有符号

// 布尔类型（通常用8位无符号）
typedef uint8_t             bool_t;     // 0=false, 非0=true

#define TX_BUFFER_SIZE  256  // USART发送缓冲区大小
#define RX_BUFFER_SIZE  256  // USART接收缓冲区大小

// 定义命令码
#define CMD_LED_CTRL    0x01
#define CMD_MOTOR_CTRL  0x02
#define CMD_SENSOR_READ 0x03
#define CMD_SYSTEM_INFO 0x04
#define CMD_FIRMWARE_UPGRADE 0x05

// 协议常量定义
#define UART_HEADER_0     0xAA    // 帧头第一个字节
#define UART_HEADER_1     0x55    // 帧头第二个字节
#define UART_MAX_LENGTH   64      // 最大帧长度

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
    uint8_t buffer[UART_MAX_LENGTH]; // 接收缓冲区
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

extern bool uart_send(uint8_t *data, uint32_t length) ;
extern uint32_t uart_receive(uint8_t *buffer, uint32_t max_length) ;
void UARTx_RX_IRQHandler(void);
#endif
