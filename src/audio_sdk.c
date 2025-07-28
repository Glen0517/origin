#include "audio_sdk.h"
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <string.h>

// Private device structure
struct AudioDevice {
    snd_pcm_t* handle;
    AudioDeviceConfig config;
    snd_pcm_format_t alsa_format;
};

bool audio_sdk_init(void) {
    // For ALSA, no explicit initialization is needed
    return true;
}

void audio_sdk_cleanup(void) {
    // For ALSA, no explicit cleanup is needed
}

static snd_pcm_format_t audio_format_to_alsa(AudioFormat format) {
    switch (format) {
        case AUDIO_FORMAT_S16_LE:
            return SND_PCM_FORMAT_S16_LE;
        case AUDIO_FORMAT_S32_LE:
            return SND_PCM_FORMAT_S32_LE;
        case AUDIO_FORMAT_FLOAT32:
            return SND_PCM_FORMAT_FLOAT_LE;
        default:
            return SND_PCM_FORMAT_UNKNOWN;
    }
}

AudioDevice* audio_device_open(const AudioDeviceConfig* config) {
    if (!config) return NULL;

    AudioDevice* device = malloc(sizeof(AudioDevice));
    if (!device) return NULL;

    memset(device, 0, sizeof(AudioDevice));
    device->config = *config;
    device->alsa_format = audio_format_to_alsa(config->format);

    if (device->alsa_format == SND_PCM_FORMAT_UNKNOWN) {
        free(device);
        return NULL;
    }

    int err;
    const char* dev_name = config->device_name ? config->device_name : "default";
    snd_pcm_stream_t stream = (config->type == AUDIO_DEVICE_TYPE_PLAYBACK) ?
                             SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE;

    // Open PCM device
    err = snd_pcm_open(&device->handle, dev_name, stream, 0);
    if (err < 0) {
        fprintf(stderr, "Unable to open PCM device: %s\n", snd_strerror(err));
        free(device);
        return NULL;
    }

    // Configure PCM parameters
    err = snd_pcm_set_params(device->handle,
                            device->alsa_format,
                            SND_PCM_ACCESS_RW_INTERLEAVED,
                            config->channels,
                            config->sample_rate,
                            1,  // Allow software resampling
                            config->period_size * 1000 / config->sample_rate * config->periods);  // Latency in us

    if (err < 0) {
        fprintf(stderr, "Unable to set PCM parameters: %s\n", snd_strerror(err));
        snd_pcm_close(device->handle);
        free(device);
        return NULL;
    }

    return device;
}

void audio_device_close(AudioDevice* device) {
    if (!device) return;

    snd_pcm_drain(device->handle);
    snd_pcm_close(device->handle);
    free(device);
}

int32_t audio_device_read(AudioDevice* device, void* buffer, uint32_t frames) {
    if (!device || !buffer || device->config.type != AUDIO_DEVICE_TYPE_CAPTURE) {
        return -1;
    }

    int err = snd_pcm_readi(device->handle, buffer, frames);
    if (err < 0) {
        fprintf(stderr, "Read error: %s\n", snd_strerror(err));
        // Try to recover from error
        if (snd_pcm_recover(device->handle, err, 0) < 0) {
            return -1;
        }
        return 0;
    }

    return err;
}

int32_t audio_device_write(AudioDevice* device, const void* buffer, uint32_t frames) {
    if (!device || !buffer || device->config.type != AUDIO_DEVICE_TYPE_PLAYBACK) {
        return -1;
    }

    int err = snd_pcm_writei(device->handle, buffer, frames);
    if (err < 0) {
        fprintf(stderr, "Write error: %s\n", snd_strerror(err));
        // Try to recover from error
        if (snd_pcm_recover(device->handle, err, 0) < 0) {
            return -1;
        }
        return 0;
    }

    return err;
}

uint32_t audio_device_get_sample_rate(const AudioDevice* device) {
    if (!device) return 0;

    snd_pcm_hw_params_t* params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_current(device->handle, params);

    uint32_t rate;
    snd_pcm_hw_params_get_rate(params, &rate, NULL);
    return rate;
}

uint32_t audio_device_get_buffer_size(const AudioDevice* device) {
    if (!device) return 0;

    snd_pcm_hw_params_t* params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_current(device->handle, params);

    snd_pcm_uframes_t buffer_size;
    snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
    return (uint32_t)buffer_size;
}