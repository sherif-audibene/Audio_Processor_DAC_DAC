# WiFi and Web Interface Guide

## Overview

The ESP32-S3 Audio Processor now includes WiFi connectivity and a web-based control interface. This allows you to configure all device parameters remotely through a web browser.

## Features

- **Automatic WiFi Connection**: Connects to "Sherif-Home-2.4" network on boot
- **Web Control Panel**: Access device settings via web browser
- **Real-time Parameter Updates**: Changes take effect immediately
- **Parameter Management**: Centralized configuration system

## WiFi Configuration

The device automatically connects to WiFi on boot with the following credentials:
- **SSID**: `Sherif-Home-2.4`
- **Password**: `password`

To change WiFi credentials, modify the `wifi_config` structure in `main.c`:

```c
wifi_config_t wifi_config = {
    .ssid = "Your-SSID",
    .password = "Your-Password",
    .timeout_ms = 10000
};
```

## Accessing the Web Interface

1. **Wait for WiFi Connection**: After boot, wait for the device to connect to WiFi (check serial monitor for IP address)

2. **Open Web Browser**: Navigate to the IP address shown in the serial monitor (e.g., `http://192.168.1.100`)

3. **Control Panel**: The web interface will load automatically

## Web Interface Features

### Audio Settings
- **Volume Scale**: Adjust audio volume (0.0 - 3.0)
- **Enable Debug**: Toggle debug logging
- **Channel Swap**: Swap left/right audio channels
- **Delay Effect**: Enable/disable delay effect
- **Delay Time**: Set delay time in milliseconds (0-1000ms)
- **Delay Mix**: Control wet/dry mix (0.0 - 1.0)
- **Delay Feedback**: Set feedback amount (0.0 - 1.0)

### LCD Display Settings
- **Contrast**: Adjust LCD contrast (0-63)
- **Samples Per Screen**: Set waveform resolution (32-256 samples)
- **Amplitude Scale**: Adjust vertical zoom (10-200%)
- **Show Grid**: Toggle grid lines
- **Show Center Line**: Toggle center reference line

### LED Control Settings
- **Low Threshold**: Threshold for green LEDs (0.0 - 1.0)
- **Medium Threshold**: Threshold for yellow LEDs (0.0 - 1.0)
- **High Threshold**: Threshold for red LEDs (0.0 - 1.0)
- **Smoothing Factor**: LED response smoothing (0.0 - 1.0)
- **Enable LEDs**: Toggle LED control

## API Endpoints

The web server provides REST API endpoints for programmatic control:

### GET /api/config
Get current device configuration
```json
{
  "success": true,
  "config": {
    "audio": { ... },
    "lcd": { ... },
    "led": { ... }
  }
}
```

### POST /api/config
Update device configuration
```json
{
  "audio": {
    "volume_scale": 1.5,
    "enable_debug": false,
    "enable_channel_swap": true,
    "enable_delay": true,
    "delay_time_ms": 50.0,
    "delay_mix": 0.9,
    "delay_feedback": 0.4
  },
  "lcd": {
    "lcd_contrast": 20,
    "waveform": {
      "samples_per_screen": 4096,
      "amplitude_scale": 100,
      "show_grid": true,
      "show_center_line": true
    }
  },
  "led": {
    "low_threshold": 0.15,
    "medium_threshold": 0.35,
    "high_threshold": 0.65,
    "smoothing_factor": 0.2,
    "enable_leds": true
  }
}
```

### POST /api/config/reset
Reset all parameters to defaults

## Usage

1. **Load Current Settings**: Click "Load Current Settings" to populate the form with current values

2. **Adjust Parameters**: Use sliders and checkboxes to adjust settings

3. **Save Settings**: Click "Save Settings" to apply changes immediately

4. **Reset to Defaults**: Click "Reset to Defaults" to restore factory settings

## Troubleshooting

### WiFi Not Connecting
- Check SSID and password are correct
- Verify WiFi network is 2.4GHz (ESP32-S3 doesn't support 5GHz)
- Check serial monitor for connection errors
- Ensure WiFi router is within range

### Web Interface Not Accessible
- Verify WiFi connection (check serial monitor for IP address)
- Ensure device and computer are on the same network
- Check firewall settings
- Try accessing via IP address directly

### Parameters Not Updating
- Check serial monitor for error messages
- Verify parameter ranges are valid
- Some parameters require device restart to take full effect

## Technical Details

### Components Added
- **wifi_manager.c/h**: WiFi connection management
- **web_server.c/h**: HTTP server and API endpoints
- **device_params.c/h**: Centralized parameter management

### Dependencies
- `esp_http_server`: HTTP server functionality
- `json`: JSON parsing and generation
- `nvs_flash`: Non-volatile storage
- `esp_netif`: Network interface
- `esp_event`: Event handling

### Memory Usage
- WiFi stack: ~40KB RAM
- Web server: ~8KB RAM
- Parameter storage: Minimal

## Future Enhancements

Potential improvements:
- WiFi credentials stored in NVS (non-volatile storage)
- WiFi access point mode for initial configuration
- OTA (Over-The-Air) firmware updates
- Real-time audio visualization in web interface
- Multiple user profiles
- Parameter presets

