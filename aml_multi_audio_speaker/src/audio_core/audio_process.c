/**
 * @file audio_process.c
 * @brief 音频处理进程实现
 * @details 负责音频解码、混音和输出等核心音频处理功能
 * @author AML Audio Team
 * @date 2026-01-22
 */

#include "audio_core_priv.h"
#include "logger.h"
#include "event.h"
#include "process.h"
#include "common_def.h"
#include "hal.h"  // 硬件抽象层

/******************************************************************************************
 * 音频处理进程内部数据结构
 ******************************************************************************************/

/**
 * @brief 音频进程消息类型
 */
typedef enum {
    AUDIO_MSG_INIT = 0,           // 初始化音频核心
    AUDIO_MSG_DEINIT,              // 反初始化音频核心
    AUDIO_MSG_PLAY_PCM,            // 播放PCM数据
    AUDIO_MSG_PAUSE,               // 暂停播放
    AUDIO_MSG_RESUME,              // 恢复播放
    AUDIO_MSG_STOP,                // 停止播放
    AUDIO_MSG_SET_VOLUME,          // 设置音量
    AUDIO_MSG_GET_STATUS,          // 获取播放状态
    AUDIO_MSG_MAX
} AudioMsgType_t;

/**
 * @brief 音频进程消息
 */
typedef struct {
    AudioMsgType_t type;           // 消息类型
    union {
        struct {
            AudioCoreConfig_t config;  // 音频核心配置
        } init;
        struct {
            uint8_t *pcm_data;     // PCM数据
            int data_len;          // 数据长度
        } play_pcm;
        struct {
            int volume;            // 音量值
        } set_volume;
    } data;
} AudioMsg_t;

/******************************************************************************************
 * 音频处理进程全局变量
 ******************************************************************************************/

static bool g_audio_process_running = false;
static pid_t g_audio_process_pid = -1;
static int g_audio_process_pipe[2] = {-1, -1};

/******************************************************************************************
 * 音频处理进程内部函数
 ******************************************************************************************/

/**
 * @brief 音频处理进程主函数
 * @param arg 进程参数
 * @return 进程返回值
 */
static void *audio_process_main(void *arg) {
    LOG_INFO("Audio process started, pid: %d", getpid());
    
    // 初始化音频核心
    AudioCoreConfig_t default_config = {
        .sample_rate = 48000,
        .channel_num = 2,
        .pcm_buffer_size = 1024 * 64,
        .hw_decode_en = true,
        .dolby_dts_en = false
    };
    
    if (audio_core_init(&default_config) != 0) {
        LOG_ERROR("Audio core init failed");
        return NULL;
    }
    
    // 主循环
    while (g_audio_process_running) {
        // 接收消息
        AudioMsg_t msg;
        int ret = read(g_audio_process_pipe[0], &msg, sizeof(AudioMsg_t));
        if (ret <= 0) {
            continue;
        }
        
        // 处理消息
        switch (msg.type) {
            case AUDIO_MSG_INIT:
                audio_core_deinit();
                audio_core_init(&msg.data.init.config);
                break;
            case AUDIO_MSG_DEINIT:
                audio_core_deinit();
                break;
            case AUDIO_MSG_PLAY_PCM:
                if (msg.data.play_pcm.pcm_data && msg.data.play_pcm.data_len > 0) {
                    audio_core_play_pcm(msg.data.play_pcm.pcm_data, msg.data.play_pcm.data_len);
                }
                break;
            case AUDIO_MSG_PAUSE:
                audio_core_pause();
                break;
            case AUDIO_MSG_RESUME:
                audio_core_resume();
                break;
            case AUDIO_MSG_STOP:
                audio_core_stop();
                break;
            case AUDIO_MSG_SET_VOLUME:
                audio_core_set_volume(msg.data.set_volume.volume);
                break;
            case AUDIO_MSG_GET_STATUS:
                // 发送播放状态
                break;
            default:
                LOG_ERROR("Invalid audio message type: %d", msg.type);
                break;
        }
    }
    
    // 反初始化音频核心
    audio_core_deinit();
    
    LOG_INFO("Audio process exited");
    return NULL;
}

/**
 * @brief 创建音频处理进程
 * @return 进程ID，失败返回-1
 */
static pid_t create_audio_process(void) {
    // 创建管道
    if (pipe(g_audio_process_pipe) == -1) {
        LOG_ERROR("Failed to create pipe: %d", errno);
        return -1;
    }
    
    // 创建进程
    pid_t pid = fork();
    if (pid == -1) {
        LOG_ERROR("Failed to fork audio process: %d", errno);
        close(g_audio_process_pipe[0]);
        close(g_audio_process_pipe[1]);
        g_audio_process_pipe[0] = -1;
        g_audio_process_pipe[1] = -1;
        return -1;
    } else if (pid == 0) {
        // 子进程
        close(g_audio_process_pipe[1]); // 关闭写端
        g_audio_process_running = true;
        audio_process_main(NULL);
        close(g_audio_process_pipe[0]);
        exit(0);
    } else {
        // 父进程
        close(g_audio_process_pipe[0]); // 关闭读端
        g_audio_process_pid = pid;
        LOG_INFO("Created audio process: %d", pid);
    }
    
    return pid;
}

/**
 * @brief 终止音频处理进程
 * @return SUCCESS/FAILURE
 */
