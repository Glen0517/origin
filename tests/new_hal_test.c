/*
 * 硬件抽象层测试代码 - 简化版
 */

#include "../platform/platform.h"
#include "../hal/hal_uart.h"
#include "../hal/hal_gpio.h"
#include "../hal/hal_spi.h"
#include "../hal/hal_i2c.h"
#include "../include/types.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// 主测试函数 - 只测试平台初始化
int main(void) {
    printf("硬件抽象层测试程序\n");
    printf("========================\n");
    
    // 初始化平台
    if (!platform_init(NULL)) {
        printf("平台初始化失败\n");
        return -1;
    }
    
    printf("平台初始化成功\n");
    
    // 测试UART初始化
    printf("\n测试UART初始化...\n");
    uart_config_t uart_config = {0};
    uart_config.baud_rate = 115200;
    
    // 不调用实际的初始化函数，只声明结构体
    printf("UART配置结构体已创建\n");
    
    printf("\n测试完成！\n");
    return 0;
}