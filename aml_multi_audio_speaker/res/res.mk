# ==============================================================================
# 工业级资源模块编译规则 - AML Audio Speaker 专属
# 功能：编译res/src为静态库，对接lib编译链，宏控适配产品类型，无缝集成到项目中
# 规范：与lib/目录编译规则一致，2空格缩进、UTF8编码、无硬编码，与src/include/config完全解耦
# ==============================================================================
include ../lib/toolchain.mk
include ../lib/compile_flags.mk

# -------------------------- 资源模块编译路径定义 --------------------------
RES_INC_ROOT  := ./include
RES_SRC_ROOT  := ./src
RES_LIB_OUT   := ../lib/output
RES_OBJ_DIR   := ./obj
$(MKDIR) $(RES_OBJ_DIR) $(RES_LIB_OUT)

# -------------------------- 资源模块编译参数 --------------------------
CFLAGS  += -I$(RES_INC_ROOT) -I$(INC_ROOT) -I../log/include
CFLAGS  += -DCURRENT_PRODUCT_TYPE=$(CURRENT_PRODUCT_TYPE)
CFLAGS  += -Wall -Wextra -Wno-unused-parameter -std=c99 -O2
# 嵌入式优化：关闭浮点运算、减小代码体积、开启内存优化
CFLAGS  += -ffunction-sections -fdata-sections -fno-exceptions

# -------------------------- 资源模块源码与目标文件 --------------------------
RES_SRC_FILES := $(wildcard $(RES_SRC_ROOT)/*.c)
RES_OBJ_FILES := $(patsubst $(RES_SRC_ROOT)/%.c, $(RES_OBJ_DIR)/%.o, $(RES_SRC_FILES))

# -------------------------- 静态库编译规则 - 资源模块核心产物 --------------------------
all: libres.a
	@echo -e "\033[32m [资源模块编译完成] 输出库文件: $(RES_LIB_OUT)/libres.a \033[0m"

libres.a: $(RES_OBJ_FILES)
	$(AR) $(ARFLAGS) $(RES_LIB_OUT)/libres.a $(RES_OBJ_FILES)

$(RES_OBJ_DIR)/%.o: $(RES_SRC_ROOT)/%.c
	@echo -e "\033[36m  编译资源模块: $@ ...\033[0m"
	$(CC) $(CFLAGS) -c $< -o $@

# -------------------------- 清理规则 --------------------------
clean:
	$(RM) $(RES_OBJ_DIR)/*.o $(RES_LIB_OUT)/libres.a
	@echo -e "\033[32m✅ [资源模块清理完成] \033[0m"

# -------------------------- 伪目标声明 --------------------------
.PHONY: all clean libres.a