static int terminate_audio_process(void) {
    if (g_audio_process_pid == -1) {
        return SUCCESS;
    }
    
    // 发送终止消息
    AudioMsg_t msg;
    msg.type = AUDIO_MSG_DEINIT;
    write(g_audio_process_pipe[1], &msg, sizeof(AudioMsg_t));
    
    // 终止进程
    if (kill(g_audio_process_pid, SIGTERM) == -1) {
        LOG_ERROR("Failed to terminate audio process: %d", errno);
        return FAILURE;
    }
    
    // 等待进程退出
    waitpid(g_audio_process_pid, NULL, 0);
    
    // 关闭管道
    close(g_audio_process_pipe[1]);
    g_audio_process_pipe[0] = -1;
    g_audio_process_pipe[1] = -1;
    g_audio_process_pid = -1;
    
    LOG_INFO("Terminated audio process: %d", g_audio_process_pid);
    return SUCCESS;
}

/******************************************************************************************
 * 音频处理进程对外接口
 ******************************************************************************************/

/**
 * @brief 初始化音频处理进程
 * @param config 音频核心配置
 * @return SUCCESS/FAILURE
 */
int audio_process_init(AudioCoreConfig_t *config) {
    // 创建音频处理进程
    pid_t pid = create_audio_process();
    if (pid == -1) {
        LOG_ERROR("Failed to create audio process");
        return FAILURE;
    }
    
    // 发送初始化消息
    if (config) {
        AudioMsg_t msg;
        msg.type = AUDIO_MSG_INIT;
        msg.data.init.config = *config;
        write(g_audio_process_pipe[1], &msg, sizeof(AudioMsg_t));
    }
    
    LOG_INFO("Audio process initialized");
    return SUCCESS;
}

/**
 * @brief 反初始化音频处理进程
 * @return SUCCESS/FAILURE
 */
int audio_process_deinit(void) {
    int ret = terminate_audio_process();
    if (ret == SUCCESS) {
        LOG_INFO("Audio process deinitialized");
    }
    return ret;
}

/**
 * @brief 播放PCM数据
 * @param pcm_buf PCM数据缓冲区
 * @param buf_len 数据长度
 * @return SUCCESS/FAILURE
 */
int audio_process_play_pcm(uint8_t *pcm_buf, int buf_len) {
    if (g_audio_process_pid == -1) {
        LOG_ERROR("Audio process not initialized");
        return FAILURE;
    }
    
    AudioMsg_t msg;
    msg.type = AUDIO_MSG_PLAY_PCM;
    msg.data.play_pcm.pcm_data = pcm_buf;
    msg.data.play_pcm.data_len = buf_len;
    
    if (write(g_audio_process_pipe[1], &msg, sizeof(AudioMsg_t)) != sizeof(AudioMsg_t)) {
        LOG_ERROR("Failed to send play PCM message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 暂停音频播放
 * @return SUCCESS/FAILURE
 */
int audio_process_pause(void) {
    if (g_audio_process_pid == -1) {
        LOG_ERROR("Audio process not initialized");
        return FAILURE;
    }
    
    AudioMsg_t msg;
    msg.type = AUDIO_MSG_PAUSE;
    
    if (write(g_audio_process_pipe[1], &msg, sizeof(AudioMsg_t)) != sizeof(AudioMsg_t)) {
        LOG_ERROR("Failed to send pause message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 恢复音频播放
 * @return SUCCESS/FAILURE
 */
int audio_process_resume(void) {
    if (g_audio_process_pid == -1) {
        LOG_ERROR("Audio process not initialized");
        return FAILURE;
    }
    
    AudioMsg_t msg;
    msg.type = AUDIO_MSG_RESUME;
    
    if (write(g_audio_process_pipe[1], &msg, sizeof(AudioMsg_t)) != sizeof(AudioMsg_t)) {
        LOG_ERROR("Failed to send resume message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 停止音频播放
 * @return SUCCESS/FAILURE
 */
int audio_process_stop(void) {
    if (g_audio_process_pid == -1) {
        LOG_ERROR("Audio process not initialized");
        return FAILURE;
    }
    
    AudioMsg_t msg;
    msg.type = AUDIO_MSG_STOP;
    
    if (write(g_audio_process_pipe[1], &msg, sizeof(AudioMsg_t)) != sizeof(AudioMsg_t)) {
        LOG_ERROR("Failed to send stop message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 设置音量
 * @param vol 音量值
 * @return SUCCESS/FAILURE
 */
int audio_process_set_volume(int vol) {
    if (g_audio_process_pid == -1) {
        LOG_ERROR("Audio process not initialized");
        return FAILURE;
    }
    
    AudioMsg_t msg;
    msg.type = AUDIO_MSG_SET_VOLUME;
    msg.data.set_volume.volume = vol;
    
    if (write(g_audio_process_pipe[1], &msg, sizeof(AudioMsg_t)) != sizeof(AudioMsg_t)) {
        LOG_ERROR("Failed to send set volume message");
        return FAILURE;
    }
    
    return SUCCESS;
}

/**
 * @brief 获取音频处理进程ID
 * @return 进程ID，失败返回-1
 */
pid_t audio_process_get_pid(void) {
    return g_audio_process_pid;
}

/**
 * @brief 检查音频处理进程是否运行
 * @return true表示运行，false表示未运行
 */
bool audio_process_is_running(void) {
    if (g_audio_process_pid == -1) {
        return false;
    }
    
    int status;
    pid_t result = waitpid(g_audio_process_pid, &status, WNOHANG);
    return result == 0;
}
