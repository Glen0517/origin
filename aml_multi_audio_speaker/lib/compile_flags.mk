# 编译标志配置

# 通用编译标志
CFLAGS += -std=c99 -pedantic

# 优化选项
CFLAGS += -O2 -ffast-math -funroll-loops

# 调试选项
CFLAGS += -g -ggdb

# 警告选项
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -Wno-unused-parameter
CFLAGS += -Wno-unused-variable
CFLAGS += -Wno-unused-function

# 头文件路径
CFLAGS += -I../include
CFLAGS += -I../../include
CFLAGS += -I../../src
CFLAGS += -I../../src/hal/include
CFLAGS += -I../../src/pal/include

# 宏定义
CFLAGS += -D_GNU_SOURCE
CFLAGS += -DDEBUG

# 链接选项
LDFLAGS += -lpthread -ldl -lm
