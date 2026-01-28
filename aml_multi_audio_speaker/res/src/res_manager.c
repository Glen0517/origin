#include "res_manager.h"

/******************************************************************************************
 * 静态全局变量 - 资源管理器私有，外部不可见，保证封装性，嵌入式内存友好设计
 ******************************************************************************************/
static const char *g_res_root_path = "./res/assets/";  // 资源根路径，集中管理可一键修改
static ResInfo_t g_res_cache[RES_TYPE_MAX];            // 资源缓存池，加载一次缓存永久使用
static int g_res_manager_inited = 0;                   // 资源管理器初始化标识

/******************************************************************************************
 * 私有函数声明 - 资源管理器内部调用，无外部依赖，封装所有底层细节
 ******************************************************************************************/
static int res_create_root_dir(void);
static int res_get_product_subpath(char *subpath, int buf_size);
static int res_read_file_to_mem(const char *file_path, unsigned char **data, unsigned int *size);
static void res_load_default_resource(ResInfo_t *res, ResType_e type);

/******************************************************************************************
 * 资源管理器初始化 - 仅调用一次，核心入口
 ******************************************************************************************/
int res_manager_init(void)
{
    int i = 0;
    if (g_res_manager_inited == 1)
    {
        LOG_WARN("Resource manager already inited!");
        return 0;
    }

    // 1. 创建资源根目录，不存在则自动创建
    if (res_create_root_dir() != 0)
    {
        LOG_ERROR("Resource root dir create failed: %s", g_res_root_path);
        return -1;
    }

    // 2. 初始化资源缓存池，清空所有缓存项
    memset(g_res_cache, 0, sizeof(g_res_cache));
    for (i = 0; i < RES_TYPE_MAX; i++)
    {
        g_res_cache[i].res_valid = 0;
        g_res_cache[i].res_data = NULL;
        g_res_cache[i].res_size = 0;
    }

    // 3. 打印资源加载信息，与产品类型联动
    LOG_INFO("=====================================");
    LOG_INFO("Resource Manager Init Success");
    LOG_INFO("Product Type: %d, Resource Load Level: %d", CURRENT_PRODUCT_TYPE, RES_LOAD_LEVEL);
    LOG_INFO("Resource Root Path: %s", g_res_root_path);
    LOG_INFO("=====================================");

    g_res_manager_inited = 1;
    return 0;
}

/******************************************************************************************
 * 资源管理器反初始化 - 释放所有资源，无内存泄漏
 ******************************************************************************************/
void res_manager_deinit(void)
{
    int i = 0;
    if (g_res_manager_inited == 0)
    {
        return;
    }

    // 释放所有缓存的资源数据
    for (i = 0; i < RES_TYPE_MAX; i++)
    {
        if (g_res_cache[i].res_data != NULL)
        {
            free(g_res_cache[i].res_data);
            g_res_cache[i].res_data = NULL;
        }
        g_res_cache[i].res_valid = 0;
        g_res_cache[i].res_size = 0;
    }

    LOG_INFO("Resource Manager Deinit Success, all resource released");
    g_res_manager_inited = 0;
}

/******************************************************************************************
 * 核心接口：加载指定类型的资源 - src业务层唯一核心调用接口，极简高效
 * 核心逻辑：缓存优先 → 宏控匹配产品资源 → 读取文件 → 缓存数据 → 返回资源信息
 * 资源缺失时自动加载兜底默认资源，保证程序不崩溃，嵌入式容错设计
 ******************************************************************************************/
