# 【第三方库目录】非AML-SDK的外部依赖库
嵌入式 Linux 项目中 lib/ 目录为 【编译核心配置目录】+【库依赖管理中心】+【静态 / 动态库编译规则】，无业务源码，核心作用是：
集中管理所有第三方库依赖 + 自研模块库编译规则，顶层 Makefile 统一引用，无零散编译配置；
基于 product_type.h 的产品分级宏，实现 4 类产品按需链接库文件，低端产品不链接 HDMI / 光纤 / 杜比等无用库，固件体积最小化；
统一交叉编译工具链配置、编译优化参数、链接参数，全项目编译规则唯一入口，杜绝编译不一致问题；
提供静态库 (.a) + 动态库 (.so) 双编译规则，量产用静态库（无运行依赖，稳定），开发用动态库（编译快）；
完美兼容你架构中「src 目录代码不变、include 头文件不变、宏控裁剪逻辑不变」的核心要求，所有编译规则与现有代码无缝对接。

aml_multi_audio_speaker/lib/
├── toolchain.mk          #  核心文件① 交叉编译工具链配置（AML SOC+X86本地编译，一键切换）
├── lib_deps.mk           #  核心文件② 全项目库依赖清单（第三方库+自研库，统一管理，无硬编码）
├── product_libs.mk       #  核心文件③ 产品分级库链接规则（最核心！4类产品按需链接，宏控裁剪）
├── compile_flags.mk      #  核心文件④ 编译/链接优化参数（Linux工业级标准，量产/开发双模式）
├── Makefile.lib          #  静态库编译规则（编译src自研模块为静态库，量产首选，无依赖）
├── Makefile.so           #  动态库编译规则（编译src自研模块为动态库，开发调试首选）
├── lib_common.h          #  库依赖头文件（声明库链接宏、库版本宏，供顶层Makefile/include引用）
└── README.md             #  编译说明文档（工业级规范，含编译指令、参数说明、常见问题）

# aml_multi_audio_speaker - lib目录编译说明文档
## 核心定位
本目录为项目**编译核心配置中心**，无任何业务代码，所有编译规则、库依赖、工具链配置均在此集中管理，是项目唯一的编译入口。
所有配置与项目架构完全匹配，支持4类产品一键编译，src/include/config目录代码无需任何修改。

## 支持的产品类型
1 - 高端回音壁（全功能：HDMI+光纤+杜比+蓝牙MESH+WIFI投屏+红外学习）
2 - 中端回音壁（基础功能：HDMI+光纤+基础音效，无杜比/WIFI）
3 - 低端蓝牙音箱（极简功能：仅蓝牙+播放+音量，无冗余库）
4 - 独立低音炮（专属功能：蓝牙MESH+低音增益+配对）

## 核心编译参数
### 编译模式
- RELEASE：量产模式（默认）→ O2优化、无调试信息、固件体积最小、稳定可靠
- DEBUG：开发模式 → O0无优化、带调试符号、编译快、便于GDB调试

### 编译平台
- AML_SOC：晶晨交叉编译（默认）→ 量产必选，编译出能在音响芯片运行的固件
- X86_LOCAL：Ubuntu本地编译 → 开发调试用，无需SDK，快速验证代码逻辑

## 一键编译指令（项目顶层目录执行）
### 1. 编译高端回音壁（量产模式）
make PRODUCT=1 TOOLCHAIN_TYPE=AML_SOC BUILD_TYPE=RELEASE

### 2. 编译中端回音壁（量产模式）
make PRODUCT=2 TOOLCHAIN_TYPE=AML_SOC BUILD_TYPE=RELEASE

### 3. 编译低端蓝牙音箱（开发模式）
make PRODUCT=3 TOOLCHAIN_TYPE=X86_LOCAL BUILD_TYPE=DEBUG

### 4. 编译独立低音炮（量产模式）
make PRODUCT=4 TOOLCHAIN_TYPE=AML_SOC BUILD_TYPE=RELEASE

### 5. 清理所有编译产物
make clean

## 输出目录说明
- 静态库文件：lib/output/*.a
- 动态库文件：lib/output/*.so
- 最终固件文件：bin/aml_audio_speaker（单一文件，所有产品通用）

## 常见问题排查
1. 编译报错 `undefined reference to xxx` → 检查lib_deps.mk是否添加对应SDK库，或product_libs.mk是否开启对应产品的库链接
2. 编译报错 `fatal error: aml_audio.h: No such file or directory` → 检查toolchain.mk中的SDK_ROOT路径是否正确
3. 固件运行崩溃 → 量产模式请使用静态库编译，避免动态库依赖缺失
4. 固件体积过大 → 确认产品类型是否正确，低端产品会自动裁剪无用库

## 注意事项
1. 请勿修改本目录下的任何编译规则，所有产品差异化均通过宏定义实现
2. SDK路径请在toolchain.mk中修改SDK_ROOT变量，适配你的实际SDK安装路径
3. 量产时必须使用RELEASE模式+静态库编译，保证固件稳定性和无依赖性