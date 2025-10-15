# LCD Waveform Display - Quick Reference

## Quick Configuration Examples

Copy and paste these configurations into your `main.c` file to quickly adjust the waveform display.

### 📊 Standard Oscilloscope View (Recommended)
```c
lcd_task_config_t lcd_config = {
    .sda_pin = 6,
    .scl_pin = 7,
    .lcd_contrast = 35,
    .waveform = {
        .samples_per_screen = 128,
        .time_scale = 1,
        .amplitude_scale = 100,
        .show_grid = true,
        .show_center_line = true,
        .mode = WAVEFORM_MODE_OSCILLOSCOPE
    }
};
```

### 🔍 High Detail View (Zoomed In)
```c
lcd_task_config_t lcd_config = {
    .sda_pin = 6,
    .scl_pin = 7,
    .lcd_contrast = 35,
    .waveform = {
        .samples_per_screen = 64,     // Fewer samples = more detail
        .time_scale = 1,
        .amplitude_scale = 100,
        .show_grid = true,
        .show_center_line = true,
        .mode = WAVEFORM_MODE_OSCILLOSCOPE
    }
};
```

### 🌊 Wide View (Zoomed Out)
```c
lcd_task_config_t lcd_config = {
    .sda_pin = 6,
    .scl_pin = 7,
    .lcd_contrast = 35,
    .waveform = {
        .samples_per_screen = 256,    // More samples = longer time
        .time_scale = 1,
        .amplitude_scale = 100,
        .show_grid = false,            // Clean view
        .show_center_line = true,
        .mode = WAVEFORM_MODE_OSCILLOSCOPE
    }
};
```

### 📢 Loud Signal (Compressed View)
```c
lcd_task_config_t lcd_config = {
    .sda_pin = 6,
    .scl_pin = 7,
    .lcd_contrast = 35,
    .waveform = {
        .samples_per_screen = 128,
        .time_scale = 1,
        .amplitude_scale = 50,        // Reduce amplitude
        .show_grid = true,
        .show_center_line = true,
        .mode = WAVEFORM_MODE_OSCILLOSCOPE
    }
};
```

### 🔉 Quiet Signal (Amplified View)
```c
lcd_task_config_t lcd_config = {
    .sda_pin = 6,
    .scl_pin = 7,
    .lcd_contrast = 35,
    .waveform = {
        .samples_per_screen = 128,
        .time_scale = 1,
        .amplitude_scale = 180,       // Amplify signal
        .show_grid = true,
        .show_center_line = true,
        .mode = WAVEFORM_MODE_OSCILLOSCOPE
    }
};
```

### 🎯 Clean Minimal View
```c
lcd_task_config_t lcd_config = {
    .sda_pin = 6,
    .scl_pin = 7,
    .lcd_contrast = 35,
    .waveform = {
        .samples_per_screen = 128,
        .time_scale = 1,
        .amplitude_scale = 100,
        .show_grid = false,           // No grid
        .show_center_line = false,    // No center line
        .mode = WAVEFORM_MODE_OSCILLOSCOPE
    }
};
```

## Parameter Quick Guide

### samples_per_screen
- **32-64**: Very detailed, zoomed in (good for analyzing waveform shape)
- **128**: Default, balanced view (1 sample per pixel)
- **192-256**: Wide view, longer time period (good for rhythm/patterns)

### amplitude_scale
- **25-50%**: Very loud signals (prevent clipping on display)
- **75-100%**: Normal signals
- **125-150%**: Quiet signals (amplify for visibility)
- **175-200%**: Very quiet signals (maximum amplification)

### lcd_contrast
- **20-30**: Lower contrast (dimmer, easier on eyes)
- **35**: Default (balanced)
- **40-50**: Higher contrast (brighter, better visibility in bright light)
- **55-63**: Maximum contrast (may cause blooming)

### show_grid
- **true**: Shows reference grid (helpful for measurements)
- **false**: Clean view (better for visual aesthetics)

### show_center_line
- **true**: Shows zero reference line (helpful for DC offset detection)
- **false**: Clean view

## Runtime Configuration Changes

You can change settings while the program is running:

### Change Waveform Settings
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

### Change Contrast
```c
lcd_task_set_contrast(45);  // 0-63
```

### Check if LCD is Running
```c
if (lcd_task_is_running()) {
    ESP_LOGI(TAG, "LCD is active");
}
```

## Troubleshooting Quick Fixes

| Problem | Quick Fix |
|---------|-----------|
| Display too dark | Increase `lcd_contrast` to 45-50 |
| Display too bright | Decrease `lcd_contrast` to 25-30 |
| Waveform too small | Increase `amplitude_scale` to 150-200% |
| Waveform clipped | Decrease `amplitude_scale` to 50-75% |
| Too slow/static | Decrease `samples_per_screen` to 64 |
| Too fast/jittery | Increase `samples_per_screen` to 192 |
| Display not working | Check pins: SDA=GPIO6, SCL=GPIO7 |
| Can't see waveform | Enable grid: `show_grid = true` |

## I2C Address Configuration

If your display doesn't work with the default address, try these common alternatives:

Edit `lcd_display.h`:
```c
// Common ST7567S I2C addresses:
#define LCD_I2C_ADDRESS 0x3F  // Most common (default)
// #define LCD_I2C_ADDRESS 0x3C  // Alternative 1
// #define LCD_I2C_ADDRESS 0x3D  // Alternative 2
```

## Pin Remapping

To use different GPIO pins, change in `main.c`:
```c
lcd_task_config_t lcd_config = {
    .sda_pin = 6,    // Change to your SDA pin
    .scl_pin = 7,    // Change to your SCL pin
    // ... rest of config
};
```

---

For detailed information, see [LCD_DISPLAY_GUIDE.md](LCD_DISPLAY_GUIDE.md)

