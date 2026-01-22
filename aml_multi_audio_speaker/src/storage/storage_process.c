/**
 * @file storage_process.c
 * @brief 存储管理进程实现
 * @details 负责U盘挂载、媒体扫描等存储管理功能
 * @author AML Audio Team
 * @date 2026-01-22
 */

#include "storage.h"
#include "storage_priv.h"
#include "logger.h"
#include "event.h"
#include "process.h"
#include "common_def.h"

/******************************************************************************************
 * 存储管理进程内部数据结构
 ******************************************************************************************/

/**
 * @brief 存储管理进程消息类型
 */
typedef enum {
    STORAGE_MSG_INIT = 0,           // 初始化存储管理
    STORAGE_MSG_DEINIT,              // 反初始化存储管理
    STORAGE_MSG_MOUNT_DEVICE,       // 挂载存储设备
    STORAGE_MSG_UMOUNT_DEVICE,      // 卸载存储设备
    STORAGE_MSG_SCAN_MEDIA,          // 扫描媒体文件
    STORAGE_MSG_GET_FILE_LIST,       // 获取文件列表
    STORAGE_MSG_GET_STORAGE_INFO,    // 获取存储信息
    STORAGE_MSG_MAX
} StorageMsgType_t;

/**
 * @brief 存储管理进程消息
 */
typedef struct {
    StorageMsgType_t type;           // 消息类型
    union {
        struct {
            const char *dev_path;    // 设备路径
        } mount_device;
        struct {
            const char *path;        // 扫描路径
        } scan_media;
    } data;
} StorageMsg_t;

/******************************************************************************************
 * 存储管理进程全局变量
 ******************************************************************************************/

static bool g_storage_process_running = false;
static pid_t g_storage_process_pid = -1;
static int g_storage_process_pipe[2] = {-1, -1};

/******************************************************************************************
 * 存储管理进程内部函数
 ******************************************************************************************/

/**
 * @brief 存储管理进程主函数
 * @param arg 进程参数
 * @return 进程返回值
 */
static void *storage_process_main(void *arg) {
    LOG_INFO("Storage process started, pid: %d", getpid());
    
    // 初始化存储管理
    if (storage_init() != 0) {
        LOG_ERROR("Storage init failed");
        return NULL;
    }
    
    // 主循环
    while (g_storage_process_running) {
        // 接收消息
        StorageMsg_t msg;
        int ret = read(g_storage_process_pipe[0], &msg, sizeof(StorageMsg_t));
        if (ret <= 0) {
            continue;
        }
        
        // 处理消息
        switch (msg.type) {
            case STORAGE_MSG_INIT:
                storage_deinit();
                storage_init();
                break;
            case STORAGE_MSG_DEINIT:
                storage_deinit();
                break;
            case STORAGE_MSG_MOUNT_DEVICE:
                if (msg.data.mount_device.dev_path) {
                    usb_mount_mount_device(msg.data.mount_device.dev_path);
                }
                break;
            case STORAGE_MSG_UMOUNT_DEVICE:
                usb_mount_umount_device();
                break;
            case STORAGE_MSG_SCAN_MEDIA:
                if (msg.data.scan_media.path) {
                    media_scan_scan_path(msg.data.scan_media.path);
                }
                break;
            case STORAGE_MSG_GET_FILE_LIST:
                // 获取文件列表并返回
                break;
            case STORAGE_MSG_GET_STORAGE_INFO:
                // 获取存储信息并返回
                break;
            default:
                LOG_ERROR("Invalid storage message type: %d", msg.type);
                break;
        }
    }
    
    // 反初始化存储管理
    storage_deinit();
    
    LOG_INFO("Storage process exited");
    return NULL;
}

/**
 * @brief 创建存储管理进程
 * @return 进程ID，失败返回-1
 */
static pid_t create_storage_process(void) {
    // 创建管道
    if (pipe(g_storage_process_pipe) == -1) {
        LOG_ERROR("Failed to create pipe: %d", errno);
        return -1;
    }
    
    // 创建进程
    pid_t pid = fork();
    if (pid == -1) {
        LOG_ERROR("Failed to fork storage process: %d", errno);
        close(g_storage_process_pipe[0]);
        close(g_storage_process_pipe[1]);
        g_storage_process_pipe[0] = -1;
        g_storage_process_pipe[1] = -1;
        return -1;
    } else if (pid == 0) {
        // 子进程
        close(g_storage_process_pipe[1]); // 关闭写端
        g_storage_process_running = true;
        storage_process_main(NULL);
        close(g_storage_process_pipe[0]);
        exit(0);
    } else {
        // 父进程
        close(g_storage_process_pipe[0]); // 关闭读端
        g_storage_process_pid = pid;
        LOG_INFO("Created storage process: %d", pid);
    }
    
    return pid;
}

