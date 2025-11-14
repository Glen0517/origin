/*
 * 简单的硬件抽象层测试代码
 */

#include "../platform/platform.h"
#include <stdio.h>

// 主测试函数
int main(void) {
    printf("简单硬件抽象层测试程序\n");
    printf("========================\n");
    
    // 初始化平台
    if (!platform_init(NULL)) {
        printf("平台初始化失败\n");
        return -1;
    }
    
    printf("平台初始化成功\n");
    printf("测试完成！\n");
    
    return 0;
}