res/assets/
├── common/               # 所有产品通用公共资源【必加载，无产品差异】
│   ├── tone/             # 公共提示音：开机、错误、音量调节（所有产品都用）
│   │   ├── boot.wav
│   │   ├── error.wav
│   │   ├── vol_up.wav
│   │   └── vol_down.wav
│   ├── font/             # 公共基础字体
│   │   └── default_font.bin
│   ├── eq/               # 公共默认EQ配置
│   │   └── default_eq.json
│   └── icon/             # 公共图标：蓝牙、U盘
│       ├── bt_icon.bin
│       └── usb_icon.bin
├── high_end/             # 高端音箱专属资源【全量，体积稍大】
│   ├── tone/             # 全量提示音：音源切换、蓝牙连接/断开
│   │   ├── bt_connect.wav
│   │   ├── bt_disconnect.wav
│   │   └── src_switch.wav
│   ├── font/             # 高清显示字体
│   │   └── hd_font.bin
│   ├── eq/               # 全量EQ配置：摇滚、古典、杜比、默认
│   │   ├── rock_eq.json
│   │   ├── classic_eq.json
│   │   └── dolby_eq.json
│   └── icon/             # 全量图标：HDMI、光纤、蓝牙、U盘
│       ├── hdmi_icon.bin
│       └── spdif_icon.bin
├── mid_end/              # 中端音箱专属资源【基础+部分音效，体积适中】
│   ├── tone/             # 基础提示音+蓝牙连接/断开
│   │   ├── bt_connect.wav
│   │   └── bt_disconnect.wav
│   ├── eq/               # 基础EQ配置：摇滚、古典、默认
│   │   ├── rock_eq.json
│   │   └── classic_eq.json
│   └── icon/             # 图标：HDMI、光纤、蓝牙、U盘
│       ├── hdmi_icon.bin
│       └── spdif_icon.bin
├── low_end/              # 低端音箱专属资源【极简，体积最小化，核心优化】
│   ├── tone/             # 仅保留开机、错误提示音，无其他冗余
│   │   ├── boot.wav
│   │   └── error.wav
│   ├── font/             # 极简基础字体（体积最小）
│   │   └── default_font.bin
│   └── eq/               # 仅保留默认EQ配置
│       └── default_eq.json
└── subwoofer/            # 低音炮专属资源【极简+低音增强，无大屏资源】
    ├── tone/             # 基础提示音+蓝牙MESH连接提示音
    │   ├── boot.wav
    │   └── error.wav
    └── eq/               # 低音增强EQ配置
        └── bass_eq.json