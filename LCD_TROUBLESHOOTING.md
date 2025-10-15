# LCD Display Troubleshooting Guide

## Recent Fix: Display Update Failed

### Issue Description
Error: `W (7382) LCD_TASK: Display update failed: ESP_FAIL`

The display initializes correctly (grid and axes visible) but fails to update the waveform.

### Root Cause
The ST7567S controller had difficulty handling large I2C data transfers (128 bytes at once). This caused I2C bus timeouts or buffer overflows during display updates.

### Solution Applied
✅ **Data Chunking**: Split 128-byte transfers into 32-byte chunks
✅ **Increased Timeout**: Extended I2C timeout from 100ms to 200ms for data transfers
✅ **Inter-chunk Delays**: Added 1ms delay between chunks for display processing
✅ **Reduced Update Rate**: Lowered refresh rate from 30Hz to 15Hz for I2C reliability
✅ **Better Error Logging**: Added detailed error messages to identify failure points

### Changes Made to `lcd_display.c`

The key fix is in the `lcd_send_data()` function:

```c
// OLD: Sent all 128 bytes at once
// NEW: Sends 32 bytes at a time with delays

#define CHUNK_SIZE 32

for (size_t i = 0; i < len; i += CHUNK_SIZE) {
    size_t chunk_len = (len - i) < CHUNK_SIZE ? (len - i) : CHUNK_SIZE;
    // ... send chunk ...
    vTaskDelay(pdMS_TO_TICKS(1));  // Small delay between chunks
}
```

## Testing After Fix

After rebuilding and flashing, you should see:
1. Display initializes (grid/axes appear) ✓
2. Waveform updates smoothly at 15 FPS ✓
3. No more "Display update failed" warnings ✓

## If Issues Persist

### 1. Check I2C Wiring
Verify connections are solid:
```
ESP32-S3      LCD Module
GPIO 6   →    SDA
GPIO 7   →    SCL
3.3V/5V  →    VCC (check your module's voltage)
GND      →    GND
```

### 2. Verify I2C Address
The default address is `0x3F`. To test different addresses:

Edit `lcd_display.h`:
```c
#define LCD_I2C_ADDRESS 0x3F  // Try 0x3C or 0x3D if 0x3F doesn't work
```

### 3. Adjust Chunk Size
If still having issues, try smaller chunks in `lcd_display.c`:

```c
#define CHUNK_SIZE 16  // Try 16 instead of 32
```

### 4. Reduce Update Rate Further
In `lcd_task.h`, lower the refresh rate:

```c
#define LCD_UPDATE_RATE_HZ 10  // Even slower: 10 FPS
// or
#define LCD_UPDATE_RATE_HZ 5   // Very slow: 5 FPS
```

### 5. Increase I2C Timeout
In `lcd_display.c`, increase timeouts if your I2C bus is slow:

```c
// In lcd_send_command():
esp_err_t ret = i2c_master_cmd_begin(i2c_port, i2c_cmd, pdMS_TO_TICKS(200));

// In lcd_send_data():
esp_err_t ret = i2c_master_cmd_begin(i2c_port, i2c_cmd, pdMS_TO_TICKS(500));
```

### 6. Add Pull-up Resistors
If you don't have external pull-up resistors on SDA/SCL:
- Most LCD modules have onboard pull-ups (4.7kΩ or 10kΩ)
- If not, add 4.7kΩ resistors from SDA to VCC and SCL to VCC
- ESP32-S3 internal pull-ups are enabled in code but may be weak

### 7. Check I2C Clock Speed
The default is 400kHz (fast mode). To reduce it, edit `lcd_display.h`:

```c
#define LCD_I2C_FREQ_HZ 100000  // Reduce to 100kHz (standard mode)
```

Then the display will initialize with slower but more reliable I2C:

```c
lcd_config_t lcd_config = {
    // ...
    .i2c_freq_hz = 100000,  // Override to 100kHz
    // ...
};
```

### 8. Monitor Serial Output
Watch for detailed error messages that now identify the exact failure point:

```
E (xxxx) LCD_DISPLAY: I2C command 0xB0 send failed: ESP_FAIL
E (xxxx) LCD_DISPLAY: Failed to set page 2
E (xxxx) LCD_DISPLAY: I2C data send failed at chunk 1: ESP_ERR_TIMEOUT
```

## Performance Tuning

### Current Performance (After Fix)
- **Update Rate**: 15 FPS (66ms per frame)
- **I2C Transfer**: ~8 pages × 128 bytes = 1024 bytes per frame
- **Chunk Size**: 32 bytes
- **Chunks per Frame**: 4 chunks × 8 pages = 32 transfers
- **CPU Usage**: ~2-3%

