# ==============================================================================
# 产品分级库链接核心规则 - 基于include/product_type.h宏定义 4类产品按需链接库文件
# 核心：高端回音壁=全量库 | 中端回音壁=基础库+HDMI/SPDIF | 低端蓝牙音箱=纯基础库 | 低音炮=蓝牙MESH+音量库
# 无任何业务逻辑，仅编译规则，与src/include/config无缝衔接，完全解耦
# ==============================================================================
include ./toolchain.mk
include ./compile_flags.mk
include ./lib_deps.mk

# -------------------------- 产品类型宏定义(与include/product_type.h完全一致 无修改) --------------------------
ifndef CURRENT_PRODUCT_TYPE
CURRENT_PRODUCT_TYPE := 1 # 默认编译：1=高端回音壁
endif
PRODUCT_HIGH_END    := 1  # 高端回音壁(全功能)
PRODUCT_MID_END     := 2  # 中端回音壁(HDMI+光纤 无杜比)
PRODUCT_LOW_END     := 3  # 低端蓝牙音箱(仅蓝牙+播放+音量)
PRODUCT_SUBWOOFER   := 4  # 独立低音炮(蓝牙MESH+低音增益)

# -------------------------- 基础库依赖 - 所有产品必链 --------------------------
ALL_LIBS := $(LIB_PATH) $(LIB_SYS_BASE) $(LIB_SYS_AUDIO) $(LIB_SYS_STORAGE) $(LIB_SYS_BT_BASE) $(LIB_SYS_PERIPH) $(LIB_MOD_CORE)

# -------------------------- 【核心逻辑】4类产品 宏控自动裁剪库依赖 --------------------------
# ① 高端回音壁 - 全功能：HDMI+光纤+杜比+蓝牙MESH+WIFI+红外+所有音效
ifeq ($(CURRENT_PRODUCT_TYPE), $(PRODUCT_HIGH_END))
    ALL_LIBS += $(LIB_SYS_HDMI) $(LIB_SYS_SPDIF) $(LIB_SYS_EFFECT) $(LIB_SYS_BT_EXT)
    ALL_LIBS += $(LIB_SYS_WIFI) $(LIB_SYS_IR) $(LIB_MOD_EXTEND)
endif

# ② 中端回音壁 - 基础功能：HDMI+光纤+基础音效 无杜比/WIFI/红外
ifeq ($(CURRENT_PRODUCT_TYPE), $(PRODUCT_MID_END))
    ALL_LIBS += $(LIB_SYS_HDMI) $(LIB_SYS_SPDIF) $(LIB_MOD_EXTEND)
endif

# ③ 低端蓝牙音箱 - 极简功能：仅蓝牙+播放+音量 无任何冗余库 固件体积最小
ifeq ($(CURRENT_PRODUCT_TYPE), $(PRODUCT_LOW_END))
    ALL_LIBS += -L$(LIB_OUT_ROOT) # 仅链接基础自研库 无任何功能库
endif

# ④ 独立低音炮 - 专属功能：蓝牙MESH+低音增益+配对 无其他库依赖
ifeq ($(CURRENT_PRODUCT_TYPE), $(PRODUCT_SUBWOOFER))
    ALL_LIBS += $(LIB_SYS_BT_EXT) -L$(LIB_OUT_ROOT) -lsubwoofer_comm -lvolume_ctrl
endif

# -------------------------- 库依赖导出 - 顶层Makefile直接引用 --------------------------
export ALL_LIBS
export CC LD CFLAGS LDFLAGS