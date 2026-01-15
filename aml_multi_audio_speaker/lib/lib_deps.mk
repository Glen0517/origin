# ==============================================================================
# 全量库依赖清单 - 第三方系统库 + AML SDK硬件库 + 自研业务模块库 统一管理
# 命名规范：LIB_SYS_* = 系统/SDK库 | LIB_MOD_* = 自研模块库
# 依赖原则：基础库必链，功能库按需链，无硬编码，全项目唯一库依赖入口
# ==============================================================================
include ./compile_flags.mk

# -------------------------- 【必链接基础库】- 所有4类产品必链 无例外 --------------------------
# Linux标准系统库 (线程/数学/动态链接/文件/日志)
LIB_SYS_BASE    := -lpthread -lm -ldl -lc -lrt -lutil
# 音频基础库 (AML SDK原生，所有音响产品核心依赖：音频播放/音量调节/声道切换)
LIB_SYS_AUDIO   := -lasound -lpcm -laml_audio
# 存储基础库 (AML SDK原生，FLASH读写/配置存储/产线校准，对接src/storage)
LIB_SYS_STORAGE := -laml_flash -ljson-c
# 蓝牙基础库 (AML SDK原生，A2DP/AVRCP基础蓝牙，所有产品必链，对接src/bluetooth)
LIB_SYS_BT_BASE := -lbluetooth -lbt_a2dp -lbt_avrcp
# 外设基础库 (AML SDK原生，GPIO/按键/LED，对接src/peripheral)
LIB_SYS_PERIPH  := -laml_gpio -laml_key -laml_led

# -------------------------- 【可选功能库】- 宏控按需链接 低端产品自动屏蔽 --------------------------
# HDMI ARC/CEC库 (AML SDK原生，HDMI音源检测/音频采集，仅高/中端回音壁需要)
LIB_SYS_HDMI    := -laml_hdmi -lcec -lardp
# SPDIF光纤库 (AML SDK原生，光纤音频解码/传输，仅高/中端回音壁需要)
LIB_SYS_SPDIF   := -laml_spdif -losd
# 音效解码库 (AML SDK授权库，杜比/DTS/虚拟5.1声场，仅高端回音壁需要)
LIB_SYS_EFFECT  := -ldolby -ldts -lvirtual_51
# 蓝牙增强库 (AML SDK原生，蓝牙MESH/低音炮联动，仅高端回音壁+独立低音炮需要)
LIB_SYS_BT_EXT  := -lbt_mesh -lbt_sync
# WIFI投屏库 (AML SDK原生，DLNA/AirPlay投屏，仅高端回音壁需要)
LIB_SYS_WIFI    := -lwifi_dlna -lairplay
# 红外学习库 (AML SDK原生，红外遥控/码值学习，仅高端回音壁需要)
LIB_SYS_IR      := -lir_learn -lrc_core

# -------------------------- 【自研模块库】- src目录业务模块编译后的静态/动态库 --------------------------
# 所有自研库统一输出到LIB_OUT_ROOT，编译规则在Makefile.lib/Makefile.so中定义
LIB_MOD_CORE    := -L$(LIB_OUT_ROOT) -laudio_core -laudio_source -lplay_ctrl -lvolume_ctrl
LIB_MOD_PERIPH  := -L$(LIB_OUT_ROOT) -lperipheral -luart_mcu_comm -lsystem_api
LIB_MOD_EXTEND  := -L$(LIB_OUT_ROOT) -lsound_effects -lhdmi_arc -lspdif_optical -lsubwoofer_comm
LIB_MOD_ALL     := $(LIB_MOD_CORE) $(LIB_MOD_PERIPH) $(LIB_MOD_EXTEND)