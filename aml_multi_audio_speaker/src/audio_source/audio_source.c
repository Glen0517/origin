#include "audio_source.h"
#include "audio_source_priv.h"
#include "logger.h"
#include "product_type.h"
#include "../storage/storage_priv.h"
#include "event.h"

static AudioSource_t g_audio_src = {0};
static const int g_base_src[] = {AUDIO_SOURCE_BT, AUDIO_SOURCE_USB};

/**
 * @brief 音频源模块初始化
 * @details 初始化所有配置的音频源，设置默认音频源为蓝牙
 * @param cfg 音源配置结构体指针
 * @return 初始化结果：0表示成功，非0表示失败
 */
int audio_source_init(AudioSourceConfig_t *cfg) {
    memset(&g_audio_src, 0, sizeof(AudioSource_t));
    g_audio_src.cur_source = AUDIO_SOURCE_BT;                      // 默认音频源为蓝牙
    g_audio_src.src_count = sizeof(g_base_src)/sizeof(int);        // 计算基础音频源数量
    memcpy(g_audio_src.support_src, g_base_src, sizeof(g_base_src)); // 复制基础音频源列表
    
    // 如果提供了配置，使用配置中的设置
    if (cfg) {
        g_audio_src.auto_switch_en = cfg->auto_switch_en;
    }
    g_audio_src.manual_selected = false;  // 初始化手动选择标志为false

    // 基础音源（必加载）
    src_bt_init();       // 初始化蓝牙音频源
    src_usb_init();      // 初始化USB音频源

    // 宏控加载音源 - 根据产品配置加载相应的音频源
    #ifdef CONFIG_ENABLE_HDMI_ARC
    g_audio_src.support_src[g_audio_src.src_count++] = AUDIO_SOURCE_HDMI;
    src_hdmi_init();     // 初始化HDMI ARC音频源
    #endif
    #ifdef CONFIG_ENABLE_SPDIF
    g_audio_src.support_src[g_audio_src.src_count++] = AUDIO_SOURCE_SPDIF;
    src_spdif_init();    // 初始化SPDIF光纤音频源
    #endif
    #ifdef CONFIG_ENABLE_AUX
    g_audio_src.support_src[g_audio_src.src_count++] = AUDIO_SOURCE_AUX;
    src_aux_init();      // 初始化AUX音频源
    #endif
    #ifdef CONFIG_ENABLE_UAC
    g_audio_src.support_src[g_audio_src.src_count++] = AUDIO_SOURCE_UAC;
    src_uac_init();      // 初始化UAC音频源
    #endif
    #ifdef CONFIG_ENABLE_WIFI_MEDIA
    g_audio_src.support_src[g_audio_src.src_count++] = AUDIO_SOURCE_WIFI;
    src_wifi_init();     // 初始化WiFi媒体音频源（DLNA、AirPlay）
    #endif

    g_audio_src.init_ok = 1;  // 设置初始化完成标志
    LOG_INFO("Audio source init: cur=%d, support=%d", 
             g_audio_src.cur_source, g_audio_src.src_count);
    return 0;
}

int audio_source_deinit(void) {
    if (g_audio_src.init_ok) {
#ifdef CONFIG_ENABLE_WIFI_MEDIA
        src_wifi_deinit();
#endif
#ifdef CONFIG_ENABLE_UAC
        src_uac_deinit();
#endif
#ifdef CONFIG_ENABLE_AUX
        src_aux_deinit();
#endif
#ifdef CONFIG_ENABLE_SPDIF
        src_spdif_deinit();
#endif
#ifdef CONFIG_ENABLE_HDMI_ARC
        src_hdmi_deinit();
#endif
        src_usb_deinit();
        src_bt_deinit();
        g_audio_src.init_ok = 0;
        LOG_INFO("Audio source deinit success");
    }
    return SUCCESS;
}

