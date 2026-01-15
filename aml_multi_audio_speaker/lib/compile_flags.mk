# ==============================================================================
# 工业级编译&链接优化参数配置 - 全项目统一引用，无零散编译配置
# 功能：统一管理编译CFLAGS/链接LDFLAGS、编译优化等级、平台编译参数、目录宏定义
# 模式：RELEASE(量产-极致优化) | DEBUG(开发-调试友好) 一键切换
# ==============================================================================
ifndef BUILD_TYPE
# 默认编译模式：RELEASE(量产) 体积小/速度快/无调试信息 | DEBUG(开发) 无优化/带调试符号
BUILD_TYPE      := RELEASE
endif

# -------------------------- 编译模式核心参数 --------------------------
ifeq ($(BUILD_TYPE), RELEASE)
    # 量产模式：O2优化(性能+体积最优)、关闭调试、警告视为错误、去除符号表
    CFLAGS          += -O2 -DNDEBUG -Wall -Werror -Wextra -Wno-unused-parameter
    LDFLAGS         += -O2 -s --gc-sections # -s=去符号表 --gc-sections=删除无用段 极致瘦身
else ifeq ($(BUILD_TYPE), DEBUG)
    # 开发模式：O0无优化、开启调试、保留符号表、仅警告不报错、便于GDB调试
    CFLAGS          += -O0 -g -DDEBUG -Wall -Wextra -Wno-error
    LDFLAGS         += -g # 保留调试符号表
endif

# -------------------------- 通用编译规则 --------------------------
CFLAGS          += -std=c99 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L # 强制C99标准
CFLAGS          += -fstack-protector-strong -fpie -pie             # 栈保护+地址随机化 安全加固
CFLAGS          += -fPIC -D_FORTIFY_SOURCE=2                       # 位置无关代码+库函数安全增强
CFLAGS          += -fno-strict-overflow -fno-delete-null-pointer-checks  # 防止不安全的优化
CFLAGS          += -Wformat -Wformat-security -Werror=format-security  # 格式字符串安全检查
LDFLAGS         += -lpthread -lm -ldl -lc -lrt                     # Linux标准系统库 必链
LDFLAGS         += --as-needed --no-undefined                      # 按需链接 无未定义引用 防冗余
LDFLAGS         += -z relro -z now -z noexecstack                  # 内存区域保护：RELRO+立即绑定+不可执行栈

# -------------------------- 架构专属编译参数 --------------------------
ifeq ($(TOOLCHAIN_TYPE), AML_SOC)
    CFLAGS      += -march=armv8-a -mtune=cortex-a53 -mfpu=neon-fp-armv8
endif

# -------------------------- 项目目录宏定义(与你的架构完全匹配) --------------------------
SRC_ROOT        := ../src
INC_ROOT        := ../include
CFG_ROOT        := ../config
BIN_ROOT        := ../bin
LIB_OUT_ROOT    := ./output  # 库文件输出目录
$(MKDIR) $(LIB_OUT_ROOT)     # 自动创建输出目录