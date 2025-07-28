# Audio Linux SDK

A lightweight audio SDK for Linux systems with ALSA integration, providing simple audio capture and playback capabilities.

## Features
- Audio playback and capture using ALSA
- Support for multiple audio formats (16-bit PCM, 32-bit PCM, float32)
- Device configuration (sample rate, channels, buffer sizes)
- Simple and intuitive API
- Example applications included
- Cross-platform Linux support

## Prerequisites
- ALSA development libraries: `libasound2-dev`
- CMake 3.10 or higher
- GCC or Clang compiler
- Make

## Installation

### From Source

1. Clone the repository:
```bash
 git clone https://github.com/yourusername/audio-linux-sdk.git
 cd audio-linux-sdk
```

2. Create build directory and configure:
```bash
 mkdir build && cd build
 cmake ..
```

3. Build and install:
```bash
 make -j4
 sudo make install
```

The SDK will be installed to:
- Libraries: `/usr/local/lib`
- Headers: `/usr/local/include/audio_sdk`
- Examples: `/usr/local/bin/examples`

## API Overview

### Initialization
```c
#include <audio_sdk.h>

// Initialize the SDK
bool audio_sdk_init(void);

// Cleanup the SDK
void audio_sdk_cleanup(void);
```

### Device Configuration
```c
// Audio device configuration structure
typedef struct {
    const char* device_name;  // ALSA device name (NULL for default)
    AudioDeviceType type;     // Capture or playback
    AudioFormat format;       // Sample format
    uint32_t sample_rate;     // Sample rate in Hz
    uint8_t channels;         // Number of channels
    uint16_t period_size;     // Frames per period
    uint8_t periods;          // Number of periods
} AudioDeviceConfig;
```

### Device Operations
```c
// Open an audio device
AudioDevice* audio_device_open(const AudioDeviceConfig* config);

// Close an audio device
void audio_device_close(AudioDevice* device);

// Read audio data (capture)
int32_t audio_device_read(AudioDevice* device, void* buffer, uint32_t frames);

// Write audio data (playback)
int32_t audio_device_write(AudioDevice* device, const void* buffer, uint32_t frames);
```

## Examples

### Simple Playback
The SDK includes a simple sine wave playback example:
```bash
/usr/local/bin/examples/simple_playback
```

## License
MIT License

## Contributing
1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Troubleshooting
- **No sound output**: Check if your default audio device is configured correctly
- **ALSA errors**: Ensure ALSA development libraries are installed
- **Permission issues**: Run with sudo or add your user to the 'audio' group

For more issues, please open an issue on GitHub.
