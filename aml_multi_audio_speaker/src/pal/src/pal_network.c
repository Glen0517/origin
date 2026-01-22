/**
 * @file pal_network.c
 * @brief 网络服务抽象实现
 * @details 实现网络服务抽象层的接口函数，封装网络相关的服务和功能
 * @author AML Audio Team
 * @date 2026-01-16
 */

#include "pal_network.h"
#include "log/aml_log.h"

// 定义网络服务模块日志分类
AML_LOG_DEFINE(network_log);
// 设置默认日志分类
#define AML_LOG_DEFAULT AML_LOG_GET_CAT(network_log)

static bool g_network_init = false;

/**
 * @brief 初始化网络服务
 * @details 初始化网络服务模块
 * @return 初始化结果：0表示成功，非0表示失败
 */
int pal_network_init(void) {
    if (g_network_init) {
        AML_LOGI("PAL network already initialized");
        return SUCCESS;
    }

    g_network_init = true;
    AML_LOGI("PAL network init success");
    return SUCCESS;
}

/**
 * @brief 反初始化网络服务
 * @details 反初始化网络服务模块
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int pal_network_deinit(void) {
    if (!g_network_init) {
        AML_LOGI("PAL network not initialized");
        return SUCCESS;
    }

    g_network_init = false;
    AML_LOGI("PAL network deinit success");
    return SUCCESS;
}

/**
 * @brief 获取网络IP地址
 * @details 获取当前网络的IP地址
 * @param ip_address IP地址字符串指针，用于存储获取的IP地址
 * @param max_len IP地址字符串最大长度
 * @return 获取结果：0表示成功，非0表示失败
 */
int pal_network_get_ip_address(char *ip_address, int max_len) {
    if (!g_network_init || !ip_address || max_len <= 0) {
        AML_LOGE("Invalid parameters or not initialized");
        return FAILURE;
    }

    // 模拟实现，返回默认IP地址
    strncpy(ip_address, "192.168.1.100", max_len - 1);
    ip_address[max_len - 1] = '\0';

    AML_LOGI("IP address: %s", ip_address);
    return SUCCESS;
}

/**
 * @brief 检查网络连接状态
 * @details 检查当前网络是否连接
 * @return 连接状态：1表示已连接，0表示未连接
 */
int pal_network_is_connected(void) {
    if (!g_network_init) {
        AML_LOGE("Network not initialized");
        return 0;
    }

    // 模拟实现，返回默认连接状态
    AML_LOGI("Network connection status: CONNECTED");
    return 1;
}
