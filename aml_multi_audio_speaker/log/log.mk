# ==============================================================================
# 工业级日志模块编译规则 - AML Audio Speaker 专属
# 功能：编译log/src为静态库，对接lib编译链，宏控适配产品类型，无缝集成到项目中
# 规范：与lib/目录编译规则一致，2空格缩进、UTF8编码、无硬编码，与src/include/config完全解耦
# ==============================================================================
include ../lib/toolchain.mk
include ../lib/compile_flags.mk

# -------------------------- 日志模块编译路径定义 --------------------------
LOG_INC_ROOT  := ./include
LOG_SRC_ROOT  := ./src
LOG_LIB_OUT   := ../lib/output
LOG_OBJ_DIR   := ./obj
$(MKDIR) $(LOG_OBJ_DIR) $(LOG_LIB_OUT)

# -------------------------- 日志模块编译参数 --------------------------
CFLAGS  += -I$(LOG_INC_ROOT) -I$(INC_ROOT)
CFLAGS  += -DCURRENT_PRODUCT_TYPE=$(CURRENT_PRODUCT_TYPE)
CFLAGS  += -Wall -Wextra -Wno-unused-parameter -std=c99

# -------------------------- 日志模块源码与目标文件 --------------------------
LOG_SRC_FILES := $(wildcard $(LOG_SRC_ROOT)/*.c)
LOG_OBJ_FILES := $(patsubst $(LOG_SRC_ROOT)/%.c, $(LOG_OBJ_DIR)/%.o, $(LOG_SRC_FILES))

# -------------------------- 静态库编译规则 - 日志模块核心产物 --------------------------
all: liblogger.a
	@echo -e "\033[32m✅ [日志模块编译完成] 输出库文件: $(LOG_LIB_OUT)/liblogger.a ✅\033[0m"

liblogger.a: $(LOG_OBJ_FILES)
	$(AR) $(ARFLAGS) $(LOG_LIB_OUT)/liblogger.a $(LOG_OBJ_FILES)

$(LOG_OBJ_DIR)/%.o: $(LOG_SRC_ROOT)/%.c
	@echo -e "\033[36m🔨 编译日志模块: $@ ...\033[0m"
	$(CC) $(CFLAGS) -c $< -o $@

# -------------------------- 清理规则 --------------------------
clean:
	$(RM) $(LOG_OBJ_DIR)/*.o $(LOG_LIB_OUT)/liblogger.a
	@echo -e "\033[32m✅ [日志模块清理完成] ✅\033[0m"

# -------------------------- 伪目标声明 --------------------------
.PHONY: all clean liblogger.a