ResInfo_t res_load_resource(ResType_e type)
{
    ResInfo_t res = {0};
    char res_path[256] = {0};

    // 边界校验：资源类型非法，返回空资源
    if (type >= RES_TYPE_MAX || g_res_manager_inited == 0)
    {
        LOG_ERROR("Invalid resource type: %d or manager not inited", type);
        res_load_default_resource(&res, type);
        return res;
    }

    // 缓存优先：已加载的资源直接返回缓存，避免重复读取FLASH，提升性能
    if (g_res_cache[type].res_valid == 1)
    {
        memcpy(&res, &g_res_cache[type], sizeof(ResInfo_t));
        return res;
    }

    // 1. 获取资源文件完整路径
    if (res_get_resource_path(type, res_path, sizeof(res_path)) != 0)
    {
        LOG_WARN("Get resource path failed for type: %d", type);
        res_load_default_resource(&res, type);
        return res;
    }

    // 2. 读取资源文件到内存
    if (res_read_file_to_mem(res_path, &res.res_data, &res.res_size) != 0)
    {
        LOG_WARN("Load resource failed: %s, load default resource", res_path);
        res_load_default_resource(&res, type);
        return res;
    }

    // 3. 填充资源信息，标记为有效
    strncpy(res.res_path, res_path, sizeof(res.res_path)-1);
    res.res_valid = 1;

    // 4. 缓存资源数据，下次调用直接返回
    memcpy(&g_res_cache[type], &res, sizeof(ResInfo_t));

    LOG_DEBUG("Load resource success: %s, size: %d bytes", res_path, res.res_size);
    return res;
}

/******************************************************************************************
 * 释放单个资源缓存
 ******************************************************************************************/
void res_free_resource(ResInfo_t *res)
{
    if (res == NULL || res->res_data == NULL)
    {
        return;
    }
    free(res->res_data);
    res->res_data = NULL;
    res->res_size = 0;
    res->res_valid = 0;
    memset(res->res_path, 0, sizeof(res->res_path));
}

/******************************************************************************************
 * 核心私有逻辑：获取资源文件完整路径 - 产品分级/模块宏控的核心实现
 * 自动匹配「产品类型+资源类型+模块开关」，拼接出对应的资源路径，无任何硬编码
 * 这是实现「src零修改、宏控加载资源」的核心函数，所有资源路径的逻辑都在这里
 ******************************************************************************************/
int res_get_resource_path(ResType_e type, char *path_buf, int buf_size)
{
    char product_subpath[64] = {0};
    char res_name[128] = {0};

    if (path_buf == NULL || buf_size <= 0)
    {
        return -1;
    }

    // 1. 获取产品对应的资源子路径（high_end/mid_end/low_end/subwoofer）
    res_get_product_subpath(product_subpath, sizeof(product_subpath));

    // 2. 根据资源类型匹配资源文件名，标准化命名，无硬编码
    switch (type)
    {
        case RES_TYPE_TONE_BOOT:        strcpy(res_name, "tone/boot.wav"); break;
        case RES_TYPE_TONE_CONNECT:     strcpy(res_name, "tone/bt_connect.wav"); break;
        case RES_TYPE_TONE_DISCONNECT:  strcpy(res_name, "tone/bt_disconnect.wav"); break;
        case RES_TYPE_TONE_VOL_UP:      strcpy(res_name, "tone/vol_up.wav"); break;
        case RES_TYPE_TONE_VOL_DOWN:    strcpy(res_name, "tone/vol_down.wav"); break;
        case RES_TYPE_TONE_SRC_SWITCH:  strcpy(res_name, "tone/src_switch.wav"); break;
        case RES_TYPE_TONE_ERROR:       strcpy(res_name, "tone/error.wav"); break;
        case RES_TYPE_FONT_DEFAULT:     strcpy(res_name, "font/default_font.bin"); break;
        case RES_TYPE_FONT_HD:          strcpy(res_name, "font/hd_font.bin"); break;
        case RES_TYPE_EQ_DEFAULT:       strcpy(res_name, "eq/default_eq.json"); break;
        case RES_TYPE_EQ_ROCK:          strcpy(res_name, "eq/rock_eq.json"); break;
        case RES_TYPE_EQ_CLASSIC:       strcpy(res_name, "eq/classic_eq.json"); break;
        case RES_TYPE_EQ_DOLBY:         RES_LOAD_DOLBY_EQ ? strcpy(res_name, "eq/dolby_eq.json") : strcpy(res_name, "eq/default_eq.json"); break;
        case RES_TYPE_EQ_BASS:          RES_LOAD_BASS_EQ ? strcpy(res_name, "eq/bass_eq.json") : strcpy(res_name, "eq/default_eq.json"); break;
        case RES_TYPE_ICON_BT:          strcpy(res_name, "icon/bt_icon.bin"); break;
        case RES_TYPE_ICON_HDMI:        RES_LOAD_HDMI_ICON ? strcpy(res_name, "icon/hdmi_icon.bin") : strcpy(res_name, "icon/bt_icon.bin"); break;
        case RES_TYPE_ICON_SPDIF:       RES_LOAD_SPDIF_ICON ? strcpy(res_name, "icon/spdif_icon.bin") : strcpy(res_name, "icon/bt_icon.bin"); break;
        case RES_TYPE_ICON_USB:         strcpy(res_name, "icon/usb_icon.bin"); break;
        default:                        strcpy(res_name, "tone/error.wav"); break;
    }

    // 3. 拼接完整路径：根路径 + 产品子路径 + 资源名，Linux标准路径格式
    snprintf(path_buf, buf_size-1, "%s%s/%s", g_res_root_path, product_subpath, res_name);

    // 4. 兜底逻辑：产品专属资源不存在，则加载公共资源
    if (access(path_buf, F_OK) != 0)
    {
        snprintf(path_buf, buf_size-1, "%scommon/%s", g_res_root_path, res_name);
    }

    return 0;
}

