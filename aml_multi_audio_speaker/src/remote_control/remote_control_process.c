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
#include "process.h"
#include "common_def.h"
#include "comm_mcu.h"

#ifdef _WIN32
// Windows 特定头文件
#include <windows.h>
#define WNOHANG 1
#else
// Unix 特定头文件
#include <sys/wait.h>
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
static pid_t g_remote_control_pid = -1;
static int g_remote_control_pipe[2] = {-1, -1};
static bool g_ir_learning = false;
static KeyEvent_e g_last_key_event = KEY_EVENT_NONE;

/******************************************************************************************
 * 远程控制管理进程内部函数
 ******************************************************************************************/

/**
 * @brief 远程控制管理进程主函数
 * @param arg 进程参数
 * @return 进程返回值
 */
static void *remote_control_process_main(void *arg) {
    LOG_INFO("Remote control process started, pid: %d", getpid());
    
    // 初始化远程控制
    if (remote_control_init() != 0) {
        LOG_ERROR("Remote control init failed");
        return NULL;
    }
    
    // 主循环
    while (g_remote_control_process_running) {
        // 接收消息
        RemoteControlMsg_t msg;
        int ret = read(g_remote_control_pipe[0], &msg, sizeof(RemoteControlMsg_t));
        if (ret <= 0) {
            // 检查按键状态
            comm_mcu_query_key_status();
            // 短暂休眠
            usleep(10000); // 10ms
            continue;
        }
        
        // 处理消息
        switch (msg.type) {
            case RC_MSG_INIT:
                remote_control_deinit();
                remote_control_init();
                break;
            case RC_MSG_DEINIT:
                remote_control_deinit();
                break;
            case RC_MSG_PROCESS_KEY_EVENT:
                remote_control_process_key_event(msg.data.process_key_event.event);
                break;
            case RC_MSG_START_IR_LEARN:
                remote_control_ir_learn_start();
                break;
            case RC_MSG_STOP_IR_LEARN:
                remote_control_ir_learn_stop();
                break;
            case RC_MSG_SET_IR_CODE:
                remote_control_set_ir_code(msg.data.set_ir_code.ir_code);
                break;
            case RC_MSG_GET_STATUS:
                // 发送状态
                break;
            default:
                LOG_ERROR("Invalid remote control message type: %d", msg.type);
                break;
        }
    }
    
    // 反初始化远程控制
    remote_control_deinit();
    
    LOG_INFO("Remote control process exited");
    return NULL;
}

/**
 * @brief 创建远程控制管理进程
 * @return 进程ID，失败返回-1
 */
static pid_t create_remote_control_process(void) {
    // 创建管道
    if (pipe(g_remote_control_pipe) == -1) {
        LOG_ERROR("Failed to create pipe: %d", errno);
        return -1;
    }
    
    // 创建进程
    pid_t pid = fork();
    if (pid == -1) {
        LOG_ERROR("Failed to fork remote control process: %d", errno);
        close(g_remote_control_pipe[0]);
        close(g_remote_control_pipe[1]);
        g_remote_control_pipe[0] = -1;
        g_remote_control_pipe[1] = -1;
        return -1;
    } else if (pid == 0) {
        // 子进程
        close(g_remote_control_pipe[1]); // 关闭写端
        g_remote_control_process_running = true;
        remote_control_process_main(NULL);
        close(g_remote_control_pipe[0]);
        exit(0);
    } else {
        // 父进程
        close(g_remote_control_pipe[0]); // 关闭读端
        g_remote_control_pid = pid;
        LOG_INFO("Created remote control process: %d", pid);
    }
    
    return pid;
}

/**
 * @brief 终止远程控制管理进程
 * @return SUCCESS/FAILURE
 */
static int terminate_remote_control_process(void) {
    if (g_remote_control_pid == -1) {
        return SUCCESS;
    }
    
    // 发送终止消息
    RemoteControlMsg_t msg;
    msg.type = RC_MSG_DEINIT;
    write(g_remote_control_pipe[1], &msg, sizeof(RemoteControlMsg_t));
    
    // 终止进程
    if (kill(g_remote_control_pid, SIGTERM) == -1) {
        LOG_ERROR("Failed to terminate remote control process: %d", errno);
        return FAILURE;
    }
    
    // 等待进程退出
    waitpid(g_remote_control_pid, NULL, 0);
    
    // 关闭管道
    close(g_remote_control_pipe[1]);
    g_remote_control_pipe[0] = -1;
    g_remote_control_pipe[1] = -1;
    g_remote_control_pid = -1;
    
    LOG_INFO("Terminated remote control process: %d", g_remote_control_pid);
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
    // 创建远程控制管理进程
    pid_t pid = create_remote_control_process();
    if (pid == -1) {
        LOG_ERROR("Failed to create remote control process");
        return FAILURE;
    }
    
    LOG_INFO("Remote control process initialized");
    return SUCCESS;
}

/**
 * @brief 反初始化远程控制管理进程
 * @return SUCCESS/FAILURE
 */
int remote_control_process_deinit(void) {
    int ret = terminate_remote_control_process();
    if (ret == SUCCESS) {
        LOG_INFO("Remote control process deinitialized");
    }
    return ret;
}

/**
 * @brief 处理按键事件
 * @param event 按键事件
 * @return SUCCESS/FAILURE
 */
int remote_control_process_handle_key_event(KeyEvent_e event) {
    if (g_remote_control_pid == -1) {
        LOG_ERROR("Remote control process not initialized");
        return FAILURE;
    }
    
    RemoteControlMsg_t msg;
    msg.type = RC_MSG_PROCESS_KEY_EVENT;
    msg.data.process_key_event.event = event;
    
    if (write(g_remote_control_pipe[1], &msg, sizeof(RemoteControlMsg_t)) != sizeof(RemoteControlMsg_t)) {
        LOG_ERROR("Failed to send process key event message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 开始红外学习
 * @return SUCCESS/FAILURE
 */
int remote_control_process_start_ir_learn(void) {
    if (g_remote_control_pid == -1) {
        LOG_ERROR("Remote control process not initialized");
        return FAILURE;
    }
    
    RemoteControlMsg_t msg;
    msg.type = RC_MSG_START_IR_LEARN;
    
    if (write(g_remote_control_pipe[1], &msg, sizeof(RemoteControlMsg_t)) != sizeof(RemoteControlMsg_t)) {
        LOG_ERROR("Failed to send start IR learn message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 停止红外学习
 * @return SUCCESS/FAILURE
 */
int remote_control_process_stop_ir_learn(void) {
    if (g_remote_control_pid == -1) {
        LOG_ERROR("Remote control process not initialized");
        return FAILURE;
    }
    
    RemoteControlMsg_t msg;
    msg.type = RC_MSG_STOP_IR_LEARN;
    
    if (write(g_remote_control_pipe[1], &msg, sizeof(RemoteControlMsg_t)) != sizeof(RemoteControlMsg_t)) {
        LOG_ERROR("Failed to send stop IR learn message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 获取远程控制管理进程ID
 * @return 进程ID，失败返回-1
 */
pid_t remote_control_process_get_pid(void) {
    return g_remote_control_pid;
}

/**
 * @brief 检查远程控制管理进程是否运行
 * @return true表示运行，false表示未运行
 */
bool remote_control_process_is_running(void) {
    if (g_remote_control_pid == -1) {
        return false;
    }
    
    int status;
    pid_t result = waitpid(g_remote_control_pid, &status, WNOHANG);
    return result == 0;
}