/**
 * @brief 终止存储管理进程
 * @return SUCCESS/FAILURE
 */
static int terminate_storage_process(void) {
    if (g_storage_process_pid == -1) {
        return SUCCESS;
    }
    
    // 发送终止消息
    StorageMsg_t msg;
    msg.type = STORAGE_MSG_DEINIT;
    write(g_storage_process_pipe[1], &msg, sizeof(StorageMsg_t));
    
    // 终止进程
    if (kill(g_storage_process_pid, SIGTERM) == -1) {
        LOG_ERROR("Failed to terminate storage process: %d", errno);
        return FAILURE;
    }
    
    // 等待进程退出
    waitpid(g_storage_process_pid, NULL, 0);
    
    // 关闭管道
    close(g_storage_process_pipe[1]);
    g_storage_process_pipe[0] = -1;
    g_storage_process_pipe[1] = -1;
    g_storage_process_pid = -1;
    
    LOG_INFO("Terminated storage process: %d", g_storage_process_pid);
    return SUCCESS;
}

/******************************************************************************************
 * 存储管理进程对外接口
 ******************************************************************************************/

/**
 * @brief 初始化存储管理进程
 * @return SUCCESS/FAILURE
 */
int storage_process_init(void) {
    // 创建存储管理进程
    pid_t pid = create_storage_process();
    if (pid == -1) {
        LOG_ERROR("Failed to create storage process");
        return FAILURE;
    }
    
    LOG_INFO("Storage process initialized");
    return SUCCESS;
}

/**
 * @brief 反初始化存储管理进程
 * @return SUCCESS/FAILURE
 */
int storage_process_deinit(void) {
    int ret = terminate_storage_process();
    if (ret == SUCCESS) {
        LOG_INFO("Storage process deinitialized");
    }
    return ret;
}

/**
 * @brief 挂载存储设备
 * @param dev_path 设备路径
 * @return SUCCESS/FAILURE
 */
int storage_process_mount_device(const char *dev_path) {
    if (g_storage_process_pid == -1) {
        LOG_ERROR("Storage process not initialized");
        return FAILURE;
    }
    
    if (!dev_path) {
        LOG_ERROR("Invalid device path");
        return FAILURE;
    }
    
    StorageMsg_t msg;
    msg.type = STORAGE_MSG_MOUNT_DEVICE;
    msg.data.mount_device.dev_path = dev_path;
    
    if (write(g_storage_process_pipe[1], &msg, sizeof(StorageMsg_t)) != sizeof(StorageMsg_t)) {
        LOG_ERROR("Failed to send mount device message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 卸载存储设备
 * @return SUCCESS/FAILURE
 */
int storage_process_umount_device(void) {
    if (g_storage_process_pid == -1) {
        LOG_ERROR("Storage process not initialized");
        return FAILURE;
    }
    
    StorageMsg_t msg;
    msg.type = STORAGE_MSG_UMOUNT_DEVICE;
    
    if (write(g_storage_process_pipe[1], &msg, sizeof(StorageMsg_t)) != sizeof(StorageMsg_t)) {
        LOG_ERROR("Failed to send umount device message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 扫描媒体文件
 * @param path 扫描路径
 * @return SUCCESS/FAILURE
 */
int storage_process_scan_media(const char *path) {
    if (g_storage_process_pid == -1) {
        LOG_ERROR("Storage process not initialized");
        return FAILURE;
    }
    
    if (!path) {
        LOG_ERROR("Invalid scan path");
        return FAILURE;
    }
    
    StorageMsg_t msg;
    msg.type = STORAGE_MSG_SCAN_MEDIA;
    msg.data.scan_media.path = path;
    
    if (write(g_storage_process_pipe[1], &msg, sizeof(StorageMsg_t)) != sizeof(StorageMsg_t)) {
        LOG_ERROR("Failed to send scan media message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 获取存储管理进程ID
 * @return 进程ID，失败返回-1
 */
pid_t storage_process_get_pid(void) {
    return g_storage_process_pid;
}

/**
 * @brief 检查存储管理进程是否运行
 * @return true表示运行，false表示未运行
 */
bool storage_process_is_running(void) {
    if (g_storage_process_pid == -1) {
        return false;
    }
    
    int status;
    pid_t result = waitpid(g_storage_process_pid, &status, WNOHANG);
    return result == 0;
}
