#ifndef AUDIO_SDK_H
#define AUDIO_SDK_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

/**
 * Audio device types
 */
typedef enum {
    AUDIO_DEVICE_TYPE_PLAYBACK,
    AUDIO_DEVICE_TYPE_CAPTURE
} AudioDeviceType;

/**
 * Audio sample formats
 */
typedef enum {
    AUDIO_FORMAT_S16_LE,  // 16-bit signed little-endian
    AUDIO_FORMAT_S32_LE,  // 32-bit signed little-endian
    AUDIO_FORMAT_FLOAT32  // 32-bit float
} AudioFormat;

/**
 * Audio device configuration
 */
typedef struct {
    const char* device_name;  // ALSA device name (NULL for default)
    AudioDeviceType type;     // Capture or playback
    AudioFormat format;       // Sample format
    uint32_t sample_rate;     // Sample rate in Hz (e.g., 44100, 48000)
    uint8_t channels;         // Number of channels (1 for mono, 2 for stereo)
    uint16_t period_size;     // Number of frames per period
    uint8_t periods;          // Number of periods
} AudioDeviceConfig;

/**
 * Audio device handle
 */
typedef struct AudioDevice AudioDevice;

/**
 * Initialize the audio SDK
 * 
 * @return true if successful, false otherwise
 */
bool audio_sdk_init(void);

/**
 * Cleanup the audio SDK
 */
void audio_sdk_cleanup(void);

/**
 * Open an audio device with the specified configuration
 * 
 * @param config Device configuration
 * @return AudioDevice handle, NULL on failure
 */
AudioDevice* audio_device_open(const AudioDeviceConfig* config);

/**
 * Close an audio device
 * 
 * @param device AudioDevice handle
 */
void audio_device_close(AudioDevice* device);

/**
 * Read audio data from a capture device
 * 
 * @param device AudioDevice handle
 * @param buffer Buffer to store audio data
 * @param frames Number of frames to read
 * @return Number of frames actually read, negative on error
 */
int32_t audio_device_read(AudioDevice* device, void* buffer, uint32_t frames);

/**
 * Write audio data to a playback device
 * 
 * @param device AudioDevice handle
 * @param buffer Buffer containing audio data
 * @param frames Number of frames to write
 * @return Number of frames actually written, negative on error
 */
int32_t audio_device_write(AudioDevice* device, const void* buffer, uint32_t frames);

/**
 * Get the actual sample rate used by the device
 * 
 * @param device AudioDevice handle
 * @return Sample rate in Hz
 */
uint32_t audio_device_get_sample_rate(const AudioDevice* device);

/**
 * Get the actual buffer size (period size * periods) used by the device
 * 
 * @param device AudioDevice handle
 * @return Buffer size in frames
 */
uint32_t audio_device_get_buffer_size(const AudioDevice* device);

#endif // AUDIO_SDK_H