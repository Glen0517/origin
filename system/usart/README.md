# UART驱动使用说明

## 概述

本目录包含STM32F4系列微控制器的UART驱动实现，符合STM32 HAL库风格。驱动提供了非阻塞式和阻塞式的发送和接收功能，支持中断处理和数据解析。

## 文件结构

- `usart.h`: UART驱动头文件，包含函数声明和常量定义
- `usart.c`: UART驱动实现文件，包含初始化、发送、接收和中断处理函数
- `usart_demo.h`: UART示例头文件，包含示例函数声明
- `usart_demo.c`: UART示例实现文件，包含UART的使用示例
- `README.md`: 本说明文件

## 使用方法

### 1. 初始化UART

```c
// 初始化UART，波特率115200
uart_init(115200);
```

### 2. 非阻塞式发送

```c
uint8_t data[] = "Hello, UART!";
uart_send(data, sizeof(data));
```

### 3. 阻塞式发送

```c
uint8_t data[] = "Hello, UART!";
uart_send_blocking(data, sizeof(data));
```

### 4. 非阻塞式接收

```c
uint8_t buffer[128];
uint32_t len = uart_receive(buffer, sizeof(buffer) - 1);
if (len > 0) {
    buffer[len] = '\0'; // 添加字符串结束符
    printf("Received: %s\n", buffer);
}
```

### 5. 阻塞式接收

```c
uint8_t buffer[128];
printf("Waiting for data...\n");
uint32_t len = uart_receive_blocking(buffer, sizeof(buffer) - 1, 5000); // 等待5秒
if (len > 0) {
    buffer[len] = '\0'; // 添加字符串结束符
    printf("Received: %s\n", buffer);
} else {
    printf("Receive timeout\n");
}
```

### 6. 使用示例任务

在main.c中，我们创建了一个UART演示任务，你可以通过以下方式启动：

```c
// 创建UART演示任务
xTaskCreate((TaskFunction_t )uart_demo_task,
            (const char*    )"uart_demo_task",
            (uint16_t       )UART_DEMO_STK_SIZE,
            (void*          )NULL,
            (UBaseType_t    )UART_DEMO_TASK_PRIO,
            (TaskHandle_t*  )&UARTDemoTask_Handler);
```

## 注意事项

1. 根据实际硬件配置，修改`usart.c`中的`USARTx`定义，指向正确的UART实例（如`USART1`、`USART2`等）

2. 确保在`HAL_UART_MspInit`函数中正确配置UART的GPIO引脚和时钟

3. 根据实际需求调整缓冲区大小

4. 示例代码中使用了`printf`函数，需要确保已经重定向到UART

5. 对于中断处理，确保在NVIC中正确配置了UART中断优先级

## 示例功能

`usart_demo.c`中提供了以下功能：

- UART初始化和配置
- 非阻塞式发送和接收测试
- 阻塞式发送和接收测试
- 数据解析示例
- `printf`和`getchar`函数重定向到UART

希望本驱动能够帮助你快速实现UART通信功能。如有任何问题或建议，请随时联系我们。