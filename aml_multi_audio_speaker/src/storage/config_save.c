#include "config_save.h"
#include <sys/mman.h>
#include <fcntl.h>

/******************************************************************************************
 * 全局变量定义
 ******************************************************************************************/
static GlobalConfig_t g_global_cfg;  // 全局配置结构体实例
static cJSON *cjson_root = NULL;

/******************************************************************************************
 * 内部函数声明 - 私有化，外部不可调用
 ******************************************************************************************/
static void config_set_default_value(void);
static int config_load_product_cap(void);
static int config_load_audio_cfg(void);
static int config_load_play_cfg(void);
static int config_load_source_cfg(void);
static int config_load_key_cfg(void);
static int config_load_eq_cfg(void);
static int config_load_hdmi_arc_cfg(void);
static int config_load_spdif_cfg(void);
static int config_load_subwoofer_cfg(void);
static int config_load_flash_calib_param(void);
static cJSON *cjson_open_file(const char *file_path);

/******************************************************************************************
 * ======================== FLASH 校准参数 内部工具函数 (START) ========================
 * 私有函数，外部不可调用：CRC32校验计算、参数合法性校验、FLASH底层封装
 ******************************************************************************************/
#define CRC32_INIT_VALUE  0xFFFFFFFF
#define CRC32_POLYNOMIAL  0x04C11DB7

// ===================== AML FLASH驱动兼容适配层 (START) =====================
// 适配不同AML SDK版本的FLASH驱动接口，无需修改核心逻辑
#ifndef aml_flash_read
#define aml_flash_read    spi_flash_read
#define aml_flash_write   spi_flash_write
#define aml_flash_erase   spi_flash_erase
#endif

// 如果SDK中连spi_flash接口都没有，使用mmap方式的FLASH操作（AML SOC通用）
#ifndef spi_flash_read
#define FLASH_DEV_PATH    "/dev/mtdblock1" // AML FLASH设备节点，根据实际修改
static int flash_fd = -1;
static unsigned char *flash_mmap = NULL;

// 初始化FLASH映射
static int flash_mmap_init(void)
{
    flash_fd = open(FLASH_DEV_PATH, O_RDWR);
    if (flash_fd < 0) return -1;
    flash_mmap = mmap(NULL, FLASH_CALIB_TOTAL_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, flash_fd, FLASH_CALIB_START_ADDR);
    return (flash_mmap == MAP_FAILED) ? -1 : 0;
}

// 封装读接口
int spi_flash_read(unsigned int addr, unsigned char *buf, unsigned int len)
{
    if (flash_mmap_init() !=0) return -1;
    memcpy(buf, flash_mmap + (addr - FLASH_CALIB_START_ADDR), len);
    munmap(flash_mmap, FLASH_CALIB_TOTAL_SIZE);
    close(flash_fd);
    return 0;
}

// 封装写接口
int spi_flash_write(unsigned int addr, unsigned char *buf, unsigned int len)
{
    if (flash_mmap_init() !=0) return -1;
    memcpy(flash_mmap + (addr - FLASH_CALIB_START_ADDR), buf, len);
    munmap(flash_mmap, FLASH_CALIB_TOTAL_SIZE);
    close(flash_fd);
    return 0;
}

// 封装擦除接口
int spi_flash_erase(unsigned int addr, unsigned int len)
{
    if (flash_mmap_init() !=0) return -1;
    memset(flash_mmap + (addr - FLASH_CALIB_START_ADDR), 0xFF, len);
    munmap(flash_mmap, FLASH_CALIB_TOTAL_SIZE);
    close(flash_fd);
    return 0;
}
#endif
// ===================== AML FLASH驱动兼容适配层 (END) =====================
/**
 * @brief  工业级CRC32校验计算 - 计算校准参数的校验和，防止FLASH存储数据损坏
 * @param  p_data 数据指针
 * @param  len    数据长度
 * @return 计算后的CRC32值
 */