int audio_source_switch(AudioSourceType_e source) {
    if (!g_audio_src.init_ok) {
        LOG_ERROR("Source switch failed: not initialized");
        return FAILURE;
    }
    
    // 校验是否支持
    int i;
    for (i = 0; i < g_audio_src.src_count; i++) {
        if (g_audio_src.support_src[i] == source) break;
    }
    if (i >= g_audio_src.src_count) {
        LOG_ERROR("Source %d not supported", source);
        return NOT_SUPPORT;
    }
    
    g_audio_src.cur_source = source;
    g_audio_src.manual_selected = true; // 设置手动选择标志
    LOG_INFO("Switch source to: %d (manual selection)", source);
    return SUCCESS;
}

/**
 * @brief 音频源事件轮询
 * @details 定期检查各音频源的状态变化，并处理相应的事件
 * @return 无
 */
void audio_source_event_poll(void) {
    if (!g_audio_src.init_ok) return;
    
    // 轮询各音源状态变化，处理连接/断开事件
    
    // 1. HDMI ARC音频源状态轮询
    #ifdef CONFIG_ENABLE_HDMI_ARC
    int hdmi_status = hdmi_arc_get_status();
    if (hdmi_status != g_audio_src.source_status[AUDIO_SOURCE_HDMI]) {
        g_audio_src.source_status[AUDIO_SOURCE_HDMI] = hdmi_status;
        
        if (hdmi_status) {
            LOG_INFO("HDMI ARC source connected");
            // 处理HDMI ARC连接事件
            led_ctrl_set_state(LED_SOURCE, LED_STATE_BREATH); // 更新LED状态
            lcd_display_text(0, 0, "Source: HDMI ARC");     // 更新LCD显示
            event_notify(EVENT_SOURCE_HDMI_CONNECTED, NULL);  // 发送事件通知
        } else {
            LOG_INFO("HDMI ARC source disconnected");
            // 处理HDMI ARC断开事件
            event_notify(EVENT_SOURCE_HDMI_DISCONNECTED, NULL); // 发送事件通知
        }
    }
    #endif
    
    // 2. SPDIF光纤音频源状态轮询
    #ifdef CONFIG_ENABLE_SPDIF
    int spdif_status = spdif_optical_get_status();
    if (spdif_status != g_audio_src.source_status[AUDIO_SOURCE_SPDIF]) {
        g_audio_src.source_status[AUDIO_SOURCE_SPDIF] = spdif_status;
        
        if (spdif_status) {
            LOG_INFO("SPDIF source connected");
            // 处理SPDIF连接事件
            led_ctrl_set_state(LED_SOURCE, LED_STATE_BREATH); // 更新LED状态
            lcd_display_text(0, 0, "Source: SPDIF");        // 更新LCD显示
            event_notify(EVENT_SOURCE_SPDIF_CONNECTED, NULL); // 发送事件通知
        } else {
            LOG_INFO("SPDIF source disconnected");
            // 处理SPDIF断开事件
            event_notify(EVENT_SOURCE_SPDIF_DISCONNECTED, NULL); // 发送事件通知
        }
    }
    #endif
    
    // 3. 蓝牙音频源状态轮询
    int bt_status = bluetooth_get_connect_state();
    if (bt_status != g_audio_src.source_status[AUDIO_SOURCE_BT]) {
        g_audio_src.source_status[AUDIO_SOURCE_BT] = bt_status;
        
        if (bt_status) {
            LOG_INFO("Bluetooth source connected");
            // 处理蓝牙连接事件
            led_ctrl_set_state(LED_SOURCE, LED_STATE_BREATH); // 更新LED状态
            lcd_display_text(0, 0, "Source: Bluetooth");    // 更新LCD显示
            event_notify(EVENT_SOURCE_BT_CONNECTED, NULL);   // 发送事件通知
        } else {
            LOG_INFO("Bluetooth source disconnected");
            // 处理蓝牙断开事件
            event_notify(EVENT_SOURCE_BT_DISCONNECTED, NULL); // 发送事件通知
        }
    }
    
    // 4. USB音频源状态轮询
    int usb_status = src_usb_get_status();
    if (usb_status != g_audio_src.source_status[AUDIO_SOURCE_USB]) {
        g_audio_src.source_status[AUDIO_SOURCE_USB] = usb_status;
        
        if (usb_status) {
            LOG_INFO("USB source connected");
            // 处理USB连接事件
            led_ctrl_set_state(LED_SOURCE, LED_STATE_BREATH); // 更新LED状态
            lcd_display_text(0, 0, "Source: USB");          // 更新LCD显示
            media_scan_scan_path("/mnt/usb");               // 自动触发媒体扫描
            event_notify(EVENT_SOURCE_USB_CONNECTED, NULL);  // 发送事件通知
        } else {
            LOG_INFO("USB source disconnected");
            // 处理USB断开事件
            file_reader_stop();                              // 停止当前播放的U盘文件
            event_notify(EVENT_SOURCE_USB_DISCONNECTED, NULL); // 发送事件通知
        }
    }
    
    // 5. AUX音频源状态轮询
    int aux_status = src_aux_get_status();
    if (aux_status != g_audio_src.source_status[AUDIO_SOURCE_AUX]) {
        g_audio_src.source_status[AUDIO_SOURCE_AUX] = aux_status;
        
        if (aux_status) {
            LOG_INFO("AUX source connected");
            // 处理AUX连接事件
            led_ctrl_set_state(LED_SOURCE, LED_STATE_BREATH); // 更新LED状态
            lcd_display_text(0, 0, "Source: AUX");          // 更新LCD显示
            event_notify(EVENT_SOURCE_AUX_CONNECTED, NULL);  // 发送事件通知
        } else {
            LOG_INFO("AUX source disconnected");
            // 处理AUX断开事件
            event_notify(EVENT_SOURCE_AUX_DISCONNECTED, NULL); // 发送事件通知
        }
    }
    
    // 6. UAC音频源状态轮询
    int uac_status = src_uac_get_status();
    if (uac_status != g_audio_src.source_status[AUDIO_SOURCE_UAC]) {
        g_audio_src.source_status[AUDIO_SOURCE_UAC] = uac_status;
        
        if (uac_status) {
            LOG_INFO("UAC source connected");
            // 处理UAC连接事件
            led_ctrl_set_state(LED_SOURCE, LED_STATE_BREATH); // 更新LED状态
            lcd_display_text(0, 0, "Source: UAC");          // 更新LCD显示
            event_notify(EVENT_SOURCE_UAC_CONNECTED, NULL);  // 发送事件通知
        } else {
            LOG_INFO("UAC source disconnected");
            // 处理UAC断开事件
            event_notify(EVENT_SOURCE_UAC_DISCONNECTED, NULL); // 发送事件通知
        }
    }
    
    // 7. WiFi媒体音频源状态轮询（DLNA、AirPlay）
    int wifi_status = wifi_media_get_status();
    if (wifi_status != g_audio_src.source_status[AUDIO_SOURCE_WIFI]) {
        g_audio_src.source_status[AUDIO_SOURCE_WIFI] = wifi_status;
        
        if (wifi_status) {
            LOG_INFO("WIFI media source connected");
            // 处理WIFI媒体连接事件
            led_ctrl_set_state(LED_SOURCE, LED_STATE_BREATH); // 更新LED状态
            lcd_display_text(0, 0, "Source: WIFI");         // 更新LCD显示
            event_notify(EVENT_SOURCE_WIFI_CONNECTED, NULL);  // 发送事件通知
        } else {
            LOG_INFO("WIFI media source disconnected");
            // 处理WIFI媒体断开事件
            event_notify(EVENT_SOURCE_WIFI_DISCONNECTED, NULL); // 发送事件通知
        }
    }
    
    // 8. 处理自动音源切换
    // 如果启用了自动切换功能，根据音源优先级自动切换到可用的最高优先级音源
    if (g_audio_src.auto_switch_en) {
        audio_source_auto_switch();
    }
}

