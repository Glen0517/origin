# src/audio_core/Makefile 工业级编译规则（通用模板）
include ../../lib/toolchain.mk
include ../../lib/compile_flags.mk

# 模块名称 
MODULE_NAME := audio_core
# 当前模块的所有源文件 (相对于当前目录)
SRC_FILES   := audio_core.c audio_decode.c audio_mixer.c audio_ringbuf.c audio_process.c
# 目标文件
OBJ_FILES   := $(SRC_FILES:.c=.o)
# 输出静态库路径
LIB_OUT_DIR := ../../lib/output
LIB_FILE    := $(LIB_OUT_DIR)/lib$(MODULE_NAME).a

# 编译规则
all: $(LIB_FILE)
	@echo -e "\033[32m [$(MODULE_NAME)] compile success: $(LIB_FILE)\033[0m"

$(LIB_FILE): $(OBJ_FILES)
	$(MKDIR) -p $(LIB_OUT_DIR)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ -I./ -I../../include

# 清理规则
clean:
	$(RM) -rf $(OBJ_FILES) $(LIB_FILE)
	@echo -e "\033[32m [$(MODULE_NAME)] clean success\033[0m"

.PHONY: all clean