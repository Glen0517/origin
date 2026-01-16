# HAL和PAL层架构设计文档

## 1. 架构概述

### 1.1 设计目标

设计并实现HAL（硬件抽象层）和PAL（平台抽象层），以：

1. **提高代码可移植性**：隐藏底层硬件和平台差异，使业务逻辑与具体实现分离
2. **增强代码可维护性**：统一硬件访问接口，简化业务应用层的代码结构
3. **支持多平台适配**：通过抽象层实现，便于在不同硬件平台上快速移植
4. **优化开发效率**：提供统一的接口，减少重复代码，提高开发效率

### 1.2 架构层次

在现有系统架构的基础上，添加HAL和PAL层：

```
+-------------------------+
|      业务应用层          |
+-------------------------+
|        PAL层            |  平台抽象层，封装平台相关服务
+-------------------------+
|        HAL层            |  硬件抽象层，封装硬件相关操作
+-------------------------+
|      Amlogic SDK层      |  晶晨SDK，提供底层硬件访问
+-------------------------+
|        硬件层            |  物理硬件设备
+-------------------------+
```

## 2. 目录结构设计

### 2.1 HAL层目录结构

```
src/
└── hal/                 # 硬件抽象层
    ├── include/         # HAL层头文件
    │   ├── hal.h        # HAL层主头文件
    │   ├── hal_audio.h  # 音频硬件抽象接口
    │   ├── hal_bt.h     # 蓝牙硬件抽象接口
    │   ├── hal_usb.h    # USB硬件抽象接口
    │   └── hal_peri.h   # 外设硬件抽象接口
    └── src/             # HAL层实现文件
        ├── hal_audio.c  # 音频硬件抽象实现
        ├── hal_bt.c     # 蓝牙硬件抽象实现
        ├── hal_usb.c    # USB硬件抽象实现
        ├── hal_peri.c   # 外设硬件抽象实现
        └── Makefile     # HAL层编译配置
```

### 2.2 PAL层目录结构

```
src/
└── pal/                 # 平台抽象层
    ├── include/         # PAL层头文件
    │   ├── pal.h        # PAL层主头文件
    │   ├── pal_system.h # 系统服务抽象接口
    │   ├── pal_storage.h # 存储服务抽象接口
    │   └── pal_network.h # 网络服务抽象接口
    └── src/             # PAL层实现文件
        ├── pal_system.c # 系统服务抽象实现
        ├── pal_storage.c # 存储服务抽象实现
        ├── pal_network.c # 网络服务抽象实现
        └── Makefile     # PAL层编译配置
```

## 3. 模块划分与接口定义

### 3.1 HAL层模块

#### 3.1.1 音频模块 (hal_audio)

**功能**：封装Amlogic音频SDK的功能，提供统一的音频硬件访问接口。

**主要接口**：

| 函数名 | 功能描述 | 参数 | 返回值 |
|-------|---------|------|-------|
| `hal_audio_init` | 初始化音频硬件 | 无 | 0表示成功，非0表示失败 |
| `hal_audio_deinit` | 反初始化音频硬件 | 无 | 0表示成功，非0表示失败 |
| `hal_audio_set_sample_rate` | 设置音频采样率 | sample_rate: 采样率值 | 0表示成功，非0表示失败 |
| `hal_audio_set_channels` | 设置音频通道数 | channels: 通道数 | 0表示成功，非0表示失败 |
| `hal_audio_play_pcm` | 播放PCM音频数据 | data: 音频数据指针<br>len: 数据长度 | 0表示成功，非0表示失败 |
| `hal_audio_stop` | 停止音频播放 | 无 | 0表示成功，非0表示失败 |
| `hal_audio_get_status` | 获取音频播放状态 | 无 | 播放状态值 |

#### 3.1.2 蓝牙模块 (hal_bt)

**功能**：封装Amlogic蓝牙SDK的功能，提供统一的蓝牙硬件访问接口。

**主要接口**：