AudioSourceType_e audio_source_get_current(void) {
    return g_audio_src.init_ok ? g_audio_src.cur_source : AUDIO_SOURCE_BT;
}

/**
 * @brief 切换到下一个音源(按优先级)
 * @return 切换后的音源类型
 */
AudioSourceType_e audio_source_switch_next(void) {
    if (!g_audio_src.init_ok) {
        return AUDIO_SOURCE_BT;
    }
    
    // 找到当前音源在支持列表中的位置
    int current_idx = -1;
    for (int i = 0; i < g_audio_src.src_count; i++) {
        if (g_audio_src.support_src[i] == g_audio_src.cur_source) {
            current_idx = i;
            break;
        }
    }
    
    // 计算下一个音源的索引
    int next_idx = (current_idx + 1) % g_audio_src.src_count;
    
    // 切换到下一个音源
    audio_source_switch(g_audio_src.support_src[next_idx]);
    
    return g_audio_src.cur_source;
}

/**
 * @brief 音源状态检测
 * @param source 音源类型
 * @return TRUE-有信号 FALSE-无信号
 */
bool audio_source_detect(AudioSourceType_e source) {
    if (!g_audio_src.init_ok) {
        return FALSE;
    }
    
    // 检查音源是否在支持列表中
    int i;
    for (i = 0; i < g_audio_src.src_count; i++) {
        if (g_audio_src.support_src[i] == source) break;
    }
    if (i >= g_audio_src.src_count) {
        LOG_ERROR("Source %d not supported", source);
        return FALSE;
    }
    
    // 返回音源状态
    return g_audio_src.source_status[source] ? TRUE : FALSE;
}

