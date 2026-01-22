/**
 * @file network_process.c
 * @brief 网络服务进程实现
 * @details 负责网络服务（DLNA、AirPlay）的处理功能
 * @author AML Audio Team
 * @date 2026-01-22
 */

#include "wifi_media.h"
#include "logger.h"
#include "event.h"
#include "process.h"
#include "common_def.h"

#ifdef _WIN32
// Windows 特定头文件
#include <windows.h>
#define WNOHANG 1
#else
// Unix 特定头文件
#include <sys/wait.h>
#endif

/******************************************************************************************
 * 网络服务进程内部数据结构
 ******************************************************************************************/

/**
 * @brief 网络服务进程消息类型
 */
typedef enum {
    NETWORK_MSG_INIT = 0,           // 初始化网络服务
    NETWORK_MSG_DEINIT,              // 反初始化网络服务
    NETWORK_MSG_START_DLNA,          // 启动DLNA服务
    NETWORK_MSG_STOP_DLNA,           // 停止DLNA服务
    NETWORK_MSG_START_AIRPLAY,       // 启动AirPlay服务
    NETWORK_MSG_STOP_AIRPLAY,        // 停止AirPlay服务
    NETWORK_MSG_SET_DEVICE_NAME,     // 设置设备名称
    NETWORK_MSG_GET_STATUS,          // 获取服务状态
    NETWORK_MSG_MAX
} NetworkMsgType_t;

/**
 * @brief 网络服务进程消息
 */
typedef struct {
    NetworkMsgType_t type;           // 消息类型
    union {
        struct {
            WifiMediaConfig_t config;  // 网络服务配置
        } init;
        struct {
            char device_name[64];    // 设备名称
        } set_device_name;
    } data;
} NetworkMsg_t;

/******************************************************************************************
 * 网络服务进程全局变量
 ******************************************************************************************/

static bool g_network_process_running = false;
static pid_t g_network_process_pid = -1;
static int g_network_process_pipe[2] = {-1, -1};

/******************************************************************************************
 * 网络服务进程内部函数
 ******************************************************************************************/

/**
 * @brief 网络服务进程主函数
 * @param arg 进程参数
 * @return 进程返回值
 */
static void *network_process_main(void *arg) {
    LOG_INFO("Network process started, pid: %d", getpid());
    
    // 初始化网络服务
    WifiMediaConfig_t default_config = {
        .wifi_name = "Aml_Soundbar",
        .dlna_en = true,
        .airplay_en = true
    };
    
    if (wifi_media_init(&default_config) != 0) {
        LOG_ERROR("Wifi media init failed");
        return NULL;
    }
    
    // 主循环
    while (g_network_process_running) {
        // 接收消息
        NetworkMsg_t msg;
        int ret = read(g_network_process_pipe[0], &msg, sizeof(NetworkMsg_t));
        if (ret <= 0) {
            continue;
        }
        
        // 处理消息
        switch (msg.type) {
            case NETWORK_MSG_INIT:
                wifi_media_deinit();
                wifi_media_init(&msg.data.init.config);
                break;
            case NETWORK_MSG_DEINIT:
                wifi_media_deinit();
                break;
            case NETWORK_MSG_START_DLNA:
                wifi_media_start_dlna();
                break;
            case NETWORK_MSG_STOP_DLNA:
                wifi_media_stop_dlna();
                break;
            case NETWORK_MSG_START_AIRPLAY:
                wifi_media_start_airplay();
                break;
            case NETWORK_MSG_STOP_AIRPLAY:
                wifi_media_stop_airplay();
                break;
            case NETWORK_MSG_SET_DEVICE_NAME:
                wifi_media_set_device_name(msg.data.set_device_name.device_name);
                break;
            case NETWORK_MSG_GET_STATUS:
                // 发送服务状态
                break;
            default:
                LOG_ERROR("Invalid network message type: %d", msg.type);
                break;
        }
    }
    
    // 反初始化网络服务
    wifi_media_deinit();
    
    LOG_INFO("Network process exited");
    return NULL;
}

/**
 * @brief 创建网络服务进程
 * @return 进程ID，失败返回-1
 */
static pid_t create_network_process(void) {
    // 创建管道
    if (pipe(g_network_process_pipe) == -1) {
        LOG_ERROR("Failed to create pipe: %d", errno);
        return -1;
    }
    
    // 创建进程
    pid_t pid = fork();
    if (pid == -1) {
        LOG_ERROR("Failed to fork network process: %d", errno);
        close(g_network_process_pipe[0]);
        close(g_network_process_pipe[1]);
        g_network_process_pipe[0] = -1;
        g_network_process_pipe[1] = -1;
        return -1;
    } else if (pid == 0) {
        // 子进程
        close(g_network_process_pipe[1]); // 关闭写端
        g_network_process_running = true;
        network_process_main(NULL);
        close(g_network_process_pipe[0]);
        exit(0);
    } else {
        // 父进程
        close(g_network_process_pipe[0]); // 关闭读端
        g_network_process_pid = pid;
        LOG_INFO("Created network process: %d", pid);
    }
    
    return pid;
}

/**
 * @brief 终止网络服务进程
 * @return SUCCESS/FAILURE
 */
