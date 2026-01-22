/**
 * @file remote_control.c
 * @brief 远程控制模块实现
 * @details 实现远程控制模块的核心功能，包括按键事件处理和红外学习
 * @author AML Audio Team
 * @date 2026-01-22
 */

#include "remote_control_priv.h"
#include "logger.h"
#include "product_type.h"
#include "common_def.h"

#include "comm_mcu.h"        // MCU通信接口
#include "event.h"           // 事件系统接口

/******************************************************************************************
 * 远程控制模块全局变量
 ******************************************************************************************/

bool g_remote_control_init = false;
bool g_ir_enabled = false;
bool g_ir_learning = false;
KeyEvent_e g_last_key_event = KEY_EVENT_NONE;
IrCode_t g_ir_codes[MAX_IR_CODES];
int g_ir_code_count = 0;

/******************************************************************************************
 * 远程控制模块内部函数
 ******************************************************************************************/

/**
 * @brief 保存红外码配置到文件
 * @return 操作结果：0表示成功，非0表示失败
 */
static int save_ir_codes_to_file(void) {
    FILE *fp = fopen(IR_CODE_CONFIG_FILE, "w");
    if (!fp) {
        LOG_ERROR("Failed to open IR code config file for writing: %s", strerror(errno));
        return -1;
    }
    
    fprintf(fp, "# IR Code Configuration File\n");
    fprintf(fp, "# Format: IR_CODE KEY_EVENT REMOTE_NAME KEY_NAME\n\n");
    
    for (int i = 0; i < g_ir_code_count; i++) {
        fprintf(fp, "%u %d %s %s\n", 
                g_ir_codes[i].ir_code, 
                g_ir_codes[i].key_event, 
                g_ir_codes[i].remote_name, 
                g_ir_codes[i].key_name);
    }
    
    fclose(fp);
    LOG_INFO("Saved %d IR codes to config file", g_ir_code_count);
    return 0;
}

/**
 * @brief 从文件加载红外码配置
 * @return 操作结果：0表示成功，非0表示失败
 */
static int load_ir_codes_from_file(void) {
    FILE *fp = fopen(IR_CODE_CONFIG_FILE, "r");
    if (!fp) {
        LOG_WARN("IR code config file not found, using default codes");
        return -1;
    }
    
    char line[256];
    int count = 0;
    
    while (fgets(line, sizeof(line), fp) && count < MAX_IR_CODES) {
        // 跳过注释和空行
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        
        // 解析行数据
        uint32_t ir_code;
        int key_event;
        char remote_name[64];
        char key_name[32];
        
        if (sscanf(line, "%u %d %s %s", &ir_code, &key_event, remote_name, key_name) == 4) {
            g_ir_codes[count].ir_code = ir_code;
            g_ir_codes[count].key_event = (KeyEvent_e)key_event;
            strncpy(g_ir_codes[count].remote_name, remote_name, sizeof(g_ir_codes[count].remote_name) - 1);
            strncpy(g_ir_codes[count].key_name, key_name, sizeof(g_ir_codes[count].key_name) - 1);
            count++;
        }
    }
    
    fclose(fp);
    g_ir_code_count = count;
    LOG_INFO("Loaded %d IR codes from config file", g_ir_code_count);
    return 0;
}

/**
 * @brief 添加默认红外码
 */
static void add_default_ir_codes(void) {
    // 添加一些常用的默认红外码
    static const IrCode_t default_codes[] = {
        {0x00FF00FF, KEY_EVENT_PLAY_PAUSE, "DEFAULT", "PLAY_PAUSE"},
        {0x00FF11EE, KEY_EVENT_VOL_UP, "DEFAULT", "VOL_UP"},
        {0x00FF22DD, KEY_EVENT_VOL_DOWN, "DEFAULT", "VOL_DOWN"},
        {0x00FF33CC, KEY_EVENT_SOURCE_SWITCH, "DEFAULT", "SOURCE_SWITCH"},
        {0x00FF44BB, KEY_EVENT_SOUND_MODE, "DEFAULT", "SOUND_MODE"},
        {0x00FF55AA, KEY_EVENT_BASS_UP, "DEFAULT", "BASS_UP"},
        {0x00FF6699, KEY_EVENT_TREBLE_UP, "DEFAULT", "TREBLE_UP"},
        {0x00FF7788, KEY_EVENT_IR_LEARN, "DEFAULT", "IR_LEARN"},
        {0x00FF8877, KEY_EVENT_NEXT, "DEFAULT", "NEXT"},
        {0x00FF9966, KEY_EVENT_PREV, "DEFAULT", "PREV"}
    };
    
    int default_count = sizeof(default_codes) / sizeof(default_codes[0]);
    
    for (int i = 0; i < default_count && g_ir_code_count < MAX_IR_CODES; i++) {
        g_ir_codes[g_ir_code_count] = default_codes[i];
        g_ir_code_count++;
    }
    
    LOG_INFO("Added %d default IR codes", default_count);
}

