# ==============================================================================
# HDMI ARC 模块 Makefile - 仅高/中端产品编译
# 适配晶晨AML SOC Linux SDK，支持HDMI ARC/eARC功能
# ==============================================================================

# 导入全局编译配置
include ../../lib/toolchain.mk
include ../../lib/compile_flags.mk

# 模块名称
MODULE_NAME := hdmi_arc

# 源文件列表
SRC_FILES := hdmi_arc.c

# 目标文件列表
OBJ_FILES := $(SRC_FILES:.c=.o)

# 输出静态库路径
LIB_OUT_DIR := ../../lib/output
LIB_FILE    := $(LIB_OUT_DIR)/lib$(MODULE_NAME).a

# 编译规则：将.c文件编译为.o文件
%.o: %.c
	$(CC) $(CFLAGS) -I./ -I../../include -c $< -o $@

# 目标：编译当前模块
all: $(LIB_FILE)
	@echo -e "\033[32m ✅ $(MODULE_NAME) module compile success!\033[0m"
	@echo -e "\033[32m    Library file: $(LIB_FILE)\033[0m"

$(LIB_FILE): $(OBJ_FILES)
	$(MKDIR) -p $(LIB_OUT_DIR)
	$(AR) rcs $@ $^

# 目标：清理当前模块
clean:
	$(RM) -rf $(OBJ_FILES) $(LIB_FILE)
	@echo -e "\033[33m ⚠️  $(MODULE_NAME) module clean completed!\033[0m"

# 目标：强制清理
force_clean: clean

.PHONY: all clean force_clean
