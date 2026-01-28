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

/**
 * @brief 音频处理任务
 */
typedef struct {
    uint8_t *pcm_data;
    int data_len;
    int task_id;
    bool processed;
} AudioTask_t;

/**
 * @brief 音频处理线程池
 */
typedef struct {
    pthread_t *threads;
    int thread_count;
    AudioTask_t *tasks;
    int task_capacity;
    int task_count;
    int task_head;
    int task_tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool running;
} AudioThreadPool_t;

/**
 * @brief 音频数据缓存
 */
typedef struct {
    uint8_t *buffer;
    int buffer_size;
    int data_len;
    int read_pos;
    int write_pos;
    bool full;
    pthread_mutex_t mutex;
} AudioBuffer_t;

/******************************************************************************************
 * 音频处理进程全局变量
 ******************************************************************************************/

static bool g_audio_process_running = false;
static pid_t g_audio_process_pid = -1;
static int g_audio_process_pipe[2] = {-1, -1};

// 音频处理线程池
static AudioThreadPool_t *g_audio_thread_pool = NULL;

// 音频数据缓存
static AudioBuffer_t *g_audio_buffer = NULL;

// 缓存配置
#define AUDIO_BUFFER_SIZE (1024 * 1024)  // 1MB缓存
#define THREAD_POOL_SIZE 4               // 4线程
#define TASK_QUEUE_SIZE 64               // 64任务队列

/******************************************************************************************
 * 音频处理进程内部函数
 ******************************************************************************************/

/**
 * @brief 初始化音频缓存
 * @param buffer_size 缓存大小
 * @return 音频缓存指针，失败返回NULL
 */
static AudioBuffer_t *audio_buffer_init(int buffer_size) {
    AudioBuffer_t *buffer = (AudioBuffer_t *)malloc(sizeof(AudioBuffer_t));
    if (!buffer) {
        LOG_ERROR("Failed to allocate audio buffer");
        return NULL;
    }
    
    buffer->buffer = (uint8_t *)malloc(buffer_size);
    if (!buffer->buffer) {
        LOG_ERROR("Failed to allocate audio buffer memory");
        free(buffer);
        return NULL;
    }
    
    buffer->buffer_size = buffer_size;
    buffer->data_len = 0;
    buffer->read_pos = 0;
    buffer->write_pos = 0;
    buffer->full = false;
    pthread_mutex_init(&buffer->mutex, NULL);
    
    LOG_INFO("Audio buffer initialized with size: %d bytes", buffer_size);
    return buffer;
}

/**
 * @brief 反初始化音频缓存
 * @param buffer 音频缓存指针
 */
static void audio_buffer_deinit(AudioBuffer_t *buffer) {
    if (buffer) {
        if (buffer->buffer) {
            free(buffer->buffer);
        }
        pthread_mutex_destroy(&buffer->mutex);
        free(buffer);
        LOG_INFO("Audio buffer deinitialized");
    }
}

/**
 * @brief 向音频缓存写入数据
 * @param buffer 音频缓存指针
 * @param data 数据指针
 * @param len 数据长度
 * @return 写入的字节数
 */
static int audio_buffer_write(AudioBuffer_t *buffer, const uint8_t *data, int len) {
    if (!buffer || !data || len <= 0) {
        return 0;
    }
    
    pthread_mutex_lock(&buffer->mutex);
    
    if (buffer->full) {
        pthread_mutex_unlock(&buffer->mutex);
        return 0;
    }
    
    int write_len = len;
    if (write_len > buffer->buffer_size - buffer->data_len) {
        write_len = buffer->buffer_size - buffer->data_len;
    }
    
    // 分两部分写入
    int part1 = buffer->buffer_size - buffer->write_pos;
    if (write_len <= part1) {
        memcpy(buffer->buffer + buffer->write_pos, data, write_len);
        buffer->write_pos += write_len;
        if (buffer->write_pos >= buffer->buffer_size) {
            buffer->write_pos = 0;
        }
    } else {
        memcpy(buffer->buffer + buffer->write_pos, data, part1);
        memcpy(buffer->buffer, data + part1, write_len - part1);
        buffer->write_pos = write_len - part1;
    }
    
    buffer->data_len += write_len;
    buffer->full = (buffer->data_len == buffer->buffer_size);
    
    pthread_mutex_unlock(&buffer->mutex);
    return write_len;
}

/**
 * @brief 从音频缓存读取数据
 * @param buffer 音频缓存指针
 * @param data 数据指针
 * @param len 数据长度
 * @return 读取的字节数
 */
