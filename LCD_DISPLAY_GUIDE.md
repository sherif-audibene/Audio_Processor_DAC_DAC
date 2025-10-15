# LCD Waveform Display Guide

## Overview

This project now includes a real-time audio waveform display on a **ST7567S 128x64 I2C LCD module**. The display shows the audio signal being processed by the system in oscilloscope-style visualization.

## Hardware Connection

### LCD Module: 12864 IIC 4P LCD Module (128x64, ST7567S Controller)

**Pin Connections:**
- **SDA** → GPIO 6
- **SCL** → GPIO 7
- **VCC** → 3.3V or 5V (depending on module)
- **GND** → GND

> **Note:** The default I2C address is `0x3F`. If your display uses a different address (common alternatives: `0x3C`, `0x3D`), update the `LCD_I2C_ADDRESS` definition in `lcd_display.h`.

## Features

### Current Implementation

1. **Real-time Waveform Display**
   - Displays audio signal in oscilloscope style
   - Updates at 30 FPS for smooth visualization
   - Automatic scaling and decimation for optimal display

2. **Configurable Display Options**
   - **Resolution**: Number of samples displayed (adjustable)
   - **Amplitude Scale**: Zoom in/out on signal amplitude (10-200%)
   - **Grid Lines**: Optional reference grid
   - **Center Line**: Optional center reference line
   - **Time Scale**: Time axis scaling (1-10x)

3. **Display Features**
   - 128x64 pixel resolution
   - Clear, high-contrast monochrome display
   - Adjustable contrast (0-63)
   - Low latency visualization

## Configuration

### Basic Configuration (in `main.c`)

The LCD display is configured in the `app_main()` function:

```c
lcd_task_config_t lcd_config = {
    .sda_pin = 6,                    // I2C SDA pin
    .scl_pin = 7,                    // I2C SCL pin
    .lcd_contrast = 35,              // Display contrast (0-63)
    .waveform = {
        .samples_per_screen = 128,   // Number of samples to display
        .time_scale = 1,             // Time scale multiplier
        .amplitude_scale = 100,      // Amplitude scale percentage
        .show_grid = true,           // Show grid lines
        .show_center_line = true,    // Show center reference line
        .mode = WAVEFORM_MODE_OSCILLOSCOPE
    }
};
```

### Adjustable Parameters

#### 1. **samples_per_screen** (Resolution)
Controls how many audio samples are displayed across the screen width.

- **Lower values (32-64)**: Zoomed-in view, shows more detail, slower time base
- **Default (128)**: Balanced view, one sample per pixel
- **Higher values (256+)**: Zoomed-out view, shows longer time periods

```c
.samples_per_screen = 64,    // Zoom in (more detail)
.samples_per_screen = 128,   // Default (balanced)
.samples_per_screen = 256,   // Zoom out (longer time)
```

#### 2. **amplitude_scale** (Vertical Zoom)
Scales the signal amplitude for better visibility.

- **Range**: 10-200%
- **50%**: Half amplitude (for loud signals)
- **100%**: Normal amplitude
- **150-200%**: Amplified view (for quiet signals)

```c
.amplitude_scale = 50,     // Reduce amplitude for loud signals
.amplitude_scale = 100,    // Normal
.amplitude_scale = 150,    // Amplify for quiet signals
```

#### 3. **time_scale**
Future feature for additional time axis control (currently 1-10).

#### 4. **show_grid**
Enables/disables reference grid on display.

```c
.show_grid = true,    // Show grid
.show_grid = false,   // Hide grid
```

#### 5. **show_center_line**
Enables/disables horizontal center reference line (0V reference).

```c
.show_center_line = true,    // Show center line
.show_center_line = false,   // Hide center line
```

#### 6. **lcd_contrast**
Adjusts LCD contrast for optimal visibility.

- **Range**: 0-63
- **Low values (20-30)**: Lower contrast
- **Default (35)**: Balanced contrast
- **High values (40-50)**: Higher contrast

```c
.lcd_contrast = 25,    // Lower contrast
.lcd_contrast = 35,    // Default
.lcd_contrast = 45,    // Higher contrast
```

### Runtime Configuration Changes

You can change the waveform configuration at runtime using:

```c
waveform_config_t new_config = {
    .samples_per_screen = 64,
    .amplitude_scale = 150,
    .show_grid = false,
    .show_center_line = true,
    .mode = WAVEFORM_MODE_OSCILLOSCOPE
};

lcd_task_set_waveform_config(&new_config);
```

Change contrast at runtime:

```c
lcd_task_set_contrast(40);
```

## Usage Examples

### Example 1: High Detail View (Zoomed In)

For viewing fine details in the waveform:

```c
.waveform = {
    .samples_per_screen = 64,      // Show fewer samples (zoomed in)
    .amplitude_scale = 100,         // Normal amplitude
    .show_grid = true,
    .show_center_line = true,
    .mode = WAVEFORM_MODE_OSCILLOSCOPE
}
```

### Example 2: Overview Mode (Zoomed Out)

For viewing longer time periods:

```c
.waveform = {
    .samples_per_screen = 256,     // Show more samples (zoomed out)
    .amplitude_scale = 100,
    .show_grid = false,             // Clean view without grid
    .show_center_line = true,
    .mode = WAVEFORM_MODE_OSCILLOSCOPE
}
```

### Example 3: Quiet Signal Analysis

