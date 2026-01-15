#include "play_ctrl.h"
#include "play_ctrl_priv.h"
#include "logger.h"

static PlayCtrl_t g_play_cfg = {0};

int play_ctrl_init(void) {
    memset(&g_play_cfg, 0, sizeof(PlayCtrl_t));
    play_state_init();
    eq_effect_init();
#ifdef CONFIG_ENABLE_SOUND_FIELD
    sound_field_init();
#endif
    g_play_cfg.state = PLAY_STATE_STOP;
    g_play_cfg.init_ok = 1;
    LOG_INFO("Play control init success");
    return 0;
}

void play_ctrl_deinit(void) {
    if (g_play_cfg.init_ok) {
#ifdef CONFIG_ENABLE_SOUND_FIELD
        sound_field_deinit();
#endif
        eq_effect_deinit();
        play_state_deinit();
        g_play_cfg.init_ok = 0;
        LOG_INFO("Play control deinit success");
    }
}

void play_ctrl_play(void) {
    if (!g_play_cfg.init_ok) return;
    g_play_cfg.state = PLAY_STATE_PLAY;
    LOG_INFO("Play: start");
}

void play_ctrl_pause(void) {
    if (!g_play_cfg.init_ok) return;
    g_play_cfg.state = PLAY_STATE_PAUSE;
    LOG_INFO("Play: pause");
}

/**
 * @brief 设置声场模式
 */
int play_ctrl_set_sound_mode(SoundMode_e mode) {
    if (!g_play_cfg.init_ok) {
        LOG_ERROR("Play control: not initialized");
        return -1;
    }
    
    return sound_field_set_mode(mode);
}

/**
 * @brief 获取当前声场模式
 */
SoundMode_e play_ctrl_get_sound_mode(void) {
    if (!g_play_cfg.init_ok) {
        LOG_ERROR("Play control: not initialized");
        return SOUND_MODE_NORMAL;
    }
    
    return sound_field_get_mode();
}

void play_ctrl_event_poll(void) {
    if (!g_play_cfg.init_ok) return;
    
    // 轮询播放状态变化
    static PlayState_e last_play_state = PLAY_STATE_STOP;
    PlayState_e current_play_state = play_state_get_current();
    
    if (current_play_state != last_play_state) {
        LOG_INFO("Play state changed: %d -> %d", last_play_state, current_play_state);
        last_play_state = current_play_state;
        
        // 根据播放状态执行相应操作
        switch (current_play_state) {
            case PLAY_STATE_PLAYING:
                // 处理播放开始事件
                // 更新LED状态
                led_ctrl_set_state(LED_PLAY, LED_STATE_ON);
                // 更新LCD显示
                lcd_display_text(1, 0, "Playing");
                // 发送事件给其他模块
                event_notify(EVENT_PLAY_START, NULL);
                break;
            case PLAY_STATE_PAUSED:
                // 处理播放暂停事件
                // 更新LED状态
                led_ctrl_set_state(LED_PLAY, LED_STATE_FLASH_SLOW);
                // 更新LCD显示
                lcd_display_text(1, 0, "Paused");
                // 发送事件给其他模块
                event_notify(EVENT_PLAY_PAUSE, NULL);
                break;
            case PLAY_STATE_STOP:
                // 处理播放停止事件
                // 更新LED状态
                led_ctrl_set_state(LED_PLAY, LED_STATE_OFF);
                // 更新LCD显示
                lcd_display_text(1, 0, "Stopped");
                // 发送事件给其他模块
                event_notify(EVENT_PLAY_STOP, NULL);
                break;
            case PLAY_STATE_ERROR:
                // 处理播放错误事件
                // 更新LED状态
                led_ctrl_set_state(LED_PLAY, LED_STATE_FLASH_FAST);
                // 更新LCD显示
                lcd_display_text(1, 0, "Play Error");
                // 发送事件给其他模块
                event_notify(EVENT_PLAY_ERROR, NULL);
                break;
            default:
                break;
        }
    }
    
    // 轮询EQ效果变化
    static int last_eq_mode = 0;
    int current_eq_mode = eq_effect_get_mode();
    
    if (current_eq_mode != last_eq_mode) {
        LOG_INFO("EQ mode changed: %d -> %d", last_eq_mode, current_eq_mode);
        last_eq_mode = current_eq_mode;
        // 处理EQ模式变化事件
        // 更新LCD显示
        char lcd_msg[32] = {0};
        snprintf(lcd_msg, sizeof(lcd_msg), "EQ: %d", current_eq_mode);
        lcd_display_text(1, 8, lcd_msg);
        // 发送事件给其他模块
        event_notify(EVENT_EQ_CHANGED, &current_eq_mode);
    }
    
    // 轮询声场模式变化
    static int last_sound_field = 0;
    int current_sound_field = sound_field_get_mode();
    
    if (current_sound_field != last_sound_field) {
        LOG_INFO("Sound field mode changed: %d -> %d", last_sound_field, current_sound_field);
        last_sound_field = current_sound_field;
        // 处理声场模式变化事件
        // 更新LCD显示
        char lcd_msg[32] = {0};
        snprintf(lcd_msg, sizeof(lcd_msg), "SF: %d", current_sound_field);
        lcd_display_text(1, 8, lcd_msg);
        // 发送事件给其他模块
        event_notify(EVENT_SOUND_FIELD_CHANGED, &current_sound_field);
    }
    
    // 轮询播放进度
    static uint64_t last_pos = 0;
    uint64_t current_pos = play_state_get_position();
    
    if (current_pos != last_pos) {
        // 每1秒更新一次进度日志
        if (current_pos - last_pos > 1000) {
            LOG_DEBUG("Play position: %llu ms", current_pos);
            last_pos = current_pos;
        }
    }
    
    // 轮询播放缓冲状态
    static int last_buffer_status = 0;
    int current_buffer_status = play_state_get_buffer_status();
    
    if (current_buffer_status != last_buffer_status) {
        LOG_INFO("Buffer status changed: %d -> %d", last_buffer_status, current_buffer_status);
        last_buffer_status = current_buffer_status;
        
        // 处理缓冲状态变化事件
        if (current_buffer_status < 0) {
            // 缓冲错误
            led_ctrl_set_state(LED_BUFFER, LED_STATE_FLASH_FAST);
            lcd_display_text(1, 12, "Buf Err");
            event_notify(EVENT_BUFFER_ERROR, NULL);
        } else if (current_buffer_status < 100) {
            // 正在缓冲中
            led_ctrl_set_state(LED_BUFFER, LED_STATE_BREATH);
            char lcd_msg[8] = {0};
            snprintf(lcd_msg, sizeof(lcd_msg), "Buf: %d%%", current_buffer_status);
            lcd_display_text(1, 12, lcd_msg);
            event_notify(EVENT_BUFFERING, &current_buffer_status);
        } else {
            // 缓冲完成
            led_ctrl_set_state(LED_BUFFER, LED_STATE_OFF);
            lcd_display_text(1, 12, "     ");
            event_notify(EVENT_BUFFER_COMPLETE, NULL);
        }
    }
}