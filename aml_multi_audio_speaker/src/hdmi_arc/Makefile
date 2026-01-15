# ==============================================================================
# HDMI ARC 模块 Makefile - 仅高/中端产品编译
# 适配晶晨AML SOC Linux SDK，支持HDMI ARC/eARC功能
# ==============================================================================

# 导入全局编译配置
include ../../lib/compile_flags.mk
include ../../lib/toolchain.mk
include ../../lib/lib_deps.mk

# 模块名称
MODULE_NAME := hdmi_arc

# 源文件列表
SRC_FILES := hdmi_arc.c

# 头文件路径
INC_PATH := -I../../include -I./include

# 编译输出路径
OBJ_PATH := ../../obj/$(MODULE_NAME)

# 目标文件列表
OBJ_FILES := $(patsubst %.c, $(OBJ_PATH)/%.o, $(SRC_FILES))

# 创建目标目录
$(shell mkdir -p $(OBJ_PATH))

# 编译规则：将.c文件编译为.o文件
$(OBJ_PATH)/%.o: %.c
	$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

# 目标：编译当前模块
all: $(OBJ_FILES)
	@echo "
33[32m✅ $(MODULE_NAME) module compile success!33[0m"
	@echo "   Object files: $(OBJ_FILES)"

# 目标：清理当前模块
clean:
	$(RM) $(OBJ_PATH)/*.o
	@echo "
33[33m⚠️  $(MODULE_NAME) module clean completed!33[0m"

# 目标：强制清理
force_clean: clean

.PHONY: all clean force_clean