/**
 * @brief 检查当前音源是否正在播放
 * @return 播放状态：true表示正在播放，false表示未播放
 */
static bool is_current_source_playing(void) {
    switch (g_audio_src.cur_source) {
        case AUDIO_SOURCE_BT:
            return src_bt_is_playing();
        case AUDIO_SOURCE_USB:
            return src_usb_is_playing();
        case AUDIO_SOURCE_HDMI:
#ifdef CONFIG_ENABLE_HDMI_ARC
            return src_hdmi_is_playing();
#else
            return false;
#endif
        case AUDIO_SOURCE_SPDIF:
#ifdef CONFIG_ENABLE_SPDIF
            return src_spdif_is_playing();
#else
            return false;
#endif
        case AUDIO_SOURCE_AUX:
#ifdef CONFIG_ENABLE_AUX
            return src_aux_is_playing();
#else
            return false;
#endif
        case AUDIO_SOURCE_UAC:
#ifdef CONFIG_ENABLE_UAC
            return src_uac_is_playing();
#else
            return false;
#endif
        case AUDIO_SOURCE_WIFI:
#ifdef CONFIG_ENABLE_WIFI_MEDIA
            return src_wifi_is_playing();
#else
            return false;
#endif
        default:
            return false;
    }
}

/**
 * @brief 自动音源切换
 */
static void audio_source_auto_switch(void) {
    if (!g_audio_src.init_ok || !g_audio_src.auto_switch_en) {
        return;
    }
    
    // 检查当前音源是否被手动选择，如果是则不自动切换
    if (g_audio_src.manual_selected) {
        LOG_DEBUG("Auto switch disabled: source manually selected");
        return;
    }
    
    // 检查当前音源是否正在播放，如果是则不自动切换
    if (is_current_source_playing()) {
        LOG_DEBUG("Auto switch disabled: current source is playing");
        return;
    }
    
    // 音源优先级列表（从高到低）
    int priority_list[] = {
        AUDIO_SOURCE_HDMI,
        AUDIO_SOURCE_SPDIF,
        AUDIO_SOURCE_UAC,
        AUDIO_SOURCE_USB,
        AUDIO_SOURCE_AUX,
        AUDIO_SOURCE_WIFI,
        AUDIO_SOURCE_BT
    };
    
    int priority_count = sizeof(priority_list) / sizeof(priority_list[0]);
    
    // 查找最高优先级的可用音源
    for (int i = 0; i < priority_count; i++) {
        int source = priority_list[i];
        
        // 检查音源是否可用
        if (g_audio_src.source_status[source]) {
            // 检查是否需要切换
            if (g_audio_src.cur_source != source) {
                LOG_INFO("Auto switch to source: %d (priority %d)", source, i+1);
                audio_source_switch((AudioSourceType_e)source);
            }
            return;
        }
    }
    
    // 如果没有可用音源，保持当前音源
    LOG_DEBUG("No available source for auto switch, keep current: %d", g_audio_src.cur_source);
}