/******************************************************************************************
 * 私有函数：获取产品对应的资源子路径
 ******************************************************************************************/
static int res_get_product_subpath(char *subpath, int buf_size)
{
    if (subpath == NULL || buf_size <= 0)
    {
        return -1;
    }

    switch (CURRENT_PRODUCT_TYPE)
    {
        case PRODUCT_HIGH_END_SOUNDBAR:  strcpy(subpath, "high_end"); break;
        case PRODUCT_MID_END_SOUNDBAR:   strcpy(subpath, "mid_end"); break;
        case PRODUCT_LOW_END_BT_SPEAKER: strcpy(subpath, "low_end"); break;
        case PRODUCT_SUBWOOFER: strcpy(subpath, "subwoofer"); break;
        default:                strcpy(subpath, "common"); break;
    }
    return 0;
}

/******************************************************************************************
 * 私有函数：读取文件到内存，嵌入式通用文件读取逻辑，支持二进制/文本文件
 ******************************************************************************************/
static int res_read_file_to_mem(const char *file_path, unsigned char **data, unsigned int *size)
{
    FILE *fp = NULL;
    struct stat file_stat;

    if (file_path == NULL || data == NULL || size == NULL)
    {
        return -1;
    }

    if (stat(file_path, &file_stat) != 0)
    {
        LOG_ERROR("File stat failed: %s", file_path);
        return -1;
    }

    *size = file_stat.st_size;
    *data = (unsigned char *)malloc(*size);
    if (*data == NULL)
    {
        LOG_ERROR("Malloc failed for resource: %s, size: %d", file_path, *size);
        return -1;
    }

    fp = fopen(file_path, "rb");
    if (fp == NULL)
    {
        LOG_ERROR("File open failed: %s", file_path);
        free(*data);
        *data = NULL;
        *size = 0;
        return -1;
    }

    if (fread(*data, 1, *size, fp) != *size)
    {
        LOG_ERROR("File read failed: %s", file_path);
        free(*data);
        fclose(fp);
        *data = NULL;
        *size = 0;
        return -1;
    }

    fclose(fp);
    return 0;
}

/******************************************************************************************
 * 私有函数：加载兜底默认资源，嵌入式容错核心逻辑，资源缺失时保证程序不崩溃
 ******************************************************************************************/
static void res_load_default_resource(ResInfo_t *res, ResType_e type)
{
    if (res == NULL)
    {
        return;
    }
    res->res_valid = 0;
    res->res_data = NULL;
    res->res_size = 0;
    strcpy(res->res_path, "default_resource");

    // 提示音类资源缺失，返回空提示音
    if (type >= RES_TYPE_TONE_BOOT && type <= RES_TYPE_TONE_ERROR)
    {
        res->res_size = 0;
    }
    // EQ类资源缺失，返回默认EQ
    else if (type >= RES_TYPE_EQ_DEFAULT && type <= RES_TYPE_EQ_BASS)
    {
        res->res_data = (unsigned char *)malloc(strlen("{\"eq\":\"default\"}")+1);
        strcpy((char *)res->res_data, "{\"eq\":\"default\"}");
        res->res_size = strlen("{\"eq\":\"default\"}")+1;
        res->res_valid = 1;
    }
}