#include "esp_log.h"
#include "audio_processor.h"
#include "audio_buffer_manager.h"
#include "i2s_config.h"
#include "lcd_task.h"
#include "lcd_display.h"
#include "i2c_scanner.h"
#include "led_control.h"
#include "driver/i2c.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "device_params.h"
#include "cpu_monitor.h"
#include "freertos/task.h"

static const char *TAG = "MAIN";

/**
 * @brief WiFi connection callback - called when WiFi connects or disconnects
 */
static void wifi_connection_callback(bool connected, const char *ip_str) {
    if (connected && ip_str != NULL) {
        ESP_LOGI(TAG, "WiFi connected! IP address: %s", ip_str);
        
        // Show confirmation message on LCD
        if (lcd_task_is_running()) {
            char message[32];
            snprintf(message, sizeof(message), "%s", ip_str);
            lcd_task_show_message(message);
        }
        
        // Start web server
        ESP_LOGI(TAG, "Starting web server...");
        esp_err_t ret = web_server_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start web server: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Web server started! Access control panel at http://%s", ip_str);
        }
    } else {
        ESP_LOGW(TAG, "WiFi disconnected");
    }
}




/**
 * @brief Main application entry point
 */
void app_main(void) {
    ESP_LOGI(TAG, "Starting I2S Audio Passthrough Application with LCD Display and WiFi");


    // Initialize device parameters system
    ESP_LOGI(TAG, "Initializing device parameters...");
    esp_err_t ret = device_params_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize device parameters: %s", esp_err_to_name(ret));
        return;
    }

    // Initialize WiFi (non-blocking - will connect in background)
    ESP_LOGI(TAG, "Initializing WiFi (non-blocking)...");
    wifi_manager_config_t wifi_config = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
        .timeout_ms = 10000
    };
    
    ret = wifi_manager_init(&wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        ESP_LOGW(TAG, "Continuing without WiFi...");
    } else {
        // Register callback for WiFi connection events
        wifi_manager_set_connection_callback(wifi_connection_callback);
        
        // Start WiFi connection in background (non-blocking)
        ret = wifi_manager_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "WiFi connection started in background, continuing with audio initialization...");
        }
    }

    // First, scan I2C bus to detect LCD
    ESP_LOGI(TAG, "Scanning I2C bus for LCD display...");
    
    // Temporarily initialize I2C for scanning
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 6,
        .scl_io_num = 7,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    
    ret = i2c_param_config(I2C_NUM_0, &i2c_conf);
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
        .i2c_freq_hz = 400000,
        .contrast = 20,
        .flip_horizontal = false,
        .flip_vertical = false
    };
    
    ret = lcd_display_init(&startup_lcd_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Drawing startup letter 'S'...");
        lcd_display_draw_letter_s(40, 8, 48);  // Center the S on screen (128x64)
        lcd_display_update();
        vTaskDelay(pdMS_TO_TICKS(500));  // Show for 2 seconds
        
        // Clean up before starting LCD task
        lcd_display_cleanup();
        vTaskDelay(pdMS_TO_TICKS(100));  // Wait for cleanup
        ESP_LOGI(TAG, "Startup display complete");
    } else {
        ESP_LOGW(TAG, "Failed to show startup screen: %s", esp_err_to_name(ret));
    }

    // Get device parameters (with defaults)
    device_params_t params;
    device_params_get(&params);

    // Initialize and start LCD display task
    ESP_LOGI(TAG, "Initializing LCD display task...");
    ret = lcd_task_start(&params.lcd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start LCD task: %s", esp_err_to_name(ret));
        ESP_LOGW(TAG, "Continuing without LCD display...");
    } else {
        ESP_LOGI(TAG, "LCD display task started successfully");
    }

    // Initialize LED control
    ESP_LOGI(TAG, "Initializing LED control...");
    ret = led_control_init(&params.led);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize LED control: %s", esp_err_to_name(ret));
        ESP_LOGW(TAG, "Continuing without LED control...");
    } else {
        ESP_LOGI(TAG, "LED control initialized successfully");
        
        // Test all LEDs to verify hardware connection
        ESP_LOGI(TAG, "Running LED hardware test...");
        ret = led_control_test_all(500);  // 500ms delay between tests
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "LED test failed: %s", esp_err_to_name(ret));
        }
    }

    // Initialize audio processor
    ret = audio_processor_init(&params.audio);
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
    ESP_LOGI(TAG, "  - Resolution: %d samples per screen", params.lcd.waveform.samples_per_screen);
    ESP_LOGI(TAG, "  - Amplitude Scale: %d%%", params.lcd.waveform.amplitude_scale);
    ESP_LOGI(TAG, "  - Grid: %s", params.lcd.waveform.show_grid ? "ON" : "OFF");
    ESP_LOGI(TAG, "  - Direct buffer access (zero-copy)");
    
    if (wifi_manager_is_connected()) {
        char ip_str[16];
        if (wifi_manager_get_ip(ip_str, sizeof(ip_str)) == ESP_OK) {
            ESP_LOGI(TAG, "Web interface available at: http://%s", ip_str);
        }
    }

    // Initialize CPU monitor for performance tracking (used by LCD stats mode)
    ret = cpu_monitor_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "CPU monitoring enabled - switch to STATS mode on LCD to view");
        // Note: To enable console logging, call: cpu_monitor_start_periodic(10);
    }
}
