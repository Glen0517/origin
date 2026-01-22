#ifndef NETWORK_MUSIC_H
#define NETWORK_MUSIC_H

#include <stddef.h>

/**
 * @brief 初始化网络音乐服务
 * @return 0 成功，-1 失败
 */
int network_music_init(void);

/**
 * @brief 反初始化网络音乐服务
 */
void network_music_deinit(void);

/**
 * @brief 获取网络音乐的元数据
 * @param url 音乐URL
 * @param metadata 返回的元数据
 * @return 0 成功，-1 失败
 */
int network_music_get_metadata(const char *url, char **metadata);

/**
 * @brief 流式播放网络音乐
 * @param url 音乐URL
 * @param data_callback 数据回调函数
 * @return 0 成功，-1 失败
 */
int network_music_stream(const char *url, void (*data_callback)(const char *, size_t));

/**
 * @brief 搜索网络音乐
 * @param query 搜索关键词
 * @param results 返回的搜索结果
 * @return 0 成功，-1 失败
 */
int network_music_search(const char *query, char **results);

#endif /* NETWORK_MUSIC_H */