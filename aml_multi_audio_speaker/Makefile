# 核心修改 ①：新增【产品分级编译宏】+【模块按需编译开关】（最核心，控制所有产品差异）
# 链接cJSON库，必须开启注释兼容
CFLAGS += -DCJSON_ALLOW_COMMENTS -I./src/storage -I./include
LDFLAGS += -L./lib -ljson-c -lpthread -ldl

# Makefile 产品编译宏配置 - 简化版本
PRODUCT ?= 1  # 默认产品类型为1
CFLAGS += -DCURRENT_PRODUCT_TYPE=$(PRODUCT)

# 头文件路径
CFLAGS += -I./include -I./src/storage -I./src/peripheral -I./src/hal/include -I./src/pal/include

# HAL和PAL层编译配置
SUBDIRS += src/hal/src src/pal/src
LIBS += -Lsrc/hal/src -lhal -Lsrc/pal/src -lpal



 