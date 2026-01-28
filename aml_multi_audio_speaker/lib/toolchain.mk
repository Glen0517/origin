# 工具链配置

# 编译器设置
CC := gcc
AR := ar
RM := rm -f
MKDIR := mkdir -p

# 编译选项
CFLAGS += -Wall -Wextra -Werror -O2 -g

# 平台相关配置
ifeq ($(OS),Windows_NT)
    # Windows平台
    CFLAGS += -DWIN32
else
    # Linux平台
    CFLAGS += -DLINUX
endif
