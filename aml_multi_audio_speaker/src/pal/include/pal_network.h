/**
 * @file pal_network.h
 * @brief 网络服务抽象接口
 * @details 定义网络服务抽象层的接口函数，封装网络相关的服务和功能
 * @author AML Audio Team
 * @date 2026-01-16
 */

#ifndef PAL_NETWORK_H
#define PAL_NETWORK_H

#include "common_def.h"

/**
 * @brief 初始化网络服务
 * @details 初始化网络服务模块
 * @return 初始化结果：0表示成功，非0表示失败
 */
int pal_network_init(void);

/**
 * @brief 反初始化网络服务
 * @details 反初始化网络服务模块
 * @return 反初始化结果：0表示成功，非0表示失败
 */
int pal_network_deinit(void);

/**
 * @brief 获取IP地址
 * @details 获取指定网络接口的IP地址
 * @param interface 网络接口名，如"eth0"、"wlan0"
 * @param ip IP地址缓冲区，用于存储获取的IP地址，至少需要16字节空间
 * @return 获取结果：0表示成功，非0表示失败
 */
int pal_network_get_ip_address(const char *interface, char *ip);

/**
 * @brief 检查网络连接状态
 * @details 检查指定网络接口的连接状态
 * @param interface 网络接口名，如"eth0"、"wlan0"
 * @return 连接状态：1表示已连接，0表示未连接
 */
int pal_network_is_connected(const char *interface);

/**
 * @brief 启动DHCP服务
 * @details 为指定的网络接口启动DHCP服务，获取动态IP地址
 * @param interface 网络接口名，如"eth0"、"wlan0"
 * @return 启动结果：0表示成功，非0表示失败
 */
int pal_network_start_dhcp(const char *interface);

/**
 * @brief 停止DHCP服务
 * @details 为指定的网络接口停止DHCP服务
 * @param interface 网络接口名，如"eth0"、"wlan0"
 * @return 停止结果：0表示成功，非0表示失败
 */
int pal_network_stop_dhcp(const char *interface);

/**
 * @brief 设置静态IP地址
 * @details 为指定的网络接口设置静态IP地址
 * @param interface 网络接口名，如"eth0"、"wlan0"
 * @param ip IP地址，如"192.168.1.100"
 * @param netmask 子网掩码，如"255.255.255.0"
 * @param gateway 网关地址，如"192.168.1.1"
 * @return 设置结果：0表示成功，非0表示失败
 */
int pal_network_set_static_ip(const char *interface, const char *ip, const char *netmask, const char *gateway);

/**
 * @brief 解析域名
 * @details 将域名解析为IP地址
 * @param domain 域名，如"www.example.com"
 * @param ip IP地址缓冲区，用于存储解析得到的IP地址，至少需要16字节空间
 * @return 解析结果：0表示成功，非0表示失败
 */
int pal_network_resolve_domain(const char *domain, char *ip);

#endif /* PAL_NETWORK_H */