static int audio_buffer_read(AudioBuffer_t *buffer, uint8_t *data, int len) {
    if (!buffer || !data || len <= 0) {
        return 0;
    }
    
    pthread_mutex_lock(&buffer->mutex);
    
    if (buffer->data_len == 0) {
        pthread_mutex_unlock(&buffer->mutex);
        return 0;
    }
    
    int read_len = len;
    if (read_len > buffer->data_len) {
        read_len = buffer->data_len;
    }
    
    // 分两部分读取
    int part1 = buffer->buffer_size - buffer->read_pos;
    if (read_len <= part1) {
        memcpy(data, buffer->buffer + buffer->read_pos, read_len);
        buffer->read_pos += read_len;
        if (buffer->read_pos >= buffer->buffer_size) {
            buffer->read_pos = 0;
        }
    } else {
        memcpy(data, buffer->buffer + buffer->read_pos, part1);
        memcpy(data + part1, buffer->buffer, read_len - part1);
        buffer->read_pos = read_len - part1;
    }
    
    buffer->data_len -= read_len;
    buffer->full = false;
    
    pthread_mutex_unlock(&buffer->mutex);
    return read_len;
}

/**
 * @brief 初始化音频处理线程池
 * @param thread_count 线程数量
 * @param task_capacity 任务队列容量
 * @return 线程池指针，失败返回NULL
 */
static AudioThreadPool_t *audio_thread_pool_init(int thread_count, int task_capacity) {
    AudioThreadPool_t *pool = (AudioThreadPool_t *)malloc(sizeof(AudioThreadPool_t));
    if (!pool) {
        LOG_ERROR("Failed to allocate thread pool");
        return NULL;
    }
    
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    if (!pool->threads) {
        LOG_ERROR("Failed to allocate thread pool threads");
        free(pool);
        return NULL;
    }
    
    pool->tasks = (AudioTask_t *)malloc(sizeof(AudioTask_t) * task_capacity);
    if (!pool->tasks) {
        LOG_ERROR("Failed to allocate thread pool tasks");
        free(pool->threads);
        free(pool);
        return NULL;
    }
    
    pool->thread_count = thread_count;
    pool->task_capacity = task_capacity;
    pool->task_count = 0;
    pool->task_head = 0;
    pool->task_tail = 0;
    pool->running = true;
    
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond, NULL);
    
    LOG_INFO("Audio thread pool initialized with %d threads and task capacity %d", thread_count, task_capacity);
    return pool;
}

/**
 * @brief 反初始化音频处理线程池
 * @param pool 线程池指针
 */
static void audio_thread_pool_deinit(AudioThreadPool_t *pool) {
    if (pool) {
        pool->running = false;
        
        // 唤醒所有线程
        pthread_cond_broadcast(&pool->cond);
        
        // 等待所有线程退出
        for (int i = 0; i < pool->thread_count; i++) {
            pthread_join(pool->threads[i], NULL);
        }
        
        if (pool->tasks) {
            free(pool->tasks);
        }
        if (pool->threads) {
            free(pool->threads);
        }
        
        pthread_mutex_destroy(&pool->mutex);
        pthread_cond_destroy(&pool->cond);
        free(pool);
        
        LOG_INFO("Audio thread pool deinitialized");
    }
}

/**
 * @brief 音频处理线程函数
 * @param arg 线程参数
 * @return 线程返回值
 */
static void *audio_thread_func(void *arg) {
    AudioThreadPool_t *pool = (AudioThreadPool_t *)arg;
    if (!pool) {
        return NULL;
    }
    
    LOG_INFO("Audio thread started: %d", (int)pthread_self());
    
    while (pool->running) {
        AudioTask_t task;
        bool got_task = false;
        
        pthread_mutex_lock(&pool->mutex);
        
        // 等待任务
        while (pool->running && pool->task_count == 0) {
            pthread_cond_wait(&pool->cond, &pool->mutex);
        }
        
        if (pool->running && pool->task_count > 0) {
            // 获取任务
            task = pool->tasks[pool->task_head];
            pool->task_head = (pool->task_head + 1) % pool->task_capacity;
            pool->task_count--;
            got_task = true;
        }
        
        pthread_mutex_unlock(&pool->mutex);
        
        if (got_task) {
            // 处理音频数据
            if (task.pcm_data && task.data_len > 0) {
                // 这里可以添加音频处理逻辑，如重采样、混音等
                // 目前直接传递给音频核心
                audio_core_play_pcm(task.pcm_data, task.data_len);
            }
            task.processed = true;
        }
    }
    
    LOG_INFO("Audio thread exited: %d", (int)pthread_self());
    return NULL;
}

