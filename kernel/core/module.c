#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/rtmutex.h>
#include <linux/sched.h>
#include <linux/audio.h>
#include <sound/core.h>
#include "rt_patches.h"
#include "audio_buffer.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Audio Framework Developers");
MODULE_DESCRIPTION("Real-time Audio Framework Kernel Module");
MODULE_VERSION("1.0");

// Audio framework global state
static struct audio_framework {
    struct rt_mutex lock;
    bool initialized;
    struct audio_buffer *buffer;
    struct task_struct *rt_thread;
    atomic_t running;
} afw;

// Real-time audio processing thread
static int audio_processing_thread(void *data)
{
    struct sched_param param = {
        .sched_priority = MAX_RT_PRIO - 1
    };
    int ret;

    // Set real-time scheduling
    ret = sched_setscheduler(current, SCHED_FIFO, &param);
    if (ret < 0) {
        pr_err("Failed to set real-time scheduler: %d\n", ret);
        return ret;
    }

    // Lock memory to prevent swapping
    ret = memalloc_reserve(0);
    if (ret < 0) {
        pr_err("Failed to lock memory: %d\n", ret);
        return ret;
    }

    pr_info("Real-time audio processing thread started (PID: %d)\n", current->pid);

    // Main processing loop
    while (atomic_read(&afw.running)) {
        rt_mutex_lock(&afw.lock);

        // Process audio buffer
        if (afw.buffer && afw.buffer->data && afw.buffer->size > 0) {
            process_audio_buffer(afw.buffer);
            route_audio_buffer(afw.buffer);
        }

        rt_mutex_unlock(&afw.lock);
        schedule();
    }

    memalloc_free(0);
    pr_info("Real-time audio processing thread stopped\n");
    return 0;
}

// Initialize audio framework
static int __init audio_framework_init(void)
{
    int ret;

    pr_info("Audio Framework (Linux kernel %s + preempt-rt) initializing...\n", UTS_RELEASE);

    // Initialize mutex
    rt_mutex_init(&afw.lock);

    // Initialize audio buffer
    afw.buffer = audio_buffer_create(4096, 4); // 4096 frames, 4 channels
    if (!afw.buffer) {
        pr_err("Failed to create audio buffer\n");
        return -ENOMEM;
    }

    // Apply real-time patches
    ret = apply_rt_patches();
    if (ret < 0) {
        pr_err("Failed to apply real-time patches: %d\n", ret);
        audio_buffer_destroy(afw.buffer);
        return ret;
    }

    // Create real-time processing thread
    atomic_set(&afw.running, 1);
    afw.rt_thread = kthread_run(audio_processing_thread, NULL, "audio-rt-thread");
    if (IS_ERR(afw.rt_thread)) {
        ret = PTR_ERR(afw.rt_thread);
        pr_err("Failed to create real-time thread: %d\n", ret);
        atomic_set(&afw.running, 0);
        audio_buffer_destroy(afw.buffer);
        revert_rt_patches();
        return ret;
    }

    afw.initialized = true;
    pr_info("Audio Framework initialized successfully\n");
    return 0;
}

// Cleanup audio framework
static void __exit audio_framework_exit(void)
{
    if (!afw.initialized) return;

    pr_info("Audio Framework cleaning up...\n");

    // Stop processing thread
    atomic_set(&afw.running, 0);
    if (afw.rt_thread) {
        kthread_stop(afw.rt_thread);
    }

    // Cleanup resources
    audio_buffer_destroy(afw.buffer);
    revert_rt_patches();
    rt_mutex_destroy(&afw.lock);

    pr_info("Audio Framework cleaned up successfully\n");
}

module_init(audio_framework_init);
module_exit(audio_framework_exit);