| 函数名 | 功能描述 | 参数 | 返回值 |
|-------|---------|------|-------|
| `hal_bt_init` | 初始化蓝牙硬件 | 无 | 0表示成功，非0表示失败 |
| `hal_bt_deinit` | 反初始化蓝牙硬件 | 无 | 0表示成功，非0表示失败 |
| `hal_bt_get_connection_status` | 获取蓝牙连接状态 | 无 | 1表示已连接，0表示未连接 |
| `hal_bt_a2dp_get_media_status` | 获取蓝牙A2DP媒体状态 | 无 | 1表示正在播放，0表示停止 |
| `hal_bt_event_poll` | 轮询蓝牙事件 | 无 | 0表示成功，非0表示失败 |

#### 3.1.3 USB模块 (hal_usb)

**功能**：封装Amlogic USB SDK的功能，提供统一的USB硬件访问接口。

**主要接口**：

| 函数名 | 功能描述 | 参数 | 返回值 |
|-------|---------|------|-------|
| `hal_usb_init` | 初始化USB硬件 | 无 | 0表示成功，非0表示失败 |
| `hal_usb_deinit` | 反初始化USB硬件 | 无 | 0表示成功，非0表示失败 |
| `hal_usb_audio_open` | 打开USB音频设备 | dev_path: 设备路径 | 0表示成功，非0表示失败 |
| `hal_usb_audio_close` | 关闭USB音频设备 | 无 | 0表示成功，非0表示失败 |
| `hal_usb_audio_set_device_callback` | 设置USB设备连接状态回调 | callback: 回调函数指针 | 0表示成功，非0表示失败 |
| `hal_usb_audio_set_data_callback` | 设置USB音频数据接收回调 | callback: 回调函数指针 | 0表示成功，非0表示失败 |
| `hal_usb_audio_enable_detection` | 启用/禁用USB音频设备检测 | enable: true/false | 0表示成功，非0表示失败 |

#### 3.1.4 外设模块 (hal_peri)

**功能**：封装Amlogic GPIO、PWM等SDK的功能，提供统一的外设硬件访问接口。

**主要接口**：

| 函数名 | 功能描述 | 参数 | 返回值 |
|-------|---------|------|-------|
| `hal_peri_init` | 初始化外设硬件 | 无 | 0表示成功，非0表示失败 |
| `hal_peri_deinit` | 反初始化外设硬件 | 无 | 0表示成功，非0表示失败 |
| `hal_gpio_set_value` | 设置GPIO引脚值 | pin: GPIO引脚号<br>value: 引脚值(0/1) | 0表示成功，非0表示失败 |
| `hal_gpio_get_value` | 获取GPIO引脚值 | pin: GPIO引脚号 | 引脚值(0/1)，失败返回-1 |
| `hal_pwm_set_duty` | 设置PWM占空比 | channel: PWM通道<br>duty: 占空比(0-100) | 0表示成功，非0表示失败 |
| `hal_pwm_set_frequency` | 设置PWM频率 | channel: PWM通道<br>freq: 频率值 | 0表示成功，非0表示失败 |

### 3.2 PAL层模块

#### 3.2.1 系统服务模块 (pal_system)

**功能**：封装系统相关的服务和功能，提供统一的系统服务接口。

**主要接口**：

| 函数名 | 功能描述 | 参数 | 返回值 |
|-------|---------|------|-------|
| `pal_system_init` | 初始化系统服务 | 无 | 0表示成功，非0表示失败 |
| `pal_system_deinit` | 反初始化系统服务 | 无 | 0表示成功，非0表示失败 |
| `pal_system_get_time` | 获取系统时间 | time: 时间结构体指针 | 0表示成功，非0表示失败 |
| `pal_system_sleep` | 系统睡眠 | ms: 睡眠时长(毫秒) | 0表示成功，非0表示失败 |
| `pal_system_get_cpu_usage` | 获取CPU使用率 | usage: 使用率指针(0-100) | 0表示成功，非0表示失败 |
| `pal_system_get_memory_usage` | 获取内存使用率 | usage: 使用率指针(0-100) | 0表示成功，非0表示失败 |

