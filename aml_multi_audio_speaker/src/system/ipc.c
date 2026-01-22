/**
 * @file ipc.c
 * @brief 进程间通信模块实现
 * @details 提供统一的进程间通信机制，支持多种通信方式
 * @author AML Audio Team
 * @date 2026-01-22
 */

#include "ipc.h"
#include "logger.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/******************************************************************************************
 * IPC内部数据结构
 ******************************************************************************************/

/**
 * @brief IPC通道节点
 */
typedef struct IpcChannelNode {
    IpcChannel_t channel;           // 通道结构体
    struct IpcChannelNode *next;    // 指向下一个节点
} IpcChannelNode_t;

/**
 * @brief IPC模块全局变量
 */
typedef struct {
    IpcChannelNode_t *channel_list; // 通道列表
    pthread_mutex_t mutex;          // 互斥锁
    bool initialized;               // 初始化标志
} IpcManager_t;

/******************************************************************************************
 * IPC模块全局变量
 ******************************************************************************************/

static IpcManager_t g_ipc_manager = {
    .channel_list = NULL,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .initialized = false
};

/******************************************************************************************
 * IPC内部函数
 ******************************************************************************************/

/**
 * @brief 查找IPC通道
 * @param name 通道名称
 * @return 通道指针
 */
static IpcChannel_t *find_channel(const char *name) {
    pthread_mutex_lock(&g_ipc_manager.mutex);
    
    IpcChannelNode_t *node = g_ipc_manager.channel_list;
    while (node) {
        if (strcmp(node->channel.name, name) == 0) {
            pthread_mutex_unlock(&g_ipc_manager.mutex);
            return &node->channel;
        }
        node = node->next;
    }
    
    pthread_mutex_unlock(&g_ipc_manager.mutex);
    return NULL;
}

/**
 * @brief 添加IPC通道
 * @param channel 通道结构体
 * @return SUCCESS/FAILURE
 */
static int add_channel(const IpcChannel_t *channel) {
    pthread_mutex_lock(&g_ipc_manager.mutex);
    
    // 检查通道是否已存在
    IpcChannelNode_t *node = g_ipc_manager.channel_list;
    while (node) {
        if (strcmp(node->channel.name, channel->name) == 0) {
            pthread_mutex_unlock(&g_ipc_manager.mutex);
            return FAILURE;
        }
        node = node->next;
    }
    
    // 创建新节点
    IpcChannelNode_t *new_node = (IpcChannelNode_t *)malloc(sizeof(IpcChannelNode_t));
    if (!new_node) {
        LOG_ERROR("Failed to allocate IPC channel node");
        pthread_mutex_unlock(&g_ipc_manager.mutex);
        return FAILURE;
    }
    
    // 复制通道信息
    memcpy(&new_node->channel, channel, sizeof(IpcChannel_t));
    new_node->next = g_ipc_manager.channel_list;
    g_ipc_manager.channel_list = new_node;
    
    pthread_mutex_unlock(&g_ipc_manager.mutex);
    return SUCCESS;
}

/**
 * @brief 移除IPC通道
 * @param name 通道名称
 * @return SUCCESS/FAILURE
 */
static int remove_channel(const char *name) {
    pthread_mutex_lock(&g_ipc_manager.mutex);
    
    IpcChannelNode_t *node = g_ipc_manager.channel_list;
    IpcChannelNode_t *prev = NULL;
    
    while (node) {
        if (strcmp(node->channel.name, name) == 0) {
            if (prev) {
                prev->next = node->next;
            } else {
                g_ipc_manager.channel_list = node->next;
            }
            
            // 关闭文件描述符
            if (node->channel.initialized) {
                if (node->channel.fd[0] != -1) {
                    close(node->channel.fd[0]);
                }
                if (node->channel.fd[1] != -1) {
                    close(node->channel.fd[1]);
                }
            }
            
            free(node);
            pthread_mutex_unlock(&g_ipc_manager.mutex);
            return SUCCESS;
        }
        prev = node;
        node = node->next;
    }
    
    pthread_mutex_unlock(&g_ipc_manager.mutex);
    return FAILURE;
}

/******************************************************************************************
 * IPC对外接口
 ******************************************************************************************/