static int terminate_network_process(void) {
    if (g_network_process_pid == -1) {
        return SUCCESS;
    }
    
    // 发送终止消息
    NetworkMsg_t msg;
    msg.type = NETWORK_MSG_DEINIT;
    write(g_network_process_pipe[1], &msg, sizeof(NetworkMsg_t));
    
    // 终止进程
    if (kill(g_network_process_pid, SIGTERM) == -1) {
        LOG_ERROR("Failed to terminate network process: %d", errno);
        return FAILURE;
    }
    
    // 等待进程退出
    waitpid(g_network_process_pid, NULL, 0);
    
    // 关闭管道
    close(g_network_process_pipe[1]);
    g_network_process_pipe[0] = -1;
    g_network_process_pipe[1] = -1;
    g_network_process_pid = -1;
    
    LOG_INFO("Terminated network process: %d", g_network_process_pid);
    return SUCCESS;
}

/******************************************************************************************
 * 网络服务进程对外接口
 ******************************************************************************************/

/**
 * @brief 初始化网络服务进程
 * @param config 网络服务配置
 * @return SUCCESS/FAILURE
 */
int network_process_init(WifiMediaConfig_t *config) {
    // 创建网络服务进程
    pid_t pid = create_network_process();
    if (pid == -1) {
        LOG_ERROR("Failed to create network process");
        return FAILURE;
    }
    
    // 发送初始化消息
    if (config) {
        NetworkMsg_t msg;
        msg.type = NETWORK_MSG_INIT;
        msg.data.init.config = *config;
        write(g_network_process_pipe[1], &msg, sizeof(NetworkMsg_t));
    }
    
    LOG_INFO("Network process initialized");
    return SUCCESS;
}

/**
 * @brief 反初始化网络服务进程
 * @return SUCCESS/FAILURE
 */
int network_process_deinit(void) {
    int ret = terminate_network_process();
    if (ret == SUCCESS) {
        LOG_INFO("Network process deinitialized");
    }
    return ret;
}

/**
 * @brief 启动DLNA服务
 * @return SUCCESS/FAILURE
 */
int network_process_start_dlna(void) {
    if (g_network_process_pid == -1) {
        LOG_ERROR("Network process not initialized");
        return FAILURE;
    }
    
    NetworkMsg_t msg;
    msg.type = NETWORK_MSG_START_DLNA;
    
    if (write(g_network_process_pipe[1], &msg, sizeof(NetworkMsg_t)) != sizeof(NetworkMsg_t)) {
        LOG_ERROR("Failed to send start DLNA message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 停止DLNA服务
 * @return SUCCESS/FAILURE
 */
int network_process_stop_dlna(void) {
    if (g_network_process_pid == -1) {
        LOG_ERROR("Network process not initialized");
        return FAILURE;
    }
    
    NetworkMsg_t msg;
    msg.type = NETWORK_MSG_STOP_DLNA;
    
    if (write(g_network_process_pipe[1], &msg, sizeof(NetworkMsg_t)) != sizeof(NetworkMsg_t)) {
        LOG_ERROR("Failed to send stop DLNA message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 启动AirPlay服务
 * @return SUCCESS/FAILURE
 */
int network_process_start_airplay(void) {
    if (g_network_process_pid == -1) {
        LOG_ERROR("Network process not initialized");
        return FAILURE;
    }
    
    NetworkMsg_t msg;
    msg.type = NETWORK_MSG_START_AIRPLAY;
    
    if (write(g_network_process_pipe[1], &msg, sizeof(NetworkMsg_t)) != sizeof(NetworkMsg_t)) {
        LOG_ERROR("Failed to send start AirPlay message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 停止AirPlay服务
 * @return SUCCESS/FAILURE
 */
int network_process_stop_airplay(void) {
    if (g_network_process_pid == -1) {
        LOG_ERROR("Network process not initialized");
        return FAILURE;
    }
    
    NetworkMsg_t msg;
    msg.type = NETWORK_MSG_STOP_AIRPLAY;
    
    if (write(g_network_process_pipe[1], &msg, sizeof(NetworkMsg_t)) != sizeof(NetworkMsg_t)) {
        LOG_ERROR("Failed to send stop AirPlay message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 设置设备名称
 * @param device_name 设备名称
 * @return SUCCESS/FAILURE
 */
int network_process_set_device_name(const char *device_name) {
    if (g_network_process_pid == -1) {
        LOG_ERROR("Network process not initialized");
        return FAILURE;
    }
    
    if (!device_name) {
        LOG_ERROR("Invalid device name");
        return FAILURE;
    }
    
    NetworkMsg_t msg;
    msg.type = NETWORK_MSG_SET_DEVICE_NAME;
    strncpy(msg.data.set_device_name.device_name, device_name, sizeof(msg.data.set_device_name.device_name) - 1);
    
    if (write(g_network_process_pipe[1], &msg, sizeof(NetworkMsg_t)) != sizeof(NetworkMsg_t)) {
        LOG_ERROR("Failed to send set device name message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 获取网络服务进程ID
 * @return 进程ID，失败返回-1
 */
pid_t network_process_get_pid(void) {
    return g_network_process_pid;
}

/**
 * @brief 检查网络服务进程是否运行
 * @return true表示运行，false表示未运行
 */
bool network_process_is_running(void) {
    if (g_network_process_pid == -1) {
        return false;
    }
    
    int status;
    pid_t result = waitpid(g_network_process_pid, &status, WNOHANG);
    return result == 0;
}
