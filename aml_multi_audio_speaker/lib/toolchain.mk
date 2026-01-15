# ==============================================================================
# 工业级交叉编译工具链配置 - AML SOC 多媒体音响产品专用 (适配S905X4/A311D/T950全系列)
# 功能：统一管理编译工具链、系统根目录、头文件/库文件路径，一键切换编译平台
# 无业务代码，仅编译配置，与src/include/config完全解耦
# ==============================================================================
ifndef TOOLCHAIN_TYPE
# 默认编译平台：AML_SOC(交叉编译-量产) | 可选：X86_LOCAL(本地编译-开发调试)
TOOLCHAIN_TYPE := AML_SOC
endif

# -------------------------- 编译工具链定义 --------------------------
ifeq ($(TOOLCHAIN_TYPE), AML_SOC)
    # 晶晨AML SOC官方交叉编译工具链 (SDK自带，无需额外安装)
    CROSS_COMPILE  := aarch64-linux-gnu-
    CC             := $(CROSS_COMPILE)gcc
    CXX            := $(CROSS_COMPILE)g++
    AR             := $(CROSS_COMPILE)ar
    LD             := $(CROSS_COMPILE)ld
    STRIP          := $(CROSS_COMPILE)strip
    # SDK根目录 (请根据你的实际SDK路径修改，必配！)
    SDK_ROOT       := /opt/aml_sdk_v3.0
    # AML SOC系统根文件系统路径 (SDK自带，库/头文件基础路径)
    SYSROOT        := $(SDK_ROOT)/sysroot
    # SDK头文件搜索路径 (对接AML SDK API头文件)
    INC_PATH       += -I$(SYSROOT)/usr/include -I$(SDK_ROOT)/include/hardware
    # SDK库文件搜索路径 (对接AML SDK库文件)
    LIB_PATH       += -L$(SYSROOT)/usr/lib -L$(SDK_ROOT)/lib -L$(SDK_ROOT)/lib/aarch64
else ifeq ($(TOOLCHAIN_TYPE), X86_LOCAL)
    # X86 Ubuntu本地编译工具链 (开发调试用，无需SDK)
    CROSS_COMPILE  := 
    CC             := gcc
    CXX            := g++
    AR             := ar
    LD             := ld
    STRIP          := strip
    # 本地系统路径
    INC_PATH       += -I/usr/include -I/usr/local/include
    LIB_PATH       += -L/usr/lib -L/usr/local/lib
endif

# -------------------------- 通用工具宏定义 --------------------------
RM              := rm -rf
CP              := cp -rf
MKDIR           := mkdir -p
MAKE            := make -j$(shell nproc)
ARFLAGS         := crs          # 静态库编译参数: c=创建 r=替换 s=生成索引
SOFLAGS         := -shared -fPIC# 动态库编译参数: 共享+位置无关码