int ipc_init(void) {
    if (g_ipc_manager.initialized) {
        LOG_WARN("IPC module already initialized");
        return SUCCESS;
    }
    
    g_ipc_manager.channel_list = NULL;
    g_ipc_manager.initialized = true;
    
    LOG_INFO("IPC module initialized");
    return SUCCESS;
}

int ipc_deinit(void) {
    if (!g_ipc_manager.initialized) {
        LOG_WARN("IPC module not initialized");
        return SUCCESS;
    }
    
    pthread_mutex_lock(&g_ipc_manager.mutex);
    
    // 销毁所有通道
    IpcChannelNode_t *node = g_ipc_manager.channel_list;
    while (node) {
        IpcChannelNode_t *next = node->next;
        
        // 关闭文件描述符
        if (node->channel.initialized) {
            if (node->channel.fd[0] != -1) {
                close(node->channel.fd[0]);
            }
            if (node->channel.fd[1] != -1) {
                close(node->channel.fd[1]);
            }
        }
        
        free(node);
        node = next;
    }
    
    g_ipc_manager.channel_list = NULL;
    g_ipc_manager.initialized = false;
    
    pthread_mutex_unlock(&g_ipc_manager.mutex);
    
    LOG_INFO("IPC module deinitialized");
    return SUCCESS;
}

int ipc_create_channel(const char *name, IpcChannelType_t type) {
    if (!g_ipc_manager.initialized) {
        LOG_ERROR("IPC module not initialized");
        return -1;
    }
    
    if (!name || type >= IPC_CHANNEL_MAX) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    // 检查通道是否已存在
    if (find_channel(name)) {
        LOG_ERROR("IPC channel already exists: %s", name);
        return -1;
    }
    
    // 创建通道
    IpcChannel_t channel;
    memset(&channel, 0, sizeof(IpcChannel_t));
    strncpy(channel.name, name, sizeof(channel.name) - 1);
    channel.type = type;
    channel.fd[0] = -1;
    channel.fd[1] = -1;
    channel.initialized = false;
    
    // 根据类型创建具体通道
    switch (type) {
        case IPC_CHANNEL_PIPE:
            if (pipe(channel.fd) == -1) {
                LOG_ERROR("Failed to create pipe: %d", errno);
                return -1;
            }
            channel.initialized = true;
            break;
        case IPC_CHANNEL_MSGQUEUE:
        case IPC_CHANNEL_SHARED_MEM:
        case IPC_CHANNEL_SOCKET:
            // 简化实现，仅支持管道
            LOG_WARN("Channel type not supported yet: %d", type);
            return -1;
        default:
            LOG_ERROR("Invalid channel type: %d", type);
            return -1;
    }
    
    // 添加通道到列表
    if (add_channel(&channel) != SUCCESS) {
        LOG_ERROR("Failed to add IPC channel");
        if (channel.initialized) {
            close(channel.fd[0]);
            close(channel.fd[1]);
        }
        return -1;
    }
    
    LOG_INFO("Created IPC channel: %s, type: %d", name, type);
    return 0;
}

int ipc_destroy_channel(const char *name) {
    if (!g_ipc_manager.initialized) {
        LOG_ERROR("IPC module not initialized");
        return FAILURE;
    }
    
    if (!name) {
        LOG_ERROR("Invalid parameter");
        return FAILURE;
    }
    
    int ret = remove_channel(name);
    if (ret == SUCCESS) {
        LOG_INFO("Destroyed IPC channel: %s", name);
    } else {
        LOG_ERROR("Failed to destroy IPC channel: %s", name);
    }
    
    return ret;
}

int ipc_send(const char *name, const IpcMsg_t *msg, int timeout) {
    if (!g_ipc_manager.initialized) {
        LOG_ERROR("IPC module not initialized");
        return FAILURE;
    }
    
    if (!name || !msg) {
        LOG_ERROR("Invalid parameters");
        return FAILURE;
    }
    
    // 查找通道
    IpcChannel_t *channel = find_channel(name);
    if (!channel || !channel->initialized) {
        LOG_ERROR("IPC channel not found or not initialized: %s", name);
        return FAILURE;
    }
    
    // 发送消息（简化实现）
    if (channel->type == IPC_CHANNEL_PIPE) {
        // 写入消息长度
        if (write(channel->fd[1], &msg->len, sizeof(msg->len)) != sizeof(msg->len)) {
            LOG_ERROR("Failed to send message length: %d", errno);
            return FAILURE;
        }
        
        // 写入消息数据
        if (msg->len > 0 && msg->data) {
            if (write(channel->fd[1], msg->data, msg->len) != msg->len) {
                LOG_ERROR("Failed to send message data: %d", errno);
                return FAILURE;
            }
        }
    }
    
    LOG_DEBUG("Sent IPC message: %s, type: %d, id: %u, len: %u", 
             name, msg->type, msg->id, msg->len);
    return SUCCESS;
}