For analyzing low-amplitude signals:

```c
.waveform = {
    .samples_per_screen = 128,
    .amplitude_scale = 180,         // Amplify signal
    .show_grid = true,
    .show_center_line = true,
    .mode = WAVEFORM_MODE_OSCILLOSCOPE
}
```

### Example 4: Loud Signal Monitoring

For monitoring high-amplitude signals without clipping:

```c
.waveform = {
    .samples_per_screen = 128,
    .amplitude_scale = 50,          // Reduce amplitude
    .show_grid = true,
    .show_center_line = true,
    .mode = WAVEFORM_MODE_OSCILLOSCOPE
}
```

## API Reference

### Initialization Functions

#### `lcd_task_start()`
Initialize and start the LCD display task.

```c
esp_err_t lcd_task_start(const lcd_task_config_t *config);
```

**Parameters:**
- `config`: Pointer to LCD task configuration structure

**Returns:** `ESP_OK` on success, error code on failure

#### `lcd_task_stop()`
Stop the LCD display task and cleanup resources.

```c
void lcd_task_stop(void);
```

### Configuration Functions

#### `lcd_task_set_waveform_config()`
Update waveform display configuration at runtime.

```c
esp_err_t lcd_task_set_waveform_config(const waveform_config_t *config);
```

#### `lcd_task_get_waveform_config()`
Retrieve current waveform configuration.

```c
esp_err_t lcd_task_get_waveform_config(waveform_config_t *config);
```

#### `lcd_task_set_contrast()`
Adjust LCD contrast.

```c
esp_err_t lcd_task_set_contrast(uint8_t contrast);
```

### Status Functions

#### `lcd_task_is_running()`
Check if LCD task is active.

```c
bool lcd_task_is_running(void);
```

## Technical Details

### Display Update Rate
- **Refresh Rate**: 30 Hz (configurable via `LCD_UPDATE_RATE_HZ`)
- **Audio Queue**: 4 buffers deep for smooth operation
- **Processing**: Non-blocking, won't affect audio quality

### Memory Usage
- **Framebuffer**: 1024 bytes (128 x 64 / 8)
- **Waveform Buffer**: 512 bytes (256 samples x 2 bytes)
- **Task Stack**: 4096 bytes
- **Audio Queue**: ~2KB (4 buffers)

### Signal Processing
- **Channel**: Left channel only (mono display)
- **Sample Format**: Converts 24-bit I2S samples to 16-bit for display
- **Decimation**: Automatic decimation for high sample rates
- **Scaling**: Real-time amplitude and time scaling

## Troubleshooting

### Display Not Working

1. **Check I2C Address**
   - Default is `0x3F`, but some modules use `0x3C` or `0x3D`
   - Update `LCD_I2C_ADDRESS` in `lcd_display.h`

2. **Check Pin Connections**
   - Verify GPIO 6 (SDA) and GPIO 7 (SCL) connections
   - Ensure proper power supply to LCD module

3. **Check Pull-up Resistors**
   - I2C requires pull-up resistors on SDA and SCL
   - Most modules have onboard pull-ups
   - If not, add 4.7kΩ resistors to VCC

4. **Verify Voltage**
   - Some modules require 5V, others 3.3V
   - Check your module specifications

### Display Too Dark/Bright

Adjust contrast in configuration:

```c
.lcd_contrast = 35,  // Try values between 20-50
```

### Waveform Not Visible

1. **Increase Amplitude Scale**:
   ```c
   .amplitude_scale = 150,  // Amplify signal
   ```

2. **Check Sample Rate**:
   - Display works best with audio signals
   - Very high frequency signals may appear as noise

3. **Enable Grid/Center Line**:
   ```c
   .show_grid = true,
   .show_center_line = true,
   ```

### Waveform Updates Slowly

- This is normal for very low frequency signals
- Adjust `samples_per_screen` to see more time period:
  ```c
  .samples_per_screen = 64,  // Faster updates
  ```

## Future Enhancements

Planned features for future versions:

1. **Spectrum Analyzer Mode**: FFT-based frequency domain display
2. **VU Meter Mode**: Peak level indicators
3. **Dual Channel Display**: Both left and right channels
4. **Triggering**: Edge triggering for stable display
5. **Persistence**: Trace persistence effect
6. **Measurements**: Frequency, amplitude, and RMS display

## Code Structure

### Files Added

1. **lcd_display.h/c**: Low-level ST7567S driver
   - I2C communication
   - Display initialization
   - Graphics primitives (pixel, line, rectangle)
   - Framebuffer management

2. **lcd_task.h/c**: High-level display task
   - Audio data reception
   - Waveform processing
   - Display rendering
   - Configuration management

### Integration

The LCD module integrates seamlessly with the existing audio pipeline:

```
ADC → Audio Buffer → Effects Processing → DAC
                           ↓
                      LCD Display
```

Audio data is copied to the LCD task via a FreeRTOS queue, ensuring no impact on real-time audio processing.

## Performance Notes

- **CPU Impact**: Minimal (~2-3% on ESP32-S3)
- **Memory Impact**: ~8KB total
- **Audio Latency**: No additional latency added
- **Display Latency**: ~33ms (at 30 FPS)

The display system runs independently and doesn't block or slow down audio processing.

---

**Last Updated**: October 15, 2025
**Version**: 1.0
**Author**: Audio Processor DAC Project Team

