#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <jansson.h>
#include <time.h>
#include <errno.h>

#include "wac.h"
#include "../../include/dbus_utils.h"

// 前向声明
static void* wac_worker_thread(void* data);
static int wac_setup_server(WacService* service);
static void wac_cleanup_server(WacService* service);
static void wac_set_state(WacService* service, WacState state);
static int wac_find_accessory_index(WacService* service, const char* identifier);
static void wac_update_uptime(WacService* service);

// 创建WAC服务
WacService* wac_create(struct pw_context* context, const WacConfig* config) {
    if (!context || !config) {
        fprintf(stderr, "Invalid parameters for wac_create\n");
        return NULL;
    }

    WacService* service = calloc(1, sizeof(WacService));
    if (!service) {
        fprintf(stderr, "Failed to allocate memory for WacService\n");
        return NULL;
    }

    // 初始化D-Bus
    if (!dbus_initialize()) {
        fprintf(stderr, "Failed to initialize D-Bus connection for WAC\n");
        // 继续初始化但记录错误
    }

    // 初始化互斥锁
    pthread_mutex_init(&service->mutex, NULL);

    // 设置上下文
    service->context = context;
    service->state = WAC_STATE_DISABLED;
    service->running = false;
    service->server_fd = -1;
    service->session.accessory_count = 0;
    service->session.uptime = 0;
    service->session.pairing_attempts = 0;
    service->session.successful_pairings = 0;

    // 复制配置
    service->config = *config;
    if (!service->config.device_name[0]) {
        strncpy(service->config.device_name, "RealTimeWAC", sizeof(service->config.device_name)-1);
    }
    if (service->config.port == 0) {
        service->config.port = 10020;
    }
    if (service->config.pairing_timeout == 0) {
        service->config.pairing_timeout = 60; // 默认60秒
    }
    if (service->config.max_accessories == 0) {
        service->config.max_accessories = 10;
    }

    return service;
}

// 销毁WAC服务
void wac_destroy(WacService* service) {
    if (!service) return;

    wac_stop(service);

    // 清理服务器
    wac_cleanup_server(service);

    // 清理互斥锁
    pthread_mutex_destroy(&service->mutex);

    // 清理D-Bus
    dbus_cleanup();

    free(service);
}

