 # 【资源目录】静态资源文件，无需编译
# aml_multi_audio_speaker - res/目录资源管理器说明文档
## 核心定位
本目录为项目**统一资源管理中心**，纯工具层无业务代码，负责所有静态资源（提示音、字体、EQ、图标）的加载、缓存、释放、校验；所有资源加载规则与4类产品分级+模块宏控裁剪深度绑定，做到「低端极简、高端完整、模块适配、容错兜底」。
资源管理器对src业务层零侵入，src仅需引入头文件即可调用标准化接口，无需修改任何业务逻辑。

## 核心特性
1. **分级资源加载**：低端/低音炮加载极简资源（体积最小），中/高端加载全量资源，自动适配无冗余；
2. **模块联动裁剪**：HDMI/SPDIF/杜比等模块关闭，对应资源自动不加载，无任何冗余代码；
3. **缓存优化**：资源加载一次缓存到内存，避免重复读取FLASH，提升嵌入式设备响应速度；
4. **容错兜底**：资源缺失时自动加载公共默认资源，保证程序不崩溃，量产稳定性拉满；
5. **标准化调用**：src业务层仅需调用接口，无需关心资源路径/格式/大小，开发效率极高。

## 资源调用方式（src业务层唯一操作，极简！完美匹配项目核心认知）
### 1. 资源调用前置条件
在src任意业务文件中，仅需引入资源管理器头文件，无其他依赖：
```c
#include "res_manager.h"

int main(int argc, char *argv[])
{
    log_system_init();        // 先初始化日志
    res_manager_init();       // 再初始化资源管理器
    // ...其他业务模块初始化
    // 业务逻辑执行
    res_manager_deinit();     // 程序退出前反初始化资源管理器
    log_system_deinit();      // 再反初始化日志
    return 0;
}

// 加载开机提示音
ResInfo_t boot_tone = res_load_resource(RES_TYPE_TONE_BOOT);
if (boot_tone.res_valid)
{
    // 播放提示音：使用boot_tone.res_data和boot_tone.res_size
    audio_play_tone(boot_tone.res_data, boot_tone.res_size);
}

// 加载杜比EQ配置（仅高端产品生效，低端自动加载默认EQ）
ResInfo_t dolby_eq = res_load_resource(RES_TYPE_EQ_DOLBY);
if (dolby_eq.res_valid)
{
    // 解析EQ配置：使用dolby_eq.res_data
    eq_parse_config(dolby_eq.res_data);
}

res_free_resource(&boot_tone);

产品资源适配规则
高端音箱 (PRODUCT_HIGH_END)：加载全量资源，含杜比 EQ、高清字体、所有图标、所有提示音；
中端音箱 (PRODUCT_MID_END)：加载基础资源 + 摇滚 / 古典 EQ、HDMI / 光纤图标、蓝牙提示音；
低端音箱 (PRODUCT_LOW_END)：仅加载开机 / 错误提示音、默认字体、默认 EQ、蓝牙 / U 盘图标；
低音炮 (PRODUCT_SUBWOOFER)：仅加载基础提示音、默认字体、低音增强 EQ、蓝牙图标。