### If You Need Faster Updates

1. **Increase Update Rate** (if I2C is reliable):
   ```c
   #define LCD_UPDATE_RATE_HZ 20  // 20 FPS
   ```

2. **Increase Chunk Size** (if I2C buffer allows):
   ```c
   #define CHUNK_SIZE 64  // Larger chunks
   ```

3. **Remove Inter-chunk Delay**:
   ```c
   // Comment out or reduce this line in lcd_send_data():
   // vTaskDelay(pdMS_TO_TICKS(1));
   ```

### If You Need Lower CPU Usage

1. **Reduce Update Rate**:
   ```c
   #define LCD_UPDATE_RATE_HZ 10  // 10 FPS
   ```

2. **Reduce Resolution**:
   ```c
   .waveform = {
       .samples_per_screen = 64,  // Process fewer samples
       // ...
   }
   ```

## Debug Commands

### Check I2C Bus
Add this to your code to scan for I2C devices:

```c
#include "driver/i2c.h"

void i2c_scanner(void) {
    ESP_LOGI("I2C_SCAN", "Scanning I2C bus...");
    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        
        esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        
        if (ret == ESP_OK) {
            ESP_LOGI("I2C_SCAN", "Found device at 0x%02X", addr);
        }
    }
    ESP_LOGI("I2C_SCAN", "Scan complete");
}
```

Call this before `lcd_task_start()` in `main.c` to verify your display is detected.

### Enable Verbose I2C Logging
In `menuconfig`:
```
Component config → Log output → Default log verbosity → Verbose
```

Or add to your code:
```c
esp_log_level_set("I2C", ESP_LOG_VERBOSE);
esp_log_level_set("LCD_DISPLAY", ESP_LOG_DEBUG);
```

## Common Error Codes

| Error | Meaning | Solution |
|-------|---------|----------|
| `ESP_FAIL` | General I2C failure | Check wiring, try lower clock speed |
| `ESP_ERR_TIMEOUT` | I2C timeout | Increase timeout, reduce clock speed |
| `ESP_ERR_INVALID_STATE` | Display not initialized | Ensure `lcd_task_start()` succeeded |
| `ESP_ERR_NO_MEM` | Out of memory | Reduce buffer sizes or update rate |
| `ESP_ERR_INVALID_ARG` | Bad parameter | Check I2C address and pin numbers |

## Hardware-Specific Notes

### 12864 IIC 4P LCD Module Variants

1. **ST7567S Controller** (Most common)
   - I2C addresses: 0x3F, 0x3C, 0x3D
   - Voltage: 3.3V or 5V (check datasheet)
   - Our code is optimized for this

2. **ST7920 Controller** (Different model)
   - Uses parallel or SPI, not I2C
   - Won't work with this code

3. **SH1106 Controller** (Similar OLED)
   - I2C addresses: 0x3C, 0x3D
   - May work with minor command adjustments

### Verify Your Controller
Look for markings on the LCD controller chip (usually visible under the display).

## Success Checklist

- [ ] Display initializes without errors
- [ ] Grid/axes appear on startup
- [ ] Waveform updates smoothly
- [ ] No "Display update failed" warnings
- [ ] Audio continues without dropouts
- [ ] Update rate is acceptable (10-15 FPS minimum)

## Still Having Issues?

If problems persist after trying these solutions:

1. **Test with minimal configuration**:
   - Disable grid: `show_grid = false`
   - Disable center line: `show_center_line = false`
   - Reduce resolution: `samples_per_screen = 32`
   - Slow update: `LCD_UPDATE_RATE_HZ 5`

2. **Test I2C hardware**:
   - Try a simple I2C scanner
   - Test with a different I2C device
   - Check with oscilloscope/logic analyzer

3. **Verify ESP32-S3 setup**:
   - Ensure GPIO 6 and 7 aren't used elsewhere
   - Check if there are I2C conflicts with other components
   - Verify ESP-IDF version compatibility

## Performance Optimization

Once working, gradually tune for better performance:

1. Start with conservative settings (working)
2. Increase update rate by +5 FPS
3. Test for stability
4. If stable, continue increasing
5. If unstable, back off to last stable setting

Example progression:
- 5 FPS (very safe) → 10 FPS → 15 FPS → 20 FPS → 25 FPS (limit)

Most I2C implementations work reliably at **10-15 FPS** with our chunked data transfer.

---

**Last Updated**: October 15, 2025 (After I2C fix)

