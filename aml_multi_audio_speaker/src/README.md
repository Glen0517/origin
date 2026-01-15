aml_multi_audio_speaker/src/
├── main.c                    # ✅ 项目唯一入口【零修改】，宏控初始化所有模块，业务主循环，无任何业务逻辑
├── audio_core/               # 音频核心：解码/混音/环形缓冲/声道控制，宏控杜比/DTS/多声道
│   ├── audio_core_priv.h     # ✅ 模块私有头文件：内部函数声明、私有结构体，仅模块内引用，对外不可见
│   ├── audio_core.c          # ✅ 模块总入口：初始化/反初始化/对外接口转发，核心调度，无业务逻辑
│   ├── audio_decode.c        # ✅ 单一职责：音频解码（PCM/MP3/AAC）+杜比/DTS解码，宏控裁剪
│   ├── audio_mixer.c         # ✅ 单一职责：音频混音（多音源叠加）+声道控制，产品分级裁剪声道数
│   └── audio_ringbuf.c       # ✅ 单一职责：音频环形缓冲区，通用无宏控，所有产品共用
├── audio_source/             # 音源管理：各音源独立拆分，宏控裁剪HDMI/SPDIF/AUX，仅留蓝牙+U盘
│   ├── audio_source_priv.h   # ✅ 模块私有头文件
│   ├── audio_source.c        # ✅ 模块总入口：音源调度/切换/状态管理，对外接口转发
│   ├── src_bt.c              # ✅ 单一职责：蓝牙音源（A2DP），所有产品必加载
│   ├── src_hdmi.c            # ✅ 单一职责：HDMI ARC音源，宏控编译（仅中/高端）
│   ├── src_spdif.c           # ✅ 单一职责：光纤SPDIF音源，宏控编译（仅中/高端）
│   ├── src_usb.c             # ✅ 单一职责：U盘本地音源，所有产品必加载
│   └── src_aux.c             # ✅ 单一职责：AUX线路输入，宏控编译（仅高端）
├── play_ctrl/                # 播放控制：播放/暂停/切歌/声场模式，宏控裁剪声场/音效
│   ├── play_ctrl_priv.h      # ✅ 模块私有头文件
│   ├── play_ctrl.c           # ✅ 模块总入口
│   ├── play_state.c          # ✅ 单一职责：播放状态机（播放/暂停/停止/切歌），通用无宏控
│   └── sound_field.c         # ✅ 单一职责：声场模式（立体声/全景声），宏控裁剪（仅中/高端）
├── volume_ctrl/              # 音量控制：主音量/声道音量/静音，宏控裁剪三轨/双轨/单轨
│   ├── volume_ctrl_priv.h    # ✅ 模块私有头文件
│   ├── volume_ctrl.c         # ✅ 模块总入口
│   ├── main_volume.c         # ✅ 单一职责：主音量调节/静音，所有产品必加载
│   └── channel_volume.c      # ✅ 单一职责：左/右/低音声道音量，产品分级裁剪轨数
├── peripheral/               # 外设管理：极致拆分，宏控裁剪大屏/红外/麦，仅留LED+物理按键
│   ├── peripheral_priv.h     # ✅ 模块私有头文件
│   ├── peripheral.c          # ✅ 模块总入口
│   ├── key_ir.c              # ✅ 单一职责：物理按键+红外遥控，宏控裁剪红外（仅中/高端）
│   ├── led_ctrl.c            # ✅ 单一职责：LED指示灯控制，所有产品必加载
│   ├── lcd_display.c         # ✅ 单一职责：LCD大屏显示，宏控裁剪（仅中/高端）
│   └── prompt_sound.c        # ✅ 单一职责：提示音播放，调用res接口，产品分级加载资源
├── storage/                  # 存储管理：U盘挂载/文件读取/配置存储，无产品差异，细粒度拆分
│   ├── storage_priv.h        # ✅ 模块私有头文件
│   ├── storage.c             # ✅ 模块总入口
│   ├── usb_mount.c           # ✅ 单一职责：U盘挂载/卸载/热插拔检测
│   └── file_reader.c         # ✅ 单一职责：本地音频文件读取（MP3/WAV）
├── bluetooth/                # 蓝牙模块：拆分A2DP基础功能+MESH组网，宏控裁剪MESH
│   ├── bluetooth_priv.h      # ✅ 模块私有头文件
│   ├── bluetooth.c           # ✅ 模块总入口
│   ├── bt_a2dp.c             # ✅ 单一职责：蓝牙A2DP播放/连接/断开，所有产品必加载
│   └── bt_mesh.c             # ✅ 单一职责：蓝牙MESH组网/低音炮联动，宏控编译（仅高端+低音炮）
├── wifi_media/               # WIFI媒体：无线投屏/网络播放，宏控编译（仅高端加载，无拆分）
│   └── wifi_media.c          # ✅ 单文件，宏控全包裹，中低端完全不编译
├── system/                   # 系统模块：初始化/OTA/重启/版本，宏控裁剪双路OTA
│   ├── system_priv.h         # ✅ 模块私有头文件
│   ├── system.c              # ✅ 模块总入口
│   ├── sys_init.c            # ✅ 单一职责：系统基础初始化，通用无宏控
│   └── sys_ota.c             # ✅ 单一职责：OTA升级，宏控裁剪双路OTA（仅高端）
├── uart_mcu_comm/            # 串口通信：拆分指令收发+功放控制，宏控裁剪指令集
│   ├── uart_mcu_priv.h       # ✅ 模块私有头文件
│   ├── uart_mcu_comm.c       # ✅ 模块总入口
│   ├── uart_txrx.c           # ✅ 单一职责：串口基础收发，所有产品必加载
│   └── amp_ctrl.c            # ✅ 单一职责：功放指令控制，宏控裁剪（仅中/高端）
├── sound_effects/            # 音效模块：杜比/DTS/EQ，宏控编译（仅高端加载，细粒度拆分）
│   ├── sound_effects_priv.h  # ✅ 模块私有头文件
│   ├── sound_effects.c       # ✅ 模块总入口
│   ├── dolby_dts.c           # ✅ 单一职责：杜比/DTS解码增强，宏控编译
│   └── eq_preset.c           # ✅ 单一职责：EQ音效预设，产品分级加载预设数量
├── hdmi_arc/                 # HDMI ARC：单文件，宏控编译（仅中/高端加载）
│   └── hdmi_arc.c            # ✅ 单文件，宏控全包裹
├── spdif_optical/            # 光纤SPDIF：单文件，宏控编译（仅中/高端加载）
│   └── spdif_optical.c       # ✅ 单文件，宏控全包裹
├── subwoofer_comm/           # 低音炮通信：单文件，宏控编译（仅高端+低音炮加载）
│   └── subwoofer_comm.c      # ✅ 单文件，宏控全包裹
└── prod_test/                # 生产测试：拆分自检/校准/老化，宏控裁剪校准/老化
    ├── prod_test_priv.h      # ✅ 模块私有头文件
    ├── prod_test.c           # ✅ 模块总入口
    ├── hw_selfcheck.c        # ✅ 单一职责：基础硬件自检，所有产品必加载
    └── hw_calib.c            # ✅ 单一职责：硬件校准/老化测试，宏控编译（仅高端）