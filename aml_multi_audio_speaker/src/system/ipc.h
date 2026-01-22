/**
 * @file ipc.h
 * @brief 进程间通信模块头文件
 * @details 提供统一的进程间通信机制，支持多种通信方式
 * @author AML Audio Team
 * @date 2026-01-22
 */

#ifndef __IPC_H__
#define __IPC_H__

#include "common_def.h"

/******************************************************************************************
 * 进程间通信数据类型
 ******************************************************************************************/

/**
 * @brief IPC 通道类型
 */
typedef enum {
    IPC_CHANNEL_PIPE = 0,          // 管道通信
    IPC_CHANNEL_MSGQUEUE,          // 消息队列
    IPC_CHANNEL_SHARED_MEM,        // 共享内存
    IPC_CHANNEL_SOCKET,            // 套接字
    IPC_CHANNEL_MAX
} IpcChannelType_t;

/**
 * @brief IPC 消息类型
 */
typedef enum {
    IPC_MSG_TYPE_REQUEST = 0,       // 请求消息
    IPC_MSG_TYPE_RESPONSE,          // 响应消息
    IPC_MSG_TYPE_EVENT,             // 事件消息
    IPC_MSG_TYPE_NOTIFICATION,      // 通知消息
    IPC_MSG_TYPE_MAX
} IpcMsgType_t;

/**
 * @brief IPC 消息结构体
 */
typedef struct {
    IpcMsgType_t type;              // 消息类型
    uint32_t id;                    // 消息ID
    uint32_t len;                   // 消息长度
    void *data;                     // 消息数据
} IpcMsg_t;

/**
 * @brief IPC 通道结构体
 */
typedef struct {
    char name[64];                  // 通道名称
    IpcChannelType_t type;          // 通道类型
    int fd[2];                      // 通道文件描述符
    bool initialized;               // 初始化标志
} IpcChannel_t;

/******************************************************************************************
 * 进程间通信对外接口
 ******************************************************************************************/

/**
 * @brief 初始化IPC模块
 * @return SUCCESS/FAILURE
 */
int ipc_init(void);

/**
 * @brief 反初始化IPC模块
 * @return SUCCESS/FAILURE
 */
int ipc_deinit(void);

/**
 * @brief 创建IPC通道
 * @param name 通道名称
 * @param type 通道类型
 * @return 通道ID，失败返回-1
 */
int ipc_create_channel(const char *name, IpcChannelType_t type);

/**
 * @brief 销毁IPC通道
 * @param name 通道名称
 * @return SUCCESS/FAILURE
 */
int ipc_destroy_channel(const char *name);

/**
 * @brief 发送IPC消息
 * @param name 通道名称
 * @param msg 消息结构体
 * @param timeout 超时时间（毫秒）
 * @return SUCCESS/FAILURE
 */
int ipc_send(const char *name, const IpcMsg_t *msg, int timeout);

/**
 * @brief 接收IPC消息
 * @param name 通道名称
 * @param msg 消息结构体
 * @param timeout 超时时间（毫秒）
 * @return 消息长度，失败返回-1
 */
int ipc_recv(const char *name, IpcMsg_t *msg, int timeout);

/**
 * @brief 设置IPC通道属性
 * @param name 通道名称
 * @param key 属性键
 * @param value 属性值
 * @return SUCCESS/FAILURE
 */
int ipc_set_channel_attr(const char *name, const char *key, const char *value);

/**
 * @brief 获取IPC通道属性
 * @param name 通道名称
 * @param key 属性键
 * @param value 属性值
 * @param max_len 值最大长度
 * @return SUCCESS/FAILURE
 */
int ipc_get_channel_attr(const char *name, const char *key, char *value, int max_len);

/**
 * @brief 检查IPC通道是否可用
 * @param name 通道名称
 * @return true表示可用，false表示不可用
 */
bool ipc_channel_is_available(const char *name);

/**
 * @brief 获取IPC通道状态
 * @param name 通道名称
 * @param fd 通道文件描述符
 * @return SUCCESS/FAILURE
 */
int ipc_get_channel_status(const char *name, int *fd);

#endif /* __IPC_H__ */