/**
 * @brief 向线程池提交任务
 * @param pool 线程池指针
 * @param pcm_data PCM数据
 * @param data_len 数据长度
 * @return 任务ID，失败返回-1
 */
static int audio_thread_pool_submit(AudioThreadPool_t *pool, uint8_t *pcm_data, int data_len) {
    if (!pool || !pcm_data || data_len <= 0) {
        return -1;
    }
    
    pthread_mutex_lock(&pool->mutex);
    
    if (pool->task_count >= pool->task_capacity) {
        pthread_mutex_unlock(&pool->mutex);
        LOG_WARN("Thread pool task queue full");
        return -1;
    }
    
    static int task_id_counter = 0;
    int task_id = task_id_counter++;
    
    // 添加任务
    pool->tasks[pool->task_tail].pcm_data = pcm_data;
    pool->tasks[pool->task_tail].data_len = data_len;
    pool->tasks[pool->task_tail].task_id = task_id;
    pool->tasks[pool->task_tail].processed = false;
    
    pool->task_tail = (pool->task_tail + 1) % pool->task_capacity;
    pool->task_count++;
    
    // 唤醒线程
    pthread_cond_signal(&pool->cond);
    
    pthread_mutex_unlock(&pool->mutex);
    
    return task_id;
}

/**
 * @brief 启动线程池中的线程
 * @param pool 线程池指针
 * @return 成功返回0，失败返回-1
 */
static int audio_thread_pool_start(AudioThreadPool_t *pool) {
    if (!pool) {
        return -1;
    }
    
    for (int i = 0; i < pool->thread_count; i++) {
        if (pthread_create(&pool->threads[i], NULL, audio_thread_func, pool) != 0) {
            LOG_ERROR("Failed to create audio thread: %d", i);
            return -1;
        }
    }
    
    LOG_INFO("Started %d audio threads", pool->thread_count);
    return 0;
}

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
    
    // 初始化音频缓存
    g_audio_buffer = audio_buffer_init(AUDIO_BUFFER_SIZE);
    if (!g_audio_buffer) {
        LOG_ERROR("Failed to initialize audio buffer");
        audio_core_deinit();
        return NULL;
    }
    
    // 初始化音频处理线程池
    g_audio_thread_pool = audio_thread_pool_init(THREAD_POOL_SIZE, TASK_QUEUE_SIZE);
    if (!g_audio_thread_pool) {
        LOG_ERROR("Failed to initialize audio thread pool");
        audio_buffer_deinit(g_audio_buffer);
        audio_core_deinit();
        return NULL;
    }
    
    // 启动线程池
    if (audio_thread_pool_start(g_audio_thread_pool) != 0) {
        LOG_ERROR("Failed to start audio thread pool");
        audio_thread_pool_deinit(g_audio_thread_pool);
        audio_buffer_deinit(g_audio_buffer);
        audio_core_deinit();
        return NULL;
    }
    
    // 主循环
    while (g_audio_process_running) {
        // 接收消息
        AudioMsg_t msg;
        int ret = read(g_audio_process_pipe[0], &msg, sizeof(AudioMsg_t));
        if (ret <= 0) {
            // 非阻塞读取，短暂休眠
            usleep(1000);
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
                    // 先写入缓存
                    int written = audio_buffer_write(g_audio_buffer, msg.data.play_pcm.pcm_data, msg.data.play_pcm.data_len);
                    if (written > 0) {
                        // 从缓存读取并提交给线程池处理
                        uint8_t *buffer = (uint8_t *)malloc(4096);
                        if (buffer) {
                            int read_len = audio_buffer_read(g_audio_buffer, buffer, 4096);
                            if (read_len > 0) {
                                audio_thread_pool_submit(g_audio_thread_pool, buffer, read_len);
                            } else {
                                free(buffer);
                            }
                        }
                    }
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
    
    // 反初始化音频处理线程池
    if (g_audio_thread_pool) {
        audio_thread_pool_deinit(g_audio_thread_pool);
        g_audio_thread_pool = NULL;
    }
    
    // 反初始化音频缓存
    if (g_audio_buffer) {
        audio_buffer_deinit(g_audio_buffer);
        g_audio_buffer = NULL;
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
    
#ifdef _WIN32
    // Windows平台处理
    // 这里需要根据Windows平台的API进行相应的实现
    // 暂时简化处理
    return true;
#else
    // Linux平台处理
    #include <sys/wait.h>
    int status;
    pid_t result = waitpid(g_audio_process_pid, &status, WNOHANG);
    return result == 0;
#endif
}
