# ESP32-S3 Audio Processor DAC

An ESP32-S3 based audio processing system that reads audio from an external ADC (with master clock from AliExpress) and outputs processed audio to a DAC. This project implements real-time audio passthrough with configurable processing features.

## Features

- **Real-time Audio Passthrough**: Continuous audio streaming from ADC to DAC
- **Configurable Audio Processing**:
  - Volume scaling (0.0 to 1.0)
  - Channel swapping (left/right)
  - Debug logging with sample rate monitoring
- **High-Quality Audio**: 192 kHz sample rate, 32-bit ADC input, 16-bit DAC output
- **Dual I2S Configuration**: Separate I2S peripherals for ADC (slave) and DAC (master)
- **FreeRTOS Task Management**: Dedicated audio processing task with proper resource management

## Hardware Configuration

### Pin Mapping

**DAC (I2S Master - Output)**:
- BCK (Bit Clock): GPIO 36
- LRCK (Word Select): GPIO 37  
- DATA: GPIO 38

**ADC (I2S Slave - Input)**:
- BCK (Bit Clock): GPIO 36 (shared with DAC)
- LRCK (Word Select): GPIO 37 (shared with DAC)
- DATA: GPIO 35

### Audio Specifications

- **Sample Rate**: 192,000 Hz
- **ADC Input**: 32-bit samples, stereo
- **DAC Output**: 16-bit samples, stereo
- **Buffer Size**: 256 samples per channel
- **DMA Buffers**: 8 buffers for low-latency processing

## Project Structure

```
main/
├── main.c              # Application entry point and configuration
├── audio_processor.c   # Audio processing logic and task management
├── audio_processor.h   # Audio processing API definitions
├── i2s_config.c        # I2S peripheral configuration
├── i2s_config.h        # I2S pin definitions and constants
└── CMakeLists.txt      # Component build configuration
```

## Key Components

### Audio Processor (`audio_processor.c`)
- Manages the main audio passthrough task
- Implements real-time audio processing (volume scaling, channel swapping)
- Handles I2S peripheral initialization and cleanup
- Provides debug logging for audio samples

### I2S Configuration (`i2s_config.c`)
- Configures dual I2S peripherals (ADC slave, DAC master)
- Manages pin assignments and audio parameters
- Handles peripheral lifecycle (init, start, stop, cleanup)

### Main Application (`main.c`)
- Initializes audio processing with default configuration
- Starts the audio passthrough system
- Handles error conditions and cleanup

## Configuration

The audio processing can be configured through the `audio_config_t` structure:

```c
audio_config_t audio_config = {
    .volume_scale = 0.5f,           // Volume scaling (0.0 to 1.0)
    .enable_debug = true,           // Enable debug logging
    .enable_channel_swap = true     // Swap left/right channels
};
```

## Building and Flashing

### Prerequisites
- ESP-IDF v4.4 or later
- ESP32-S3 development board
- External ADC with master clock (AliExpress module)
- External DAC

### Build Commands
```bash
# Configure the project
idf.py menuconfig

# Build the project
idf.py build

# Flash to device
idf.py flash

# Monitor serial output
idf.py monitor
```

## Usage

1. **Hardware Setup**: Connect the external ADC and DAC modules according to the pin mapping
2. **Power On**: The system will automatically initialize and start audio processing
3. **Audio Flow**: Audio from the ADC is processed and output to the DAC in real-time
4. **Debug Monitoring**: Use serial monitor to view audio processing status and sample data

## Audio Processing Features

### Volume Scaling
- Configurable volume control from 0.0 (mute) to 1.0 (full volume)
- Applied to both left and right channels

### Channel Swapping
- Optional left/right channel swapping
- Useful for correcting wiring or audio source orientation

### Debug Logging
- Periodic logging of audio samples (every 100 samples by default)
- Shows sample values and confirms 192 kHz operation
- Can be disabled for production use

## Performance Characteristics

- **Latency**: Low-latency processing with 8 DMA buffers
- **CPU Usage**: Optimized FreeRTOS task with priority 5
- **Memory**: Dynamic buffer allocation with proper cleanup
- **Reliability**: Comprehensive error handling and recovery

## Troubleshooting

### Common Issues

1. **No Audio Output**: Check DAC connections and power supply
2. **Distorted Audio**: Verify ADC input levels and clock synchronization
3. **High CPU Usage**: Reduce debug logging frequency
4. **Audio Dropouts**: Check buffer sizes and DMA configuration

### Debug Information

The system provides detailed logging for troubleshooting:
- I2S peripheral initialization status
- Audio sample processing statistics
- Error conditions with specific error codes
- Buffer underrun/overrun detection

## License

This project is open source. Please refer to the ESP-IDF license for framework components.

## Contributing

Contributions are welcome! Please ensure:
- Code follows ESP-IDF coding standards
- Proper error handling is implemented
- Documentation is updated for new features
- Hardware compatibility is maintained
