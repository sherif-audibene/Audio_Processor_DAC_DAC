#include "esp_log.h"
#include "audio_processor.h"
#include "audio_buffer_manager.h"
#include "i2s_config.h"
#include "lcd_task.h"
#include "lcd_display.h"
#include "i2c_scanner.h"
#include "driver/i2c.h"

static const char *TAG = "MAIN";




/**
 * @brief Main application entry point
 */
void app_main(void) {
    ESP_LOGI(TAG, "Starting I2S Audio Passthrough Application with LCD Display");

    // First, scan I2C bus to detect LCD
    ESP_LOGI(TAG, "Scanning I2C bus for LCD display...");
    
    // Temporarily initialize I2C for scanning
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 6,
        .scl_io_num = 7,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    
    esp_err_t ret = i2c_param_config(I2C_NUM_0, &i2c_conf);
    if (ret == ESP_OK) {
        ret = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
        if (ret == ESP_OK) {
            uint8_t detected_addr = 0;
            if (i2c_scan_bus(I2C_NUM_0, &detected_addr) == ESP_OK) {
                ESP_LOGI(TAG, "LCD detected at I2C address: 0x%02X", detected_addr);
            } else {
                ESP_LOGW(TAG, "No I2C device detected on bus!");
                ESP_LOGW(TAG, "Check wiring: SDA=GPIO6, SCL=GPIO7");
            }
            // Remove I2C driver so LCD task can reinitialize it
            i2c_driver_delete(I2C_NUM_0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    // Display startup letter 'S' BEFORE starting LCD task
    ESP_LOGI(TAG, "Initializing LCD for startup screen...");
    lcd_config_t startup_lcd_config = {
        .sda_pin = 6,
        .scl_pin = 7,
        .i2c_address = 0x3F,
        .i2c_freq_hz = 100000,
        .contrast = 35,
        .flip_horizontal = false,
        .flip_vertical = false
    };
    
    ret = lcd_display_init(&startup_lcd_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Drawing startup letter 'S'...");
        lcd_display_draw_letter_s(40, 8, 48);  // Center the S on screen (128x64)
        lcd_display_update();
        vTaskDelay(pdMS_TO_TICKS(2000));  // Show for 2 seconds
        
        // Clean up before starting LCD task
        lcd_display_cleanup();
        vTaskDelay(pdMS_TO_TICKS(100));  // Wait for cleanup
        ESP_LOGI(TAG, "Startup display complete");
    } else {
        ESP_LOGW(TAG, "Failed to show startup screen: %s", esp_err_to_name(ret));
    }

    // Configure LCD display and waveform
    lcd_task_config_t lcd_config = {
        .sda_pin = 6,                    // SDA on GPIO 6
        .scl_pin = 7,                    // SCL on GPIO 7
        .lcd_contrast = 35,              // Contrast level (0-63)
        .waveform = {
            .samples_per_screen = 128,   // Display 128 samples across screen
            .time_scale = 1,             // Time scale multiplier
            .amplitude_scale = 100,      // 100% amplitude scale
            .show_grid = true,           // Show grid lines
            .show_center_line = true,    // Show center reference line
            .mode = WAVEFORM_MODE_OSCILLOSCOPE
        }
    };

    // Initialize and start LCD display task
    ESP_LOGI(TAG, "Initializing LCD display task...");
    ret = lcd_task_start(&lcd_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start LCD task: %s", esp_err_to_name(ret));
        ESP_LOGW(TAG, "Continuing without LCD display...");
    } else {
        ESP_LOGI(TAG, "LCD display task started successfully");
    }

    // Configure audio processing
    audio_config_t audio_config = {
        .volume_scale = 0.5f,
        .enable_debug = true,
        .enable_channel_swap = true,
        .enable_delay = true,          // Enable delay effect
        .delay_time_ms = 250.0f,       // 250ms delay time
        .delay_mix = 0.9f,             // 90% wet signal
        .delay_feedback = 0.4f         // 40% feedback for multiple echoes
    };

    // Initialize audio processor
    ret = audio_processor_init(&audio_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize audio processor: %s", esp_err_to_name(ret));
        lcd_task_stop();
        return;
    }

    // Start audio processing
    ret = audio_processor_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start audio processor: %s", esp_err_to_name(ret));
        audio_processor_cleanup();
        lcd_task_stop();
        return;
    }

    // Connect LCD task to audio buffer (direct access, no copying!)
    if (lcd_task_is_running()) {
        int32_t *audio_buf = audio_buffer_manager_get_buffer();
        size_t audio_buf_size = audio_buffer_manager_get_buffer_size();
        
        ESP_LOGI(TAG, "Audio buffer info BEFORE connecting:");
        ESP_LOGI(TAG, "  - Buffer pointer: %p", audio_buf);
        ESP_LOGI(TAG, "  - Buffer size: %d bytes", audio_buf_size);
        
        if (audio_buf == NULL) {
            ESP_LOGE(TAG, "Audio buffer is NULL! Cannot connect to LCD");
        } else {
            ret = lcd_task_get_audio_source(&audio_buf, &audio_buf_size);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "LCD task connected to audio buffer successfully");
            } else {
                ESP_LOGW(TAG, "Failed to connect LCD to audio buffer: %s", esp_err_to_name(ret));
            }
        }
    } else {
        ESP_LOGE(TAG, "LCD task is NOT running!");
    }

    ESP_LOGI(TAG, "Application started successfully!");
    ESP_LOGI(TAG, "Waveform Display Settings:");
    ESP_LOGI(TAG, "  - Resolution: %d samples per screen", lcd_config.waveform.samples_per_screen);
    ESP_LOGI(TAG, "  - Amplitude Scale: %d%%", lcd_config.waveform.amplitude_scale);
    ESP_LOGI(TAG, "  - Grid: %s", lcd_config.waveform.show_grid ? "ON" : "OFF");
    ESP_LOGI(TAG, "  - Direct buffer access (zero-copy)");
}