// 启动WAC服务
int wac_start(WacService* service) {
    if (!service || service->running) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 设置状态为启用
    wac_set_state(service, WAC_STATE_ENABLED);

    // 创建服务器套接字
    if (wac_setup_server(service) < 0) {
        fprintf(stderr, "Failed to setup WAC server\n");
        wac_set_state(service, WAC_STATE_ERROR);
        snprintf(service->error_msg, sizeof(service->error_msg), "Failed to setup server: %s", strerror(errno));
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 设置运行标志
    service->running = true;

    // 记录启动时间
    service->session.uptime = time(NULL);

    // 创建工作线程
    if (pthread_create(&service->thread, NULL, wac_worker_thread, service) != 0) {
        fprintf(stderr, "Failed to create WAC worker thread\n");
        service->running = false;
        wac_cleanup_server(service);
        wac_set_state(service, WAC_STATE_ERROR);
        snprintf(service->error_msg, sizeof(service->error_msg), "Failed to create thread: %s", strerror(errno));
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    pthread_mutex_unlock(&service->mutex);
    return 0;
}

// 停止WAC服务
void wac_stop(WacService* service) {
    if (!service || !service->running) {
        return;
    }

    pthread_mutex_lock(&service->mutex);
    service->running = false;
    pthread_mutex_unlock(&service->mutex);

    // 等待线程结束
    pthread_join(service->thread, NULL);

    // 清理服务器
    wac_cleanup_server(service);

    // 重置会话信息
    service->session.accessory_count = 0;
    memset(service->session.accessories, 0, sizeof(service->session.accessories));
    service->session.uptime = 0;
    service->session.pairing_attempts = 0;
    service->session.successful_pairings = 0;

    // 设置状态为已禁用
    wac_set_state(service, WAC_STATE_DISABLED);
}

// 开始配对过程
int wac_start_pairing(WacService* service) {
    if (!service || service->state != WAC_STATE_ENABLED) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);
    if (service->state == WAC_STATE_PAIRING) {
        pthread_mutex_unlock(&service->mutex);
        return 0; // 已经在配对中
    }
    pthread_mutex_unlock(&service->mutex);

    // 设置配对状态
    wac_set_state(service, WAC_STATE_PAIRING);

    // 创建配对超时线程
    // 简化实现，实际项目中应该使用定时器

    return 0;
}

// 结束配对过程
void wac_stop_pairing(WacService* service) {
    if (!service || service->state != WAC_STATE_PAIRING) {
        return;
    }

    // 恢复到启用状态
    wac_set_state(service, WAC_STATE_ENABLED);
}

// 配对指定配件
int wac_pair_accessory(WacService* service, const char* identifier, const char* ip_address, uint16_t port) {
    if (!service || !identifier || !ip_address || service->state != WAC_STATE_PAIRING) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 检查是否已达到最大配件数量
    if (service->session.accessory_count >= service->config.max_accessories) {
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 检查配件是否已存在
    int index = wac_find_accessory_index(service, identifier);
    if (index >= 0) {
        // 更新现有配件信息
        strncpy(service->session.accessories[index].ip_address, ip_address, sizeof(service->session.accessories[index].ip_address)-1);
        service->session.accessories[index].port = port;
        service->session.accessories[index].connected = true;
        service->session.accessories[index].last_seen = time(NULL);
        pthread_mutex_unlock(&service->mutex);
        return 0;
    }

    // 添加新配件
    index = service->session.accessory_count;
    strncpy(service->session.accessories[index].identifier, identifier, sizeof(service->session.accessories[index].identifier)-1);
    snprintf(service->session.accessories[index].name, sizeof(service->session.accessories[index].name), "Accessory-%d", index+1);
    service->session.accessories[index].type = WAC_ACCESSORY_TYPE_OTHER;
    strncpy(service->session.accessories[index].ip_address, ip_address, sizeof(service->session.accessories[index].ip_address)-1);
    service->session.accessories[index].port = port;
    service->session.accessories[index].paired_time = time(NULL);
    service->session.accessories[index].connected = true;
    service->session.accessories[index].last_seen = time(NULL);
    service->session.accessory_count++;
    service->session.successful_pairings++;

    pthread_mutex_unlock(&service->mutex);

    // 更新状态为已配对
    wac_set_state(service, WAC_STATE_PAIRED);

    // 发送配对成功D-Bus信号
    json_t* details = json_object();
    json_object_set_new(details, "identifier", json_string(identifier));
    json_object_set_new(details, "name", json_string(service->session.accessories[index].name));
    json_object_set_new(details, "ip_address", json_string(ip_address));
    json_object_set_new(details, "port", json_integer(port));
    json_object_set_new(details, "accessory_count", json_integer(service->session.accessory_count));
    json_object_set_new(details, "timestamp", json_integer(time(NULL)));

    const char* json_str = json_dumps(details, JSON_COMPACT);
    if (json_str) {
        dbus_emit_signal("com.realtimeaudio.WAC", DBUS_SIGNAL_TYPE_PAIRED, json_str);
        free((void*)json_str);
    }
    json_decref(details);

    return 0;
}

// 解除与配件的配对
int wac_unpair_accessory(WacService* service, const char* identifier) {
    if (!service || !identifier || service->state == WAC_STATE_DISABLED) {
        return -1;
    }

    pthread_mutex_lock(&service->mutex);

    // 查找配件
    int index = wac_find_accessory_index(service, identifier);
    if (index < 0) {
        pthread_mutex_unlock(&service->mutex);
        return -1;
    }

    // 保存要移除的配件信息
    char name[64];
    strncpy(name, service->session.accessories[index].name, sizeof(name)-1);
    char ip_address[46];
    strncpy(ip_address, service->session.accessories[index].ip_address, sizeof(ip_address)-1);

    // 移除配件（将最后一个配件移到当前位置）
    service->session.accessory_count--;
    if (index < service->session.accessory_count) {
        service->session.accessories[index] = service->session.accessories[service->session.accessory_count];
    }
    memset(&service->session.accessories[service->session.accessory_count], 0, sizeof(WacAccessory));

    // 如果没有配件了，更新状态
    if (service->session.accessory_count == 0 && service->state == WAC_STATE_PAIRED) {
        pthread_mutex_unlock(&service->mutex);
        wac_set_state(service, WAC_STATE_ENABLED);
        pthread_mutex_lock(&service->mutex);
    }

    pthread_mutex_unlock(&service->mutex);

    // 发送解除配对D-Bus信号
    json_t* details = json_object();
    json_object_set_new(details, "identifier", json_string(identifier));
    json_object_set_new(details, "name", json_string(name));
    json_object_set_new(details, "ip_address", json_string(ip_address));
    json_object_set_new(details, "accessory_count", json_integer(service->session.accessory_count));
    json_object_set_new(details, "timestamp", json_integer(time(NULL)));

    const char* json_str = json_dumps(details, JSON_COMPACT);
    if (json_str) {
        dbus_emit_signal("com.realtimeaudio.WAC", DBUS_SIGNAL_TYPE_UNPAIRED, json_str);
        free((void*)json_str);
    }
    json_decref(details);

    return 0;
}

// 获取当前状态
WacState wac_get_state(WacService* service) {
    if (!service) {
        return WAC_STATE_ERROR;
    }

    WacState state;
    pthread_mutex_lock(&service->mutex);
    state = service->state;
    pthread_mutex_unlock(&service->mutex);
    return state;
}

// 获取会话信息
const WacSession* wac_get_session(WacService* service) {
    if (!service || service->state == WAC_STATE_DISABLED || service->state == WAC_STATE_ERROR) {
        return NULL;
    }

    return &service->session;
}

// 获取指定配件信息
const WacAccessory* wac_get_accessory(WacService* service, const char* identifier) {
    if (!service || !identifier || service->state == WAC_STATE_DISABLED || service->state == WAC_STATE_ERROR) {
        return NULL;
    }

    pthread_mutex_lock(&service->mutex);
    int index = wac_find_accessory_index(service, identifier);
    const WacAccessory* accessory = (index >= 0) ? &service->session.accessories[index] : NULL;
    pthread_mutex_unlock(&service->mutex);

    return accessory;
}

// 获取错误信息
const char* wac_get_error(WacService* service) {
    if (!service) {
        return "Invalid service pointer";
    }

    return service->error_msg;
}

// 设置状态
static void wac_set_state(WacService* service, WacState state) {
    if (!service) return;

    pthread_mutex_lock(&service->mutex);
    WacState old_state = service->state;
    service->state = state;
    pthread_mutex_unlock(&service->mutex);

    // 只有状态实际改变时才发送信号
    if (old_state != state) {
        // 创建JSON payload
        json_t* details = json_object();
        json_object_set_new(details, "old_state", json_integer(old_state));
        json_object_set_new(details, "new_state", json_integer(state));
        json_object_set_new(details, "device_name", json_string(service->config.device_name));
        json_object_set_new(details, "accessory_count", json_integer(service->session.accessory_count));
        json_object_set_new(details, "timestamp", json_integer(time(NULL)));

        const char* json_str = json_dumps(details, JSON_COMPACT);
        if (json_str) {
            // 发送D-Bus信号
            dbus_emit_signal("com.realtimeaudio.WAC", DBUS_SIGNAL_TYPE_STATE_CHANGED, json_str);
            free((void*)json_str);
        }
        json_decref(details);

        // 日志记录
        printf("WAC state changed from %d to %d\n", old_state, state);
    }
}

// 查找配件索引
static int wac_find_accessory_index(WacService* service, const char* identifier) {
    if (!service || !identifier) return -1;

    for (int i = 0; i < service->session.accessory_count; i++) {
        if (strcmp(service->session.accessories[i].identifier, identifier) == 0) {
            return i;
        }
    }
    return -1;
}

// 更新运行时间
static void wac_update_uptime(WacService* service) {
    if (!service || service->state == WAC_STATE_DISABLED) {
        return;
    }

    pthread_mutex_lock(&service->mutex);
    time_t current_time = time(NULL);
    service->session.uptime = current_time - service->session.uptime;
    pthread_mutex_unlock(&service->mutex);
}

// 工作线程函数
static void* wac_worker_thread(void* data) {
    WacService* service = (WacService*)data;
    fd_set read_fds;
    struct timeval timeout;
    int max_fd;

    while (1) {
        pthread_mutex_lock(&service->mutex);
        bool running = service->running;
        pthread_mutex_unlock(&service->mutex);

        if (!running) {
            break;
        }

        // 更新运行时间
        wac_update_uptime(service);

        // 设置文件描述符集
        FD_ZERO(&read_fds);
        max_fd = service->server_fd;

        if (service->server_fd >= 0) {
            FD_SET(service->server_fd, &read_fds);
        }

        // 设置超时
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // 等待活动
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0 && errno != EINTR) {
            fprintf(stderr, "WAC select error: %s\n", strerror(errno));
            break;
        } else if (activity > 0) {
            // 服务器套接字有活动 - 处理配对请求
            if (service->server_fd >= 0 && FD_ISSET(service->server_fd, &read_fds)) {
                struct sockaddr_in address;
                socklen_t addrlen = sizeof(address);
                int new_socket = accept(service->server_fd, (struct sockaddr*)&address, &addrlen);

                if (new_socket < 0) {
                    perror("accept");
                    continue;
                }

                // 模拟配件配对请求
                char identifier[32];
                snprintf(identifier, sizeof(identifier), "WAC-%08X", rand() % 0xFFFFFFFF);
                char ip_addr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(address.sin_addr), ip_addr, INET_ADDRSTRLEN);
                uint16_t port = ntohs(address.sin_port);

                // 如果自动接受配对或正在配对模式
                pthread_mutex_lock(&service->mutex);
                bool auto_accept = service->config.auto_accept;
                WacState current_state = service->state;
                pthread_mutex_unlock(&service->mutex);

                if (auto_accept || current_state == WAC_STATE_PAIRING) {
                    // 接受配对
                    wac_pair_accessory(service, identifier, ip_addr, port);
                } else {
                    // 拒绝配对
                    pthread_mutex_lock(&service->mutex);
                    service->session.pairing_attempts++;
                    pthread_mutex_unlock(&service->mutex);
                }

                close(new_socket);
            }
        }

        // 检查配件连接状态
        pthread_mutex_lock(&service->mutex);
        time_t now = time(NULL);
        for (int i = 0; i < service->session.accessory_count; i++) {
            // 简化处理：假设配件5秒内没有活动则断开连接
            if (service->session.accessories[i].connected && now - service->session.accessories[i].last_seen > 5) {
                service->session.accessories[i].connected = false;
                // 发送连接状态变化信号
                json_t* details = json_object();
                json_object_set_new(details, "identifier", json_string(service->session.accessories[i].identifier));
                json_object_set_new(details, "name", json_string(service->session.accessories[i].name));
                json_object_set_new(details, "connected", json_false());
                json_object_set_new(details, "timestamp", json_integer(now));

                const char* json_str = json_dumps(details, JSON_COMPACT);
                if (json_str) {
                    dbus_emit_signal("com.realtimeaudio.WAC", DBUS_SIGNAL_TYPE_CONNECTION_CHANGED, json_str);
                    free((void*)json_str);
                }
                json_decref(details);
            }
        }
        pthread_mutex_unlock(&service->mutex);

        // 每秒检查一次
        sleep(1);
    }

    return NULL;
}

// 设置服务器套接字
static int wac_setup_server(WacService* service) {
    struct sockaddr_in address;
    int opt = 1;

    // 创建套接字文件描述符
    if ((service->server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        return -1;
    }

    // 设置套接字选项
    if (setsockopt(service->server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(service->server_fd);
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(service->config.port);

    // 绑定套接字到端口
    if (bind(service->server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(service->server_fd);
        return -1;
    }

    // 监听连接
    if (listen(service->server_fd, 5) < 0) {
        perror("listen");
        close(service->server_fd);
        return -1;
    }

    return 0;
}

// 清理服务器
static void wac_cleanup_server(WacService* service) {
    if (service->server_fd >= 0) {
        close(service->server_fd);
        service->server_fd = -1;
    }
}