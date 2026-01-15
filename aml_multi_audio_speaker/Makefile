# 核心修改 ①：新增【产品分级编译宏】+【模块按需编译开关】（最核心，控制所有产品差异）
# 链接cJSON库，必须开启注释兼容
CFLAGS += -DCJSON_ALLOW_COMMENTS -I./src/storage -I./include
LDFLAGS += -L./lib -ljson-c -lpthread -ldl

# Makefile 产品编译宏配置
ifeq ($(PRODUCT),1)
CFLAGS += -DCURRENT_PRODUCT_TYPE=1
endif
ifeq ($(PRODUCT),2)
CFLAGS += -DCURRENT_PRODUCT_TYPE=2
endif
ifeq ($(PRODUCT),3)
CFLAGS += -DCURRENT_PRODUCT_TYPE=3
endif
ifeq ($(PRODUCT),4)
CFLAGS += -DCURRENT_PRODUCT_TYPE=4
endif

# 头文件路径
CFLAGS += -I./include -I./src/storage -I./src/peripheral



 