/******************************************************************************************
 * 远程控制模块对外接口实现
 ******************************************************************************************/

/**
 * @brief 初始化远程控制模块
 * @return 初始化结果：0表示成功，非0表示失败
 */
int remote_control_init(void) {
    if (g_remote_control_init) {
        LOG_INFO("Remote control already initialized");
        return SUCCESS;
    }
    
    g_remote_control_init = true;
    g_ir_enabled = false;
    g_ir_learning = false;
    g_last_key_event = KEY_EVENT_NONE;
    g_ir_code_count = 0;
    
    // 初始化红外码数组
    memset(g_ir_codes, 0, sizeof(g_ir_codes));
    
#if (CURRENT_PRODUCT_TYPE == PRODUCT_LOW_END)
    // 低端产品：仅保留物理按键，关闭红外
    LOG_INFO("Remote control: Key init success (IR disable)");
    
#elif (CURRENT_PRODUCT_TYPE == PRODUCT_MID_END) || (CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END)
    // 中/高端产品：物理按键+红外遥控全开
    LOG_INFO("Remote control: Key + IR remote init success");
    g_ir_enabled = true;
    
    // 加载红外码配置
    if (load_ir_codes_from_file() < 0) {
        // 如果加载失败，添加默认红外码
        add_default_ir_codes();
    }
    
#else
    // 低音炮产品：无按键无红外
    LOG_INFO("Remote control: Key/IR not supported for current product");
    g_remote_control_init = false;
    return SUCCESS;
#endif
    
    LOG_INFO("Remote control module init success");
    return SUCCESS;
}

/**
 * @brief 反初始化远程控制模块
 */
void remote_control_deinit(void) {
    if (!g_remote_control_init) {
        return;
    }
    
    // 停止红外学习
    if (g_ir_learning) {
        if (remote_control_ir_learn_stop() != 0) {
            LOG_ERROR("Failed to stop IR learning");
        }
    }
    
    // 保存红外码配置
    if (g_ir_enabled && g_ir_code_count > 0) {
        if (save_ir_codes_to_file() < 0) {
            LOG_ERROR("Failed to save IR codes");
        }
    }
    
    g_remote_control_init = false;
    g_last_key_event = KEY_EVENT_NONE;
    
    LOG_INFO("Remote control module deinitialized");
}

/**
 * @brief 处理按键事件
 * @param event 按键事件
 * @return 处理结果：0表示成功，非0表示失败
 */
int remote_control_process_key_event(KeyEvent_e event) {
    if (!g_remote_control_init) {
        LOG_ERROR("Remote control not initialized");
        return -1;
    }
    
    LOG_INFO("Processing key event: %d", event);
    
    // 发送事件通知
    event_notify(EVENT_KEY_PRESSED, &event);
    
    // 根据事件类型执行相应操作
    switch (event) {
        case KEY_EVENT_PLAY_PAUSE:
            LOG_INFO("Play/Pause key pressed");
            // 这里可以添加播放/暂停操作
            break;
        case KEY_EVENT_VOL_UP:
            LOG_INFO("Volume up key pressed");
            // 这里可以添加音量增加操作
            break;
        case KEY_EVENT_VOL_DOWN:
            LOG_INFO("Volume down key pressed");
            // 这里可以添加音量减少操作
            break;
        case KEY_EVENT_SOURCE_SWITCH:
            LOG_INFO("Source switch key pressed");
            // 这里可以添加音源切换操作
            break;
        case KEY_EVENT_SOUND_MODE:
            LOG_INFO("Sound mode key pressed");
            // 这里可以添加音效模式切换操作
            break;
        case KEY_EVENT_BASS_UP:
            LOG_INFO("Bass up key pressed");
            // 这里可以添加低音增加操作
            break;
        case KEY_EVENT_TREBLE_UP:
            LOG_INFO("Treble up key pressed");
            // 这里可以添加高音增加操作
            break;
        case KEY_EVENT_IR_LEARN:
            LOG_INFO("IR learn key pressed");
            // 这里可以添加红外学习操作
            if (g_ir_learning) {
                remote_control_ir_learn_stop();
            } else {
                remote_control_ir_learn_start();
            }
            break;
        case KEY_EVENT_NEXT:
            LOG_INFO("Next key pressed");
            // 这里可以添加下一首操作
            break;
        case KEY_EVENT_PREV:
            LOG_INFO("Previous key pressed");
            // 这里可以添加上一首操作
            break;
        default:
            LOG_WARN("Unknown key event: %d", event);
            break;
    }
    
    return SUCCESS;
}

