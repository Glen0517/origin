#include <audio_sdk.h>
#include <math.h>
#include <stdio.h>

// Generate a sine wave audio signal
void generate_sine_wave(int16_t* buffer, uint32_t frames, uint32_t sample_rate, float frequency) {
    static float phase = 0.0f;
    const float amplitude = 32767 * 0.7f;  // 70% of maximum amplitude

    for (uint32_t i = 0; i < frames; i++) {
        buffer[i] = (int16_t)(amplitude * sinf(phase));
        phase += 2 * M_PI * frequency / sample_rate;
        if (phase >= 2 * M_PI) {
            phase -= 2 * M_PI;
        }
    }
}

int main() {
    // Initialize the audio SDK
    if (!audio_sdk_init()) {
        fprintf(stderr, "Failed to initialize audio SDK\n");
        return 1;
    }

    // Configure audio device
    AudioDeviceConfig config = {
        .device_name = NULL,  // Use default device
        .type = AUDIO_DEVICE_TYPE_PLAYBACK,
        .format = AUDIO_FORMAT_S16_LE,
        .sample_rate = 44100,
        .channels = 1,
        .period_size = 1024,
        .periods = 4
    };

    // Open audio device
    AudioDevice* device = audio_device_open(&config);
    if (!device) {
        fprintf(stderr, "Failed to open audio device\n");
        audio_sdk_cleanup();
        return 1;
    }

    printf("Playing sine wave... (Press Ctrl+C to stop)\n");

    // Allocate buffer for audio data
    int16_t* buffer = malloc(config.period_size * sizeof(int16_t));
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        audio_device_close(device);
        audio_sdk_cleanup();
        return 1;
    }

    // Play sine wave for 5 seconds
    const uint32_t total_frames = config.sample_rate * 5;
    uint32_t frames_written = 0;

    while (frames_written < total_frames) {
        const uint32_t frames_to_write = (total_frames - frames_written) > config.period_size ?
                                        config.period_size : (total_frames - frames_written);

        // Generate audio data
        generate_sine_wave(buffer, frames_to_write, config.sample_rate, 440.0f);  // 440Hz = A4 note

        // Write audio data to device
        int32_t result = audio_device_write(device, buffer, frames_to_write);
        if (result < 0) {
            fprintf(stderr, "Failed to write audio data\n");
            break;
        }

        frames_written += result;
    }

    // Cleanup
    free(buffer);
    audio_device_close(device);
    audio_sdk_cleanup();

    printf("Playback completed successfully\n");
    return 0;
}