#### 3.2.2 存储服务模块 (pal_storage)

**功能**：封装存储相关的服务和功能，提供统一的存储服务接口。

**主要接口**：

| 函数名 | 功能描述 | 参数 | 返回值 |
|-------|---------|------|-------|
| `pal_storage_init` | 初始化存储服务 | 无 | 0表示成功，非0表示失败 |
| `pal_storage_deinit` | 反初始化存储服务 | 无 | 0表示成功，非0表示失败 |
| `pal_storage_mount` | 挂载存储设备 | dev_path: 设备路径<br>mount_point: 挂载点 | 0表示成功，非0表示失败 |
| `pal_storage_unmount` | 卸载存储设备 | mount_point: 挂载点 | 0表示成功，非0表示失败 |
| `pal_storage_get_free_space` | 获取存储设备可用空间 | path: 路径<br>free_space: 可用空间指针(字节) | 0表示成功，非0表示失败 |
| `pal_storage_scan_media` | 扫描媒体文件 | path: 扫描路径<br>callback: 扫描回调函数 | 0表示成功，非0表示失败 |

#### 3.2.3 网络服务模块 (pal_network)

**功能**：封装网络相关的服务和功能，提供统一的网络服务接口。

**主要接口**：

| 函数名 | 功能描述 | 参数 | 返回值 |
|-------|---------|------|-------|
| `pal_network_init` | 初始化网络服务 | 无 | 0表示成功，非0表示失败 |
| `pal_network_deinit` | 反初始化网络服务 | 无 | 0表示成功，非0表示失败 |
| `pal_network_get_ip_address` | 获取IP地址 | interface: 网络接口名<br>ip: IP地址缓冲区 | 0表示成功，非0表示失败 |
| `pal_network_is_connected` | 检查网络连接状态 | interface: 网络接口名 | 1表示已连接，0表示未连接 |
| `pal_network_start_dhcp` | 启动DHCP服务 | interface: 网络接口名 | 0表示成功，非0表示失败 |
| `pal_network_stop_dhcp` | 停止DHCP服务 | interface: 网络接口名 | 0表示成功，非0表示失败 |

## 4. 实现方案

### 4.1 HAL层实现

HAL层的实现将直接调用Amlogic SDK的函数，实现硬件抽象。以音频模块为例：

```c
// hal_audio.c
#include "hal_audio.h"
#include "logger.h"
#include <aml_audio.h>

int hal_audio_init(void) {
    if (aml_audio_init() != 0) {
        LOG_ERROR("HAL audio init failed");
        return FAILURE;
    }
    LOG_INFO("HAL audio init success");
    return SUCCESS;
}

int hal_audio_set_sample_rate(int sample_rate) {
    return aml_audio_set_sample_rate(sample_rate);
}

// 其他函数实现...
```

### 4.2 PAL层实现

PAL层的实现将调用系统API或封装HAL层的功能，实现平台抽象。以系统服务模块为例：

```c
// pal_system.c
#include "pal_system.h"
#include "logger.h"

int pal_system_init(void) {
    LOG_INFO("PAL system init success");
    return SUCCESS;
}

int pal_system_sleep(int ms) {
    usleep(ms * 1000);
    return SUCCESS;
}

// 其他函数实现...
```

### 4.3 业务应用层适配

业务应用层将通过HAL和PAL层的接口访问硬件和平台服务，而不是直接调用SDK函数。以audio_core模块为例：

```c
// 修改前：直接调用SDK
if (aml_audio_init() != 0) {
    LOG_ERROR("Amlogic audio SDK init failed");
    return FAILURE;
}

// 修改后：通过HAL层调用
if (hal_audio_init() != 0) {
    LOG_ERROR("HAL audio init failed");
    return FAILURE;
}
```

## 5. 编译配置

### 5.1 HAL层编译配置

在`src/hal/src/Makefile`中添加编译规则：