/**
 * @brief 开始红外学习
 * @return 操作结果：0表示成功，非0表示失败
 */
int remote_control_ir_learn_start(void) {
    if (!g_remote_control_init || !g_ir_enabled) {
        LOG_ERROR("IR learning not supported");
        return -1;
    }
    
    if (g_ir_learning) {
        LOG_INFO("IR learning already started");
        return 0;
    }
    
    // 发送红外学习开始命令给MCU
    uint8_t data = 1;
    if (uart_mcu_send_cmd(CMD_START_IR_LEARN, &data, 1) < 0) {
        LOG_ERROR("Failed to start IR learning");
        return -1;
    }
    
    g_ir_learning = true;
    LOG_INFO("IR learning started, waiting for IR signal...");
    
    // 发送红外学习开始事件
    event_notify(EVENT_IR_LEARN_STARTED, NULL);
    
    return 0;
}

/**
 * @brief 停止红外学习
 * @return 操作结果：0表示成功，非0表示失败
 */
int remote_control_ir_learn_stop(void) {
    if (!g_remote_control_init || !g_ir_enabled || !g_ir_learning) {
        LOG_INFO("IR learning not running");
        return 0;
    }
    
    // 发送红外学习停止命令给MCU
    uint8_t data = 0;
    if (uart_mcu_send_cmd(CMD_STOP_IR_LEARN, &data, 1) < 0) {
        LOG_ERROR("Failed to stop IR learning");
        return -1;
    }
    
    g_ir_learning = false;
    LOG_INFO("IR learning stopped");
    
    // 发送红外学习停止事件
    event_notify(EVENT_IR_LEARN_STOPPED, NULL);
    
    return 0;
}

/**
 * @brief 设置红外码
 * @param ir_code 红外码值
 * @return 操作结果：0表示成功，非0表示失败
 */
int remote_control_set_ir_code(uint32_t ir_code) {
    if (!g_remote_control_init || !g_ir_enabled) {
        LOG_ERROR("Remote control not initialized or IR not enabled");
        return -1;
    }
    
    LOG_INFO("Received IR code: 0x%08X", ir_code);
    
    // 检查是否在学习模式
    if (g_ir_learning) {
        // 学习模式：添加新的红外码
        if (g_ir_code_count < MAX_IR_CODES) {
            IrCode_t new_code;
            new_code.ir_code = ir_code;
            new_code.key_event = KEY_EVENT_NONE; // 默认为无事件，需要用户配置
            strcpy(new_code.remote_name, "LEARNED");
            strcpy(new_code.key_name, "UNKNOWN");
            
            g_ir_codes[g_ir_code_count] = new_code;
            g_ir_code_count++;
            
            LOG_INFO("Learned new IR code: 0x%08X, total codes: %d", ir_code, g_ir_code_count);
            
            // 发送红外码学习成功事件
            event_notify(EVENT_IR_CODE_LEARNED, &ir_code);
        } else {
            LOG_ERROR("IR code list full, cannot add new code");
            return -1;
        }
    } else {
        // 正常模式：查找匹配的红外码并触发相应事件
        for (int i = 0; i < g_ir_code_count; i++) {
            if (g_ir_codes[i].ir_code == ir_code) {
                LOG_INFO("Matched IR code to key event: %d", g_ir_codes[i].key_event);
                remote_control_process_key_event(g_ir_codes[i].key_event);
                return 0;
            }
        }
        
        LOG_WARN("No matching key event found for IR code: 0x%08X", ir_code);
    }
    
    return SUCCESS;
}

/**
 * @brief 保存红外码配置
 * @return 操作结果：0表示成功，非0表示失败
 */
int remote_control_save_ir_config(void) {
    if (!g_remote_control_init || !g_ir_enabled) {
        LOG_ERROR("Remote control not initialized or IR not enabled");
        return -1;
    }
    
    return save_ir_codes_to_file();
}

/**
 * @brief 加载红外码配置
 * @return 操作结果：0表示成功，非0表示失败
 */
int remote_control_load_ir_config(void) {
    if (!g_remote_control_init || !g_ir_enabled) {
        LOG_ERROR("Remote control not initialized or IR not enabled");
        return -1;
    }
    
    int ret = load_ir_codes_from_file();
    if (ret < 0) {
        // 如果加载失败，添加默认红外码
        add_default_ir_codes();
    }
    
    return ret;
}
