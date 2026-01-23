/**
 * @file remote_control_process.c
 * @brief 远程控制管理进程实现
 * @details 负责遥控器事件处理、红外学习等远程控制功能
 * @author AML Audio Team
 * @date 2026-01-22
 */

#include "remote_control_priv.h"
#include "logger.h"
#include "event.h"
#include "common_def.h"
#include "comm_mcu.h"
#include "../system/process_manager.h"

#ifdef _WIN32
// Windows 特定头文件
#include <windows.h>
#else
// Unix 特定头文件
#include <sys/wait.h>
#include <unistd.h>
#endif

/******************************************************************************************
 * 远程控制管理进程消息类型
 ******************************************************************************************/

/**
 * @brief 远程控制管理进程消息类型
 */
typedef enum {
    RC_MSG_INIT = 0,           // 初始化远程控制
    RC_MSG_DEINIT,              // 反初始化远程控制
    RC_MSG_PROCESS_KEY_EVENT,   // 处理按键事件
    RC_MSG_START_IR_LEARN,      // 开始红外学习
    RC_MSG_STOP_IR_LEARN,       // 停止红外学习
    RC_MSG_SET_IR_CODE,         // 设置红外码
    RC_MSG_GET_STATUS,          // 获取状态
    RC_MSG_MAX
} RemoteControlMsgType_t;

/**
 * @brief 远程控制管理进程消息
 */
typedef struct {
    RemoteControlMsgType_t type; // 消息类型
    union {
        struct {
            KeyEvent_e event;     // 按键事件
        } process_key_event;
        struct {
            uint32_t ir_code;     // 红外码
        } set_ir_code;
    } data;
} RemoteControlMsg_t;

/******************************************************************************************
 * 远程控制管理进程全局变量
 ******************************************************************************************/

static bool g_remote_control_process_running = false;
static bool g_ir_learning = false;
static KeyEvent_e g_last_key_event = KEY_EVENT_NONE;

/******************************************************************************************
 * 远程控制管理进程内部函数
 ******************************************************************************************/

/**
 * @brief 远程控制管理进程主函数
 * @param arg 进程参数
 */
static void remote_control_process_main(void *arg) {
    LOG_INFO("Remote control process started");
    
    // 初始化远程控制
    if (remote_control_init() != 0) {
        LOG_ERROR("Remote control init failed");
        return;
    }
    
    // 主循环
    while (g_remote_control_process_running) {
        // 检查按键状态
        comm_mcu_query_key_status();
        // 短暂休眠
        usleep(10000); // 10ms
    }
    
    // 反初始化远程控制
    remote_control_deinit();
    
    LOG_INFO("Remote control process exited");
}

/**
 * @brief 远程控制管理进程终止函数
 * @return SUCCESS/FAILURE
 */
static int remote_control_process_terminate(void) {
    g_remote_control_process_running = false;
    LOG_INFO("Remote control process termination requested");
    return SUCCESS;
}

/******************************************************************************************
 * 远程控制管理进程对外接口
 ******************************************************************************************/

/**
 * @brief 初始化远程控制管理进程
 * @return SUCCESS/FAILURE
 */
int remote_control_process_init(void) {
    // 注册远程控制进程到进程管理器
    int ret = process_manager_register_process(
        PROCESS_NAME_REMOTE_CONTROL,
        remote_control_process_main,
        NULL,
        remote_control_process_terminate
    );
    
    if (ret != 0) {
        LOG_ERROR("Failed to register remote control process");
        return FAILURE;
    }
    
    // 设置进程优先级
    process_manager_set_process_priority(PROCESS_NAME_REMOTE_CONTROL, PROCESS_PRIORITY_NORMAL);
    
    // 设置自动重启
    process_manager_set_process_auto_restart(PROCESS_NAME_REMOTE_CONTROL, true);
    
    // 添加依赖
    process_manager_add_process_dependency(PROCESS_NAME_REMOTE_CONTROL, PROCESS_NAME_SYSTEM);
    
    // 启动远程控制进程
    ret = process_manager_start_process(PROCESS_NAME_REMOTE_CONTROL);
    if (ret != 0) {
        LOG_ERROR("Failed to start remote control process");
        return FAILURE;
    }
    
    g_remote_control_process_running = true;
    LOG_INFO("Remote control process initialized");
    return SUCCESS;
}

/**
 * @brief 反初始化远程控制管理进程
 * @return SUCCESS/FAILURE
 */
int remote_control_process_deinit(void) {
    int ret = process_manager_stop_process(PROCESS_NAME_REMOTE_CONTROL);
    if (ret != 0) {
        LOG_ERROR("Failed to stop remote control process");
        return FAILURE;
    }
    
    g_remote_control_process_running = false;
    LOG_INFO("Remote control process deinitialized");
    return SUCCESS;
}

/**
 * @brief 处理按键事件
 * @param event 按键事件
 * @return SUCCESS/FAILURE
 */
int remote_control_process_handle_key_event(KeyEvent_e event) {
    // 直接处理按键事件，不再通过管道通信
    if (!g_remote_control_process_running) {
        LOG_ERROR("Remote control process not initialized");
        return FAILURE;
    }
    
    // 处理按键事件
    remote_control_process_key_event(event);
    g_last_key_event = event;
    
    return SUCCESS;
}

/**
 * @brief 开始红外学习
 * @return SUCCESS/FAILURE
 */
int remote_control_process_start_ir_learn(void) {
    if (!g_remote_control_process_running) {
        LOG_ERROR("Remote control process not initialized");
        return FAILURE;
    }
    
    // 开始红外学习
    remote_control_ir_learn_start();
    g_ir_learning = true;
    
    return SUCCESS;
}

/**
 * @brief 停止红外学习
 * @return SUCCESS/FAILURE
 */
int remote_control_process_stop_ir_learn(void) {
    if (!g_remote_control_process_running) {
        LOG_ERROR("Remote control process not initialized");
        return FAILURE;
    }
    
    // 停止红外学习
    remote_control_ir_learn_stop();
    g_ir_learning = false;
    
    return SUCCESS;
}

/**
 * @brief 获取远程控制管理进程ID
 * @return 进程ID，失败返回-1
 */
pid_t remote_control_process_get_pid(void) {
    return process_manager_get_process_pid(PROCESS_NAME_REMOTE_CONTROL);
}

/**
 * @brief 检查远程控制管理进程是否运行
 * @return true表示运行，false表示未运行
 */
bool remote_control_process_is_running(void) {
    return process_manager_is_process_running(PROCESS_NAME_REMOTE_CONTROL);
}

/**
 * @brief 设置红外码
 * @param ir_code 红外码
 * @return SUCCESS/FAILURE
 */
int remote_control_process_set_ir_code(uint32_t ir_code) {
    if (!g_remote_control_process_running) {
        LOG_ERROR("Remote control process not initialized");
        return FAILURE;
    }
    
    // 设置红外码
    remote_control_set_ir_code(ir_code);
    
    return SUCCESS;
}