```makefile
# HAL层编译配置

HAL_INCLUDE = -I../include

HAL_OBJS = \
    hal_audio.o \
    hal_bt.o \
    hal_usb.o \
    hal_peri.o

%.o: %.c
    $(CC) $(CFLAGS) $(HAL_INCLUDE) -c $< -o $@

libhal.a: $(HAL_OBJS)
    $(AR) rcs $@ $^

clean:
    rm -f $(HAL_OBJS) libhal.a
```

### 5.2 PAL层编译配置

在`src/pal/src/Makefile`中添加编译规则：

```makefile
# PAL层编译配置

PAL_INCLUDE = -I../include -I../../hal/include

PAL_OBJS = \
    pal_system.o \
    pal_storage.o \
    pal_network.o

%.o: %.c
    $(CC) $(CFLAGS) $(PAL_INCLUDE) -c $< -o $@

libpal.a: $(PAL_OBJS)
    $(AR) rcs $@ $^

clean:
    rm -f $(PAL_OBJS) libpal.a
```

### 5.3 主Makefile修改

修改主Makefile，添加HAL和PAL层的编译：

```makefile
# 添加HAL和PAL层的编译

SUBDIRS += src/hal/src
SUBDIRS += src/pal/src

# 链接库
LIBS += -Lsrc/hal/src -lhal
LIBS += -Lsrc/pal/src -lpal

# 包含路径
INCLUDES += -Isrc/hal/include
INCLUDES += -Isrc/pal/include
```

## 6. 迁移计划

### 6.1 迁移步骤

1. **创建HAL和PAL层目录结构**：按照设计的目录结构创建相应的文件和目录。

2. **实现HAL层核心功能**：先实现音频、蓝牙、USB等核心模块的HAL层功能。

3. **实现PAL层核心功能**：实现系统服务、存储服务等核心模块的PAL层功能。

4. **修改业务应用层代码**：逐步修改业务应用层代码，使用HAL和PAL层的接口替代直接SDK调用。

5. **测试验证**：在每一步修改后进行测试，确保系统功能正常运行。

6. **优化完善**：根据测试结果，优化和完善HAL和PAL层的实现。

### 6.2 迁移优先级

1. **高优先级**：音频核心、蓝牙、USB等核心功能模块
2. **中优先级**：外设管理、存储管理等功能模块
3. **低优先级**：网络服务、系统服务等辅助功能模块

## 7. 预期收益

1. **提高代码可移植性**：通过HAL和PAL层的抽象，系统可以更容易地适配不同的硬件平台和操作系统。

2. **增强代码可维护性**：统一的接口设计使代码结构更清晰，便于维护和调试。

3. **降低开发成本**：通过复用HAL和PAL层的代码，减少重复开发工作，提高开发效率。

4. **提高系统稳定性**：标准化的接口和错误处理机制，减少系统的不确定性，提高系统稳定性。

5. **便于功能扩展**：模块化的设计使系统更容易添加新功能和支持新硬件。

## 8. 风险评估

### 8.1 潜在风险

1. **性能开销**：添加HAL和PAL层可能会增加少量的性能开销。

2. **开发工作量**：需要修改大量现有代码，迁移工作量较大。

3. **兼容性问题**：修改后的代码可能存在兼容性问题，需要充分测试。

### 8.2 风险缓解措施

1. **性能优化**：在实现HAL和PAL层时，尽量减少不必要的开销，保持代码的高效性。

2. **分阶段迁移**：采用分阶段迁移的方式，逐步修改代码，降低风险。

3. **充分测试**：在每一步修改后进行充分的测试，确保系统功能正常运行。

4. **代码审查**：对修改的代码进行严格的代码审查，确保代码质量。

## 9. 总结

通过添加HAL和PAL层，可以显著提高系统的可移植性、可维护性和可扩展性。虽然需要一定的开发工作量，但从长期来看，这些投入是值得的。HAL和PAL层的设计和实现将为系统的后续发展奠定坚实的基础。