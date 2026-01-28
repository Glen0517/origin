# 核心修改 ①：新增【产品分级编译宏】+【模块按需编译开关】（最核心，控制所有产品差异）
# 链接cJSON库，必须开启注释兼容
CFLAGS += -DCJSON_ALLOW_COMMENTS -I./src/storage -I./include
LDFLAGS += -L./lib -ljson-c -lpthread -ldl

# Makefile 产品编译宏配置 - 简化版本
PRODUCT ?= 1  # 默认产品类型为1
CFLAGS += -DCURRENT_PRODUCT_TYPE=$(PRODUCT)

# 头文件路径
CFLAGS += -I./include -I./log/include -I./res/include -I./src/storage -I./src/peripheral -I./src/hal/include -I./src/pal/include

# HAL和PAL层编译配置
SUBDIRS += src/hal/src src/pal/src
LIBS += -Lsrc/hal/src -lhal -Lsrc/pal/src -lpal

# 其他模块编译配置
SUBDIRS += src/audio_core src/audio_source src/bluetooth src/comm_mcu src/hdmi_arc src/peripheral src/play_ctrl src/prod_test src/sound_effects src/spdif_optical src/storage src/subwoofer_comm src/system src/volume_ctrl src/wifi_media

# 输出目录
OUT_DIR := ./bin

# 主可执行文件
TARGET := $(OUT_DIR)/aml_multi_audio_speaker

# 主源文件
MAIN_SRC := src/main.c src/main_loop.c src/module_init.c
MAIN_OBJ := $(MAIN_SRC:.c=.o)

# 编译规则
all: build_subdirs $(TARGET)
	@echo -e "\033[32m [Main] Build success: $(TARGET)\033[0m"

build_subdirs:
	@for dir in $(SUBDIRS); do \
		echo "Building $$dir..."; \
		$(MAKE) -C $$dir || exit 1; \
	done

$(TARGET): $(MAIN_OBJ)
	$(MKDIR) -p $(OUT_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean || true; \
	done
	$(RM) -rf $(MAIN_OBJ) $(OUT_DIR)
	@echo -e "\033[32m [Main] Clean success\033[0m"

.PHONY: all clean build_subdirs



 