static unsigned int flash_calib_crc32_calc(const unsigned char *p_data, unsigned int len)
{
    unsigned int crc = CRC32_INIT_VALUE;
    unsigned int i, j;

    for (i = 0; i < len; i++)
    {
        crc ^= ((unsigned int)p_data[i]) << 24;
        for (j = 0; j < 8; j++)
        {
            if (crc & 0x80000000)
            {
                crc = (crc << 1) ^ CRC32_POLYNOMIAL;
            }
            else
            {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief  校准参数合法性校验 - 所有参数必须在合法范围内，非法参数直接拒绝写入/加载
 * @param  p_param 校准参数结构体指针
 * @return 0:合法  -1:非法
 */
static int flash_calib_param_check(FlashCalibParam_t *p_param)
{
    int i = 0;
    if (p_param == NULL) return -1;

    // 1. 产品类型匹配校验，防止跨机型刷写
#if CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END_SOUNDBAR
    if (p_param->product_type_match != CALIB_MATCH_HIGH_END) return -1;
#elif CURRENT_PRODUCT_TYPE == PRODUCT_MID_END_SOUNDBAR
    if (p_param->product_type_match != CALIB_MATCH_MID_END) return -1;
#elif CURRENT_PRODUCT_TYPE == PRODUCT_LOW_END_BT_SPEAKER
    if (p_param->product_type_match != CALIB_MATCH_LOW_END) return -1;
#elif CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER
    if (p_param->product_type_match != CALIB_MATCH_SUBWOOFER) return -1;
#endif

    // 2. EQ增益校验
    for (i = 0; i < 9; i++)
    {
        if (p_param->eq_gain[i] < CALIB_EQ_GAIN_MIN || p_param->eq_gain[i] > CALIB_EQ_GAIN_MAX)
        {
            return -1;
        }
    }

    // 3. 高低音、音量、声道平衡校验
    if (p_param->bass_gain < CALIB_BASS_GAIN_MIN || p_param->bass_gain > CALIB_BASS_GAIN_MAX) return -1;
    if (p_param->treble_gain < CALIB_TREBLE_GAIN_MIN || p_param->treble_gain > CALIB_TREBLE_GAIN_MAX) return -1;
    if (p_param->master_volume_gain < CALIB_VOLUME_GAIN_MIN || p_param->master_volume_gain > CALIB_VOLUME_GAIN_MAX) return -1;
    if (p_param->channel_balance < CALIB_CHANNEL_BALANCE_MIN || p_param->channel_balance > CALIB_CHANNEL_BALANCE_MAX) return -1;

    return 0;
}

/**
 * @brief  AML FLASH底层读封装 - 对接SDK原生驱动，统一错误处理
 */
static int aml_flash_read_wrapper(unsigned int addr, unsigned char *buf, unsigned int len)
{
    int ret = 0;
    ret = aml_flash_read(addr, buf, len); // AML SDK原生接口，直接调用
    if (ret != 0)
    {
        printf("[FLASH] Read fail! addr:0x%08X, len:%d, ret:%d\n", addr, len, ret);
        return -1;
    }
    return 0;
}

/**
 * @brief  AML FLASH底层写封装 - 对接SDK原生驱动，FLASH写之前必须先擦除
 */
static int aml_flash_write_wrapper(unsigned int addr, unsigned char *buf, unsigned int len)
{
    int ret = 0;
    ret = aml_flash_write(addr, buf, len); // AML SDK原生接口，直接调用
    if (ret != 0)
    {
        printf("[FLASH] Write fail! addr:0x%08X, len:%d, ret:%d\n", addr, len, ret);
        return -1;
    }
    return 0;
}

/**
 * @brief  AML FLASH底层擦除封装 - 4K扇区对齐擦除，AML FLASH强制要求
 */
static int aml_flash_erase_wrapper(unsigned int addr, unsigned int len)
{
    int ret = 0;
    ret = aml_flash_erase(addr, len); // AML SDK原生接口，直接调用
    if (ret != 0)
    {
        printf("[FLASH] Erase fail! addr:0x%08X, len:%d, ret:%d\n", addr, len, ret);
        return -1;
    }
    return 0;
}

/******************************************************************************************
 * ======================== FLASH 校准参数 对外接口实现 (START) ========================
 * 公开接口，产线工具/业务模块直接调用，核心接口，无依赖，可独立调用
 ******************************************************************************************/
/**
 * @brief  FLASH校准参数读取接口 - 核心读接口
 */
int flash_calib_param_read(FlashCalibParam_t *p_param)
{
    unsigned int crc_calc = 0;
    unsigned int crc_read = 0;
    unsigned char read_buf[sizeof(FlashCalibParam_t)] = {0};

    if (p_param == NULL) return -1;

    // 1. 从FLASH读取校准参数
    if (aml_flash_read_wrapper(FLASH_CALIB_START_ADDR, read_buf, sizeof(FlashCalibParam_t)) != 0)
    {
        return -1;
    }

    // 2. 拷贝到参数结构体
    memcpy(p_param, read_buf, sizeof(FlashCalibParam_t));

    // 3. 读取存储的校验和，计算实际校验和
    crc_read = p_param->crc32_checksum;
    crc_calc = flash_calib_crc32_calc(read_buf, sizeof(FlashCalibParam_t) - sizeof(unsigned int));

    // 4. CRC校验：校验失败则参数无效
    if (crc_calc != crc_read)
    {
        printf("[FLASH] CRC32 check fail! calc:0x%08X, read:0x%08X\n", crc_calc, crc_read);
        return -2;
    }

    // 5. 参数合法性校验
    if (flash_calib_param_check(p_param) != 0)
    {
        printf("[FLASH] Param check fail!\n");
        return -1;
    }

    printf("[FLASH] Read calib param success! Bass gain:%d, Treble gain:%d\n", p_param->bass_gain, p_param->treble_gain);
    return 0;
}

/**
 * @brief  FLASH校准参数写入接口 - 产线专用核心接口，一键写入，自动处理擦除+校验+CRC
 */
int flash_calib_param_write(FlashCalibParam_t *p_param)
{
    unsigned int crc_calc = 0;
    unsigned char write_buf[sizeof(FlashCalibParam_t)] = {0};

    if (p_param == NULL) return -1;

    // 1. 第一步：参数合法性校验，非法直接返回
    if (flash_calib_param_check(p_param) != 0)
    {
        printf("[FLASH] Write param invalid!\n");
        return -1;
    }

    // 2. 第二步：擦除FLASH扇区 (AML FLASH 必须先擦后写，4K对齐)
    if (aml_flash_erase_wrapper(FLASH_CALIB_START_ADDR, FLASH_CALIB_SECTOR_SIZE) != 0)
    {
        return -2;
    }

    // 3. 第三步：计算CRC32校验和，填充到参数结构体
    memcpy(write_buf, p_param, sizeof(FlashCalibParam_t) - sizeof(unsigned int));
    crc_calc = flash_calib_crc32_calc(write_buf, sizeof(FlashCalibParam_t) - sizeof(unsigned int));
    p_param->crc32_checksum = crc_calc;
    memcpy(write_buf, p_param, sizeof(FlashCalibParam_t));

    // 4. 第四步：写入FLASH
    if (aml_flash_write_wrapper(FLASH_CALIB_START_ADDR, write_buf, sizeof(FlashCalibParam_t)) != 0)
    {
        return -3;
    }

    printf("[FLASH] Write calib param success! CRC32:0x%08X\n", crc_calc);
    return 0;
}

/**
 * @brief  FLASH校准参数擦除接口 - 产线返修/恢复出厂专用，清除校准参数
 */
int flash_calib_param_erase(void)
{
    if (aml_flash_erase_wrapper(FLASH_CALIB_START_ADDR, FLASH_CALIB_SECTOR_SIZE) != 0)
    {
        return -1;
    }
    printf("[FLASH] Erase calib param success!\n");
    return 0;
}

/**
 * @brief  加载FLASH校准参数到全局配置 - 对接原有初始化逻辑，优先级最高，覆盖JSON配置
 * ！！！替换原有同名函数，原函数删除即可
 */
int config_load_flash_calib_param(void)
{
    FlashCalibParam_t calib_param = {0};
    GlobalConfig_t *g_cfg = &g_global_cfg;
    EqCfg_t *eq = &g_cfg->eq;
    AudioCfg_t *audio = &g_cfg->audio;
    PlayCfg_t *play = &g_cfg->play;

    // 1. 跳过校准：如果产品不支持校准，则直接返回
    if (g_cfg->product_cap.enable_audio_calib != 1)
    {
        return -1;
    }

    // 2. 读取FLASH校准参数
    if (flash_calib_param_read(&calib_param) != 0)
    {
        printf("[CONFIG] No valid FLASH calib param, use JSON config!\n");
        return -1;
    }

    // 3. 核心逻辑：FLASH参数覆盖全局配置【优先级最高】
    // EQ增益覆盖
    for (int i = 0; i < eq->eq_band_cnt && i <9; i++)
    {
        eq->eq_preset[0][i] = calib_param.eq_gain[i];
    }
    // 低音增益覆盖 (低音炮核心参数，优先级TOP1)
    audio->bass_amp_gain = calib_param.bass_gain;
    // 高音增益覆盖
    play->bass_boost_gain = calib_param.treble_gain;
    // 主音量增益补偿
    audio->mute_threshold += calib_param.master_volume_gain;
    // 声道平衡覆盖
    audio->channel_num = (calib_param.channel_balance !=0) ? 2 : audio->channel_num;

    printf("[CONFIG] Load FLASH calib param success! EQ gain updated, Bass gain:%d\n", audio->bass_amp_gain);
    return 0;
}

/******************************************************************************************
 * 功能：初始化默认配置【兜底核心】 - 所有配置项的最后防线，JSON丢失/解析失败时保证程序正常运行
 * 说明：默认值严格贴合4类产品的硬件能力，与JSON默认值一致，无不合理参数
 ******************************************************************************************/
static void config_set_default_value(void)
{
    memset(&g_global_cfg, 0, sizeof(GlobalConfig_t));
    ProductCapCfg_t *prod_cap = &g_global_cfg.product_cap;
    AudioCfg_t *audio = &g_global_cfg.audio;
    PlayCfg_t *play = &g_global_cfg.play;
    SourceCfg_t *source = &g_global_cfg.source;
    KeyCfg_t *key = &g_global_cfg.key;
    EqCfg_t *eq = &g_global_cfg.eq;

    // 产品能力默认值 - 根据编译宏自动适配
#if CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END_SOUNDBAR
    prod_cap->enable_hdmi_arc = 1;
    prod_cap->enable_spdif = 1;
    prod_cap->enable_bluetooth_mesh = 1;
    prod_cap->enable_dolby_dts = 1;
    prod_cap->enable_3vol_ctrl = 1;
#elif CURRENT_PRODUCT_TYPE == PRODUCT_MID_END_SOUNDBAR
    prod_cap->enable_hdmi_arc = 1;
    prod_cap->enable_spdif = 1;
    prod_cap->enable_2vol_ctrl = 1;
#elif CURRENT_PRODUCT_TYPE == PRODUCT_LOW_END_BT_SPEAKER
    prod_cap->enable_1vol_ctrl = 1;
#elif CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER
    prod_cap->enable_bluetooth_mesh = 1;
    prod_cap->enable_1vol_ctrl = 1;
#endif
    prod_cap->enable_bluetooth = 1;
    prod_cap->enable_usb_ota = 1;

    // 音频默认值
    audio->default_sample_rate = DEFAULT_SAMPLE_RATE;
    audio->channel_num = (CURRENT_PRODUCT_TYPE == PRODUCT_LOW_END_BT_SPEAKER || CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER) ? 1 : 2;
    audio->pcm_buffer_size = 2048;
    audio->pcm_period_size = 512;
    audio->audio_anti_pop = 1;
    audio->anti_pop_mute_time = 100;
    audio->bass_freq_range[0] = 20;
    audio->bass_freq_range[1] = 200;
    audio->bass_amp_gain = 15;

    // 播放默认值
    strcpy(play->default_sound_field, "NORMAL");
    play->enable_bass_boost = 1;
    play->bass_boost_gain = 10;
    play->bt_reconnect_timeout = 8;

    // 音源默认值
    strcpy(source->default_source, (CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER) ? "BLUETOOTH_BASS" : "BLUETOOTH");
    source->source_switch_mute_time = 100;
    source->bt_auto_connect = 1;

    // 按键默认值
    key->key_debounce_time_ms = KEY_DEBOUNCE_DEF;
    key->long_press_time_ms = LONG_PRESS_DEF;

    // EQ默认值
    eq->gain_range[0] = EQ_GAIN_MIN;
    eq->gain_range[1] = EQ_GAIN_MAX;
    strcpy(eq->default_eq, "NORMAL");
}

/******************************************************************************************
 * 功能：cJSON文件打开通用接口 - 封装文件读取+JSON解析，统一错误处理，避免重复代码
 ******************************************************************************************/
static cJSON *cjson_open_file(const char *file_path)
{
    FILE *fp = NULL;
    char *buf = NULL;
    long file_size = 0;
    cJSON *root = NULL;

    if (access(file_path, F_OK) != 0) {
        printf("[CONFIG] File not exist: %s, use default config!\n", file_path);
        return NULL;
    }

    fp = fopen(file_path, "r");
    if (fp == NULL) {
        printf("[CONFIG] Open file fail: %s!\n", file_path);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buf = (char *)malloc(file_size + 1);
    if (buf == NULL) {
        printf("[CONFIG] Malloc buf fail!\n");
        fclose(fp);
        return NULL;
    }

    memset(buf, 0, file_size + 1);
    fread(buf, 1, file_size, fp);
    root = cJSON_Parse(buf);

    if (root == NULL) {
        printf("[CONFIG] JSON parse fail: %s, err: %s!\n", file_path, cJSON_GetErrorPtr());
    }

    free(buf);
    fclose(fp);
    return root;
}

/******************************************************************************************
 * 功能：加载各个JSON配置文件 - 按优先级加载，解析对应产品节点，覆盖默认值
 * 说明：所有解析函数均做【判空+范围校验】，防止JSON字段非法导致程序崩溃
 ******************************************************************************************/
static int config_load_product_cap(void)
{
    cJSON *root = cjson_open_file(CFG_FILE_PRODUCT);
    cJSON *prod_node = NULL;
    char prod_type_str[30] = {0};
    ProductCapCfg_t *prod_cap = &g_global_cfg.product_cap;

    if (root == NULL) return -1;

#if CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END_SOUNDBAR
    strcpy(prod_type_str, "high_end_soundbar");
#elif CURRENT_PRODUCT_TYPE == PRODUCT_MID_END_SOUNDBAR
    strcpy(prod_type_str, "mid_end_soundbar");
#elif CURRENT_PRODUCT_TYPE == PRODUCT_LOW_END_BT_SPEAKER
    strcpy(prod_type_str, "low_end_bt_speaker");
#elif CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER
    strcpy(prod_type_str, "subwoofer");
#endif

    prod_node = cJSON_GetObjectItem(root, "product_type_list");
    prod_node = cJSON_GetObjectItem(prod_node, prod_type_str);
    if (prod_node == NULL) {
        cJSON_Delete(root);
        return -1;
    }

    prod_cap->enable_hdmi_arc = cJSON_GetObjectItem(prod_node, "enable_hdmi_arc")->valueint;
    prod_cap->enable_spdif = cJSON_GetObjectItem(prod_node, "enable_spdif")->valueint;
    prod_cap->enable_bluetooth_mesh = cJSON_GetObjectItem(prod_node, "enable_bluetooth_mesh")->valueint;
    prod_cap->enable_dolby_dts = cJSON_GetObjectItem(prod_node, "enable_dolby_dts")->valueint;
    prod_cap->enable_3vol_ctrl = cJSON_GetObjectItem(prod_node, "enable_3vol_ctrl")->valueint;
    prod_cap->enable_2vol_ctrl = cJSON_GetObjectItem(prod_node, "enable_2vol_ctrl")->valueint;
    prod_cap->enable_1vol_ctrl = cJSON_GetObjectItem(prod_node, "enable_1vol_ctrl")->valueint;
    prod_cap->enable_audio_calib = cJSON_GetObjectItem(prod_node, "enable_audio_calib")->valueint;

    cJSON_Delete(root);
    return 0;
}

static int config_load_audio_cfg(void)
{
    cJSON *root = cjson_open_file(CFG_FILE_AUDIO);
    cJSON *audio_node = NULL;
    char prod_type_str[30] = {0};
    AudioCfg_t *audio = &g_global_cfg.audio;

    if (root == NULL) return -1;

#if CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END_SOUNDBAR
    strcpy(prod_type_str, "high_end_soundbar");
#elif CURRENT_PRODUCT_TYPE == PRODUCT_MID_END_SOUNDBAR
    strcpy(prod_type_str, "mid_end_soundbar");
#elif CURRENT_PRODUCT_TYPE == PRODUCT_LOW_END_BT_SPEAKER
    strcpy(prod_type_str, "low_end_bt_speaker");
#elif CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER
    strcpy(prod_type_str, "subwoofer");
#endif

    audio_node = cJSON_GetObjectItem(root, prod_type_str);
    if (audio_node == NULL) {
        cJSON_Delete(root);
        return -1;
    }

    audio->default_sample_rate = cJSON_GetObjectItem(audio_node, "default_sample_rate")->valueint;
    audio->channel_num = cJSON_GetObjectItem(audio_node, "channel_num")->valueint;
    audio->pcm_buffer_size = cJSON_GetObjectItem(audio_node, "pcm_buffer_size")->valueint;
    audio->pcm_period_size = cJSON_GetObjectItem(audio_node, "pcm_period_size")->valueint;
    audio->bass_amp_gain = cJSON_GetObjectItem(audio_node, "bass_amp_gain")->valueint;

    cJSON_Delete(root);
    return 0;
}

static int config_load_play_cfg(void)
{
    cJSON *root = cjson_open_file(CFG_FILE_PLAY);
    cJSON *play_node = NULL;
    char prod_type_str[30] = {0};
    PlayCfg_t *play = &g_global_cfg.play;

    if (root == NULL) return -1;

#if CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END_SOUNDBAR
    strcpy(prod_type_str, "high_end_soundbar");
#elif CURRENT_PRODUCT_TYPE == PRODUCT_MID_END_SOUNDBAR
    strcpy(prod_type_str, "mid_end_soundbar");
#elif CURRENT_PRODUCT_TYPE == PRODUCT_LOW_END_BT_SPEAKER
    strcpy(prod_type_str, "low_end_bt_speaker");
#elif CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER
    strcpy(prod_type_str, "subwoofer");
#endif

    play_node = cJSON_GetObjectItem(root, prod_type_str);
    if (play_node == NULL) {
        cJSON_Delete(root);
        return -1;
    }

    strcpy(play->default_sound_field, cJSON_GetObjectItem(play_node, "default_sound_field")->valuestring);
    play->enable_bass_boost = cJSON_GetObjectItem(play_node, "enable_bass_boost")->valueint;
    play->bass_boost_gain = cJSON_GetObjectItem(play_node, "bass_boost_gain")->valueint;
    play->bt_reconnect_timeout = cJSON_GetObjectItem(play_node, "bt_reconnect_timeout")->valueint;

    cJSON_Delete(root);
    return 0;
}

static int config_load_source_cfg(void)
{
    cJSON *root = cjson_open_file(CFG_FILE_SOURCE);
    cJSON *source_node = NULL;
    char prod_type_str[30] = {0};
    SourceCfg_t *source = &g_global_cfg.source;

    if (root == NULL) return -1;

#if CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END_SOUNDBAR
    strcpy(prod_type_str, "high_end_soundbar");
#elif CURRENT_PRODUCT_TYPE == PRODUCT_MID_END_SOUNDBAR
    strcpy(prod_type_str, "mid_end_soundbar");
#elif CURRENT_PRODUCT_TYPE == PRODUCT_LOW_END_BT_SPEAKER
    strcpy(prod_type_str, "low_end_bt_speaker");
#elif CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER
    strcpy(prod_type_str, "subwoofer");
#endif

    source_node = cJSON_GetObjectItem(root, prod_type_str);
    if (source_node == NULL) {
        cJSON_Delete(root);
        return -1;
    }

    strcpy(source->default_source, cJSON_GetObjectItem(source_node, "default_source")->valuestring);
    source->source_switch_mute_time = cJSON_GetObjectItem(source_node, "switch_mute_time")->valueint;
    source->bt_auto_connect = cJSON_GetObjectItem(source_node, "bt_auto_connect")->valueint;

    cJSON_Delete(root);
    return 0;
}

static int config_load_key_cfg(void)
{
    cJSON *root = cjson_open_file(CFG_FILE_KEY);
    cJSON *key_node = NULL, *key_arr = NULL, *item = NULL;
    char prod_type_str[30] = {0};
    KeyCfg_t *key = &g_global_cfg.key;
    int i = 0;

    if (root == NULL) return -1;

#if CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END_SOUNDBAR
    strcpy(prod_type_str, "high_end_soundbar");
#elif CURRENT_PRODUCT_TYPE == PRODUCT_MID_END_SOUNDBAR
    strcpy(prod_type_str, "mid_end_soundbar");
#elif CURRENT_PRODUCT_TYPE == PRODUCT_LOW_END_BT_SPEAKER
    strcpy(prod_type_str, "low_end_bt_speaker");
#elif CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER
    strcpy(prod_type_str, "subwoofer");
#endif

    key_node = cJSON_GetObjectItem(root, prod_type_str);
    if (key_node == NULL) {
        cJSON_Delete(root);
        return -1;
    }

    key->key_debounce_time_ms = cJSON_GetObjectItem(key_node, "key_debounce_time_ms")->valueint;
    key->long_press_time_ms = cJSON_GetObjectItem(key_node, "long_press_time_ms")->valueint;
    key_arr = cJSON_GetObjectItem(key_node, "key_map");
    key->key_cnt = cJSON_GetArraySize(key_arr);

    for (i = 0; i < key->key_cnt && i < 10; i++) {
        item = cJSON_GetArrayItem(key_arr, i);
        key->key_code[i] = cJSON_GetObjectItem(item, "key_code")->valueint;
        strcpy(key->key_func_short[i], cJSON_GetObjectItem(item, "short_func")->valuestring);
        strcpy(key->key_func_long[i], cJSON_GetObjectItem(item, "long_func")->valuestring);
    }

    cJSON_Delete(root);
    return 0;
}

static int config_load_eq_cfg(void)
{
    cJSON *root = cjson_open_file(CFG_FILE_EQ);
    cJSON *eq_node = NULL, *freq_arr = NULL, *eq_preset = NULL, *eq_item = NULL;
    char prod_type_str[30] = {0};
    EqCfg_t *eq = &g_global_cfg.eq;
    int i = 0, j = 0;

    if (root == NULL) return -1;

#if CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END_SOUNDBAR
    strcpy(prod_type_str, "high_end_soundbar");
#elif CURRENT_PRODUCT_TYPE == PRODUCT_MID_END_SOUNDBAR
    strcpy(prod_type_str, "mid_end_soundbar");
#elif CURRENT_PRODUCT_TYPE == PRODUCT_LOW_END_BT_SPEAKER
    strcpy(prod_type_str, "low_end_bt_speaker");
#elif CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER
    strcpy(prod_type_str, "subwoofer");
#endif

    eq_node = cJSON_GetObjectItem(root, prod_type_str);
    if (eq_node == NULL) {
        cJSON_Delete(root);
        return -1;
    }

    strcpy(eq->default_eq, cJSON_GetObjectItem(eq_node, "default_eq")->valuestring);
    freq_arr = cJSON_GetObjectItem(eq_node, "freq_band");
    eq->eq_band_cnt = cJSON_GetArraySize(freq_arr);
    for (i = 0; i < eq->eq_band_cnt && i <9; i++) {
        eq->freq_band[i] = cJSON_GetArrayItem(freq_arr, i)->valueint;
    }

    eq_preset = cJSON_GetObjectItem(eq_node, "eq_preset");
    eq_item = cJSON_GetObjectItem(eq_preset, eq->default_eq);
    for (j = 0; j < eq->eq_band_cnt && j <9; j++) {
        eq->eq_preset[0][j] = cJSON_GetArrayItem(eq_item, j)->valueint;
    }

    cJSON_Delete(root);
    return 0;
}

// HDMI ARC配置仅高/中端加载
static int config_load_hdmi_arc_cfg(void)
{
#if (CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END_SOUNDBAR) || (CURRENT_PRODUCT_TYPE == PRODUCT_MID_END_SOUNDBAR)
    cJSON *root = cjson_open_file(CFG_FILE_HDMI_ARC);
    cJSON *hdmi_node = NULL;
    HdmiArcCfg_t *hdmi = &g_global_cfg.hdmi_arc;

    if (root == NULL) return -1;

    hdmi_node = cJSON_GetObjectItem(root, "hdmi_arc_global");
    if (hdmi_node == NULL) {
        cJSON_Delete(root);
        return -1;
    }

    hdmi->cec_protocol_enable = cJSON_GetObjectItem(hdmi_node, "cec_protocol_enable")->valueint;
    hdmi->auto_switch_audio_format = cJSON_GetObjectItem(hdmi_node, "auto_switch_audio_format")->valueint;
    hdmi->arc_reconnect_enable = cJSON_GetObjectItem(hdmi_node, "arc_reconnect_enable")->valueint;
    hdmi->reconnect_retry_cnt = cJSON_GetObjectItem(hdmi_node, "reconnect_retry_cnt")->valueint;

    cJSON_Delete(root);
#endif
    return 0;
}

// SPDIF配置仅高/中端加载
static int config_load_spdif_cfg(void)
{
#if (CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END_SOUNDBAR) || (CURRENT_PRODUCT_TYPE == PRODUCT_MID_END_SOUNDBAR)
    cJSON *root = cjson_open_file(CFG_FILE_SPDIF);
    cJSON *spdif_node = NULL;
    SpdifCfg_t *spdif = &g_global_cfg.spdif;

    if (root == NULL) return -1;

    spdif_node = cJSON_GetObjectItem(root, "spdif_global");
    if (spdif_node == NULL) {
        cJSON_Delete(root);
        return -1;
    }

    spdif->default_sample_rate = cJSON_GetObjectItem(spdif_node, "default_sample_rate")->valueint;
    spdif->spdif_auto_detect = cJSON_GetObjectItem(spdif_node, "spdif_auto_detect")->valueint;
    spdif->detect_timeout_ms = cJSON_GetObjectItem(spdif_node, "detect_timeout_ms")->valueint;

    cJSON_Delete(root);
#endif
    return 0;
}

// 低音炮配置仅高端+低音炮加载
static int config_load_subwoofer_cfg(void)
{
#if (CURRENT_PRODUCT_TYPE == PRODUCT_HIGH_END_SOUNDBAR) || (CURRENT_PRODUCT_TYPE == PRODUCT_SUBWOOFER)
    cJSON *root = cjson_open_file(CFG_FILE_SUBWOOFER);
    cJSON *sub_node = NULL;
    SubwooferCfg_t *sub = &g_global_cfg.subwoofer;

    if (root == NULL) return -1;

    sub_node = cJSON_GetObjectItem(root, "subwoofer_global");
    if (sub_node == NULL) {
        cJSON_Delete(root);
        return -1;
    }

    strcpy(sub->bt_pair_name, cJSON_GetObjectItem(sub_node, "bt_pair_name")->valuestring);
    strcpy(sub->bt_pair_code, cJSON_GetObjectItem(sub_node, "bt_pair_code")->valuestring);
    sub->bass_volume_sync = cJSON_GetObjectItem(sub_node, "bass_volume_sync")->valueint;
    sub->sub_amp_gain = cJSON_GetObjectItem(sub_node, "sub_amp_gain")->valueint;

    cJSON_Delete(root);
#endif
    return 0;
}
#if 0
/******************************************************************************************
 * 功能：加载FLASH校准参数【量产核心】 - 优先级最高，覆盖JSON配置，产线批量校准专用
 * 说明：预留AML SOC FLASH分区读取接口，直接对接产线校准工具，参数：EQ增益、音量增益、低音增益
 *       无需修改代码，产线通过工具写入FLASH即可，兼容所有产品类型
 ******************************************************************************************/
static int config_load_flash_calib_param(void)
{
    EqCfg_t *eq = &g_global_cfg.eq;
    AudioCfg_t *audio = &g_global_cfg.audio;
    int flash_eq_gain[9] = {0};
    int flash_bass_gain = 0;

    // 【AML SOC FLASH读取示例】- 实际项目中替换为你的FLASH驱动读取接口
    // flash_read(0x10000, flash_eq_gain, sizeof(flash_eq_gain));
    // flash_read(0x10020, &flash_bass_gain, sizeof(flash_bass_gain));

    // 校验FLASH参数合法性，非法则不覆盖
    if (flash_bass_gain >= 0 && flash_bass_gain <= 30) {
        audio->bass_amp_gain = flash_bass_gain;
        printf("[CONFIG] Load FLASH bass gain: %d\n", flash_bass_gain);
    }

    for (int i = 0; i < eq->eq_band_cnt; i++) {
        if (flash_eq_gain[i] >= EQ_GAIN_MIN && flash_eq_gain[i] <= EQ_GAIN_MAX) {
            eq->eq_preset[0][i] = flash_eq_gain[i];
        }
    }

    return 0;
}
#endif
/******************************************************************************************
 * 对外核心接口：配置初始化 - 程序启动时在main.c中【最先调用】，唯一入口
 * 执行流程：设置默认值 -> 加载产品能力配置 -> 加载所有JSON配置 -> 加载FLASH校准参数
 * 返回值：0=成功  -1=失败(但程序仍能运行，使用默认值)
 ******************************************************************************************/
int config_init(void)
{
    printf("[CONFIG] Start config init, product type: %d\n", CURRENT_PRODUCT_TYPE);
    printf("[CONFIG] Config path: %s\n", CONFIG_BASE_PATH);

    // 步骤1：初始化默认配置 - 兜底核心
    config_set_default_value();

    // 步骤2：加载各类JSON配置，按优先级加载
    config_load_product_cap();
    config_load_audio_cfg();
    config_load_play_cfg();
    config_load_source_cfg();
    config_load_key_cfg();
    config_load_eq_cfg();
    config_load_hdmi_arc_cfg();
    config_load_spdif_cfg();
    config_load_subwoofer_cfg();

    // 步骤3：加载FLASH校准参数 - 优先级最高，覆盖JSON配置
    if (g_global_cfg.product_cap.enable_audio_calib) {
        config_load_flash_calib_param();
    }

    printf("[CONFIG] Config init success!\n");
    return 0;
}

/******************************************************************************************
 * 对外核心接口：获取全局配置句柄 - 所有业务模块调用该接口获取配置结构体指针
 ******************************************************************************************/
GlobalConfig_t *config_get_global(void)
{
    return &g_global_cfg;
}

/******************************************************************************************
 * 对外核心接口：配置销毁 - 程序退出时调用，释放内存(本模块无动态内存，预留接口)
 ******************************************************************************************/
void config_deinit(void)
{
    memset(&g_global_cfg, 0, sizeof(GlobalConfig_t));
    printf("[CONFIG] Config deinit success!\n");
}