int ipc_recv(const char *name, IpcMsg_t *msg, int timeout) {
    if (!g_ipc_manager.initialized) {
        LOG_ERROR("IPC module not initialized");
        return -1;
    }
    
    if (!name || !msg) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    // 查找通道
    IpcChannel_t *channel = find_channel(name);
    if (!channel || !channel->initialized) {
        LOG_ERROR("IPC channel not found or not initialized: %s", name);
        return -1;
    }
    
    // 接收消息（简化实现）
    if (channel->type == IPC_CHANNEL_PIPE) {
        // 读取消息长度
        uint32_t len;
        if (read(channel->fd[0], &len, sizeof(len)) != sizeof(len)) {
            LOG_ERROR("Failed to receive message length: %d", errno);
            return -1;
        }
        
        msg->len = len;
        
        // 读取消息数据
        if (len > 0) {
            msg->data = malloc(len);
            if (!msg->data) {
                LOG_ERROR("Failed to allocate message buffer");
                return -1;
            }
            
            if (read(channel->fd[0], msg->data, len) != len) {
                LOG_ERROR("Failed to receive message data: %d", errno);
                free(msg->data);
                msg->data = NULL;
                return -1;
            }
        } else {
            msg->data = NULL;
        }
    }
    
    LOG_DEBUG("Received IPC message: %s, type: %d, id: %u, len: %u", 
             name, msg->type, msg->id, msg->len);
    return msg->len;
}

int ipc_set_channel_attr(const char *name, const char *key, const char *value) {
    if (!g_ipc_manager.initialized) {
        LOG_ERROR("IPC module not initialized");
        return FAILURE;
    }
    
    if (!name || !key || !value) {
        LOG_ERROR("Invalid parameters");
        return FAILURE;
    }
    
    // 查找通道
    IpcChannel_t *channel = find_channel(name);
    if (!channel) {
        LOG_ERROR("IPC channel not found: %s", name);
        return FAILURE;
    }
    
    // 简化实现，暂不支持属性设置
    LOG_WARN("Channel attributes not supported yet");
    return FAILURE;
}

int ipc_get_channel_attr(const char *name, const char *key, char *value, int max_len) {
    if (!g_ipc_manager.initialized) {
        LOG_ERROR("IPC module not initialized");
        return FAILURE;
    }
    
    if (!name || !key || !value) {
        LOG_ERROR("Invalid parameters");
        return FAILURE;
    }
    
    // 查找通道
    IpcChannel_t *channel = find_channel(name);
    if (!channel) {
        LOG_ERROR("IPC channel not found: %s", name);
        return FAILURE;
    }
    
    // 简化实现，暂不支持属性获取
    LOG_WARN("Channel attributes not supported yet");
    return FAILURE;
}

bool ipc_channel_is_available(const char *name) {
    if (!g_ipc_manager.initialized) {
        return false;
    }
    
    if (!name) {
        return false;
    }
    
    IpcChannel_t *channel = find_channel(name);
    return channel != NULL && channel->initialized;
}

int ipc_get_channel_status(const char *name, int *fd) {
    if (!g_ipc_manager.initialized) {
        LOG_ERROR("IPC module not initialized");
        return FAILURE;
    }
    
    if (!name || !fd) {
        LOG_ERROR("Invalid parameters");
        return FAILURE;
    }
    
    // 查找通道
    IpcChannel_t *channel = find_channel(name);
    if (!channel || !channel->initialized) {
        LOG_ERROR("IPC channel not found or not initialized: %s", name);
        return FAILURE;
    }
    
    // 返回文件描述符
    fd[0] = channel->fd[0];
    fd[1] = channel->fd[1];
    
    return SUCCESS;
}
