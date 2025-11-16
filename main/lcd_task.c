#include "lcd_task.h"
#include "lcd_display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>
#include <math.h>

static const char *TAG = "LCD_TASK";

// Task handle
static TaskHandle_t lcd_task_handle = NULL;

// Configuration mutex
static SemaphoreHandle_t config_mutex = NULL;
static waveform_config_t current_waveform_config;

// Direct audio buffer access (no queue, no copying!)
static int32_t *audio_source_buffer = NULL;
static size_t audio_source_size = 0;
static SemaphoreHandle_t audio_buffer_mutex = NULL;

// Waveform buffer for display
#define MAX_WAVEFORM_SAMPLES 4000
static int16_t waveform_buffer[MAX_WAVEFORM_SAMPLES];
static size_t waveform_buffer_index = 0;
static bool waveform_buffer_filled = false;  // Track if we have valid samples

/**
 * @brief Convert 24-bit I2S sample to 16-bit for display
 */
static inline int16_t convert_sample_to_display(int32_t sample) {
    // I2S samples are 24-bit in 32-bit container, MSB aligned
    // Shift right by 8 to get 24-bit value, then scale to 16-bit
    return (int16_t)((sample >> 16) & 0xFFFF);
}

/**
 * @brief Draw grid on display
 */
static void draw_grid(void) {
    // Vertical lines every 32 pixels
    for (uint8_t x = 32; x < LCD_WIDTH; x += 32) {
        for (uint8_t y = 0; y < LCD_HEIGHT; y += 4) {
            lcd_display_set_pixel(x, y, 1);
        }
    }
    
    // Horizontal lines every 16 pixels
    for (uint8_t y = 16; y < LCD_HEIGHT; y += 16) {
        for (uint8_t x = 0; x < LCD_WIDTH; x += 4) {
            lcd_display_set_pixel(x, y, 1);
        }
    }
}

/**
 * @brief Draw center reference line
 */
static void draw_center_line(void) {
    uint8_t center_y = LCD_HEIGHT / 2;
    for (uint8_t x = 0; x < LCD_WIDTH; x += 2) {
        lcd_display_set_pixel(x, center_y, 1);
    }
}

/**
 * @brief Draw waveform on display
 */
static uint32_t draw_count = 0;
static void draw_waveform(void) {
    if (!waveform_buffer_filled) {
        static uint32_t warn_count = 0;
        // More frequent warnings at startup, then reduce
        uint32_t warn_interval = (warn_count < 10) ? 1 : 100;
        if (warn_count++ % warn_interval == 0) {
            ESP_LOGW(TAG, "Buffer not yet filled: waveform_buffer_index=%d (waiting for audio data)", waveform_buffer_index);
        }
        return;  // Not enough samples to draw yet
    }
    
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    waveform_config_t config = current_waveform_config;
    xSemaphoreGive(config_mutex);
    
    // Calculate how many samples to display
    // Once buffer is filled, we always have enough samples (it's circular)
    uint16_t samples_to_display = config.samples_per_screen;
    if (samples_to_display > MAX_WAVEFORM_SAMPLES) {
        samples_to_display = MAX_WAVEFORM_SAMPLES;
    }
    
    // If buffer is not yet completely filled (rare edge case at startup)
    if (!waveform_buffer_filled && samples_to_display > waveform_buffer_index) {
        samples_to_display = waveform_buffer_index;
    }
    
    
    // Calculate decimation factor
    float x_step = (float)LCD_WIDTH / samples_to_display;
    
    // Draw the waveform
    uint8_t center_y = LCD_HEIGHT / 2;
    
    for (uint16_t i = 0; i < samples_to_display - 1; i++) {
        // Get sample from circular buffer
        uint16_t index1 = (waveform_buffer_index - samples_to_display + i) % MAX_WAVEFORM_SAMPLES;
        uint16_t index2 = (waveform_buffer_index - samples_to_display + i + 1) % MAX_WAVEFORM_SAMPLES;
        
        int16_t sample1 = waveform_buffer[index1];
        int16_t sample2 = waveform_buffer[index2];
        
        // Apply amplitude scaling
        sample1 = (sample1 * config.amplitude_scale) / 100;
        sample2 = (sample2 * config.amplitude_scale) / 100;
        
        // Convert to display coordinates
        // Scale to fit display height
        int16_t y1 = center_y - ((sample1 * (LCD_HEIGHT / 2)) / 32768);
        int16_t y2 = center_y - ((sample2 * (LCD_HEIGHT / 2)) / 32768);
        
        // Clamp to display bounds
        if (y1 < 0) y1 = 0;
        if (y1 >= LCD_HEIGHT) y1 = LCD_HEIGHT - 1;
        if (y2 < 0) y2 = 0;
        if (y2 >= LCD_HEIGHT) y2 = LCD_HEIGHT - 1;
        
        // Calculate X positions
        uint8_t x1 = (uint8_t)(i * x_step);
        uint8_t x2 = (uint8_t)((i + 1) * x_step);
        
        if (x1 >= LCD_WIDTH) x1 = LCD_WIDTH - 1;
        if (x2 >= LCD_WIDTH) x2 = LCD_WIDTH - 1;
        
        // Draw line between points
        lcd_display_draw_line(x1, y1, x2, y2, 1);
    }
}

/**
 * @brief Process audio data directly from audio buffer and update waveform buffer
 */
static uint32_t debug_sample_count = 0;
static void process_audio_data_from_source(void) {
    if (audio_source_buffer == NULL || audio_source_size == 0) {
        static uint32_t warn_count = 0;
        if (warn_count++ % 100 == 0) {
            ESP_LOGW(TAG, "No audio source buffer! Buffer=%p, Size=%d", audio_source_buffer, audio_source_size);
        }
        return;
    }
    
    // Take mutex to safely read from audio buffer
    if (xSemaphoreTake(audio_buffer_mutex, pdMS_TO_TICKS(5)) != pdTRUE) {
        return;  // Skip this update if buffer is busy
    }
    
    size_t sample_count = audio_source_size / sizeof(int32_t);
    
    // Decimate samples to fit into waveform buffer
    // Take only left channel (every other sample for stereo)
    size_t decimation = 1;
    if (sample_count > MAX_WAVEFORM_SAMPLES * 2) {
        decimation = sample_count / (MAX_WAVEFORM_SAMPLES * 2);
    }
    
    
    size_t samples_added = 0;
    for (size_t i = 0; i < sample_count && i < audio_source_size / sizeof(int32_t); i += (decimation * 2)) {
        // Convert and store sample (left channel only)
        int16_t converted = convert_sample_to_display(audio_source_buffer[i]);
        waveform_buffer[waveform_buffer_index] = converted;
        waveform_buffer_index = (waveform_buffer_index + 1) % MAX_WAVEFORM_SAMPLES;
        samples_added++;
    }
    
    // Mark buffer as filled once we've wrapped around at least once, or have enough samples
    if (samples_added > 0) {
        // If we added samples and now index is small, we must have wrapped
        if (waveform_buffer_index < samples_added) {
            waveform_buffer_filled = true;  // Wrapped around = buffer full
        } else if (waveform_buffer_index >= 128) {
            waveform_buffer_filled = true;  // Half full is good enough
        }
    }
    
    xSemaphoreGive(audio_buffer_mutex);
}

/**
 * @brief LCD display task implementation
 */
static void lcd_display_task(void *pvParameters) {
    ESP_LOGI(TAG, "LCD display task started");
    
    TickType_t last_update_time = xTaskGetTickCount();
    const TickType_t update_interval = pdMS_TO_TICKS(1000 / LCD_UPDATE_RATE_HZ);
    
    uint32_t loop_count = 0;
    
    while (1) {
        
        // Process audio data directly from source buffer
        process_audio_data_from_source();
        
        // Update display at fixed rate
        TickType_t current_time = xTaskGetTickCount();
        if ((current_time - last_update_time) >= update_interval) {
            last_update_time = current_time;
            
            // Clear display
            memset(lcd_display_get_framebuffer(), 0, LCD_WIDTH * LCD_PAGES);
            
            xSemaphoreTake(config_mutex, portMAX_DELAY);
            bool show_grid = current_waveform_config.show_grid;
            bool show_center = current_waveform_config.show_center_line;
            xSemaphoreGive(config_mutex);
            
            // Draw grid if enabled
            if (show_grid) {
                draw_grid();
            }
            
            // Draw center line if enabled
            if (show_center) {
                draw_center_line();
            }
            
            // Draw waveform
            draw_waveform();
            
            // Update display
            esp_err_t ret = lcd_display_update();
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Display update failed: %s", esp_err_to_name(ret));
            }
        }
        
        // Small delay to prevent tight loop
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

esp_err_t lcd_task_start(const lcd_task_config_t *config) {
    if (lcd_task_handle != NULL) {
        ESP_LOGW(TAG, "LCD task already running");
        return ESP_OK;
    }
    
    if (config == NULL) {
        ESP_LOGE(TAG, "Config is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Create mutex for configuration protection
    config_mutex = xSemaphoreCreateMutex();
    if (config_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create config mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Create mutex for audio buffer access
    audio_buffer_mutex = xSemaphoreCreateMutex();
    if (audio_buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create audio buffer mutex");
        vSemaphoreDelete(config_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Store initial waveform configuration
    current_waveform_config = config->waveform;
    
    // Initialize LCD display
    lcd_config_t lcd_config = {
        .sda_pin = config->sda_pin,
        .scl_pin = config->scl_pin,
        .i2c_address = LCD_I2C_ADDRESS,
        .i2c_freq_hz = LCD_I2C_FREQ_HZ,
        .contrast = config->lcd_contrast,
        .flip_horizontal = false,
        .flip_vertical = false
    };
    
    esp_err_t ret = lcd_display_init(&lcd_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LCD display: %s", esp_err_to_name(ret));
        vSemaphoreDelete(audio_buffer_mutex);
        vSemaphoreDelete(config_mutex);
        return ret;
    }
    
    // Clear waveform buffer
    memset(waveform_buffer, 0, sizeof(waveform_buffer));
    waveform_buffer_index = 0;
    waveform_buffer_filled = false;
    
    // Create LCD task
    BaseType_t task_ret = xTaskCreate(lcd_display_task, "lcd_display", 
                                     LCD_TASK_STACK_SIZE, NULL, 
                                     LCD_TASK_PRIORITY, &lcd_task_handle);
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LCD display task");
        lcd_display_cleanup();
        vSemaphoreDelete(audio_buffer_mutex);
        vSemaphoreDelete(config_mutex);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "LCD task started successfully");
    return ESP_OK;
}

void lcd_task_stop(void) {
    if (lcd_task_handle != NULL) {
        vTaskDelete(lcd_task_handle);
        lcd_task_handle = NULL;
        
        lcd_display_cleanup();
        
        if (audio_buffer_mutex != NULL) {
            vSemaphoreDelete(audio_buffer_mutex);
            audio_buffer_mutex = NULL;
        }
        
        if (config_mutex != NULL) {
            vSemaphoreDelete(config_mutex);
            config_mutex = NULL;
        }
        
        audio_source_buffer = NULL;
        audio_source_size = 0;
        
        ESP_LOGI(TAG, "LCD task stopped");
    }
}

bool lcd_task_is_running(void) {
    return (lcd_task_handle != NULL);
}

esp_err_t lcd_task_set_waveform_config(const waveform_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (config_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    current_waveform_config = *config;
    xSemaphoreGive(config_mutex);
    
    ESP_LOGI(TAG, "Waveform config updated: samples=%d, amp_scale=%d%%", 
             config->samples_per_screen, config->amplitude_scale);
    
    return ESP_OK;
}

esp_err_t lcd_task_get_waveform_config(waveform_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (config_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    *config = current_waveform_config;
    xSemaphoreGive(config_mutex);
    
    return ESP_OK;
}

esp_err_t lcd_task_get_audio_source(int32_t **buffer, size_t *size) {
    if (buffer == NULL || size == NULL) {
        ESP_LOGE(TAG, "Invalid arguments: buffer=%p, size=%p", buffer, size);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (audio_buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Audio buffer mutex not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Set the audio source buffer pointer (no copying!)
    xSemaphoreTake(audio_buffer_mutex, portMAX_DELAY);
    audio_source_buffer = *buffer;
    audio_source_size = *size;
    xSemaphoreGive(audio_buffer_mutex);
    
    ESP_LOGI(TAG, "LCD task configured to read from audio buffer:");
    ESP_LOGI(TAG, "  - Buffer address: %p", audio_source_buffer);
    ESP_LOGI(TAG, "  - Buffer size: %d bytes (%d samples)", audio_source_size, audio_source_size / sizeof(int32_t));
    ESP_LOGI(TAG, "  - First sample: 0x%08X (%d)", (unsigned int)audio_source_buffer[0], (int)audio_source_buffer[0]);
    
    return ESP_OK;
}

esp_err_t lcd_task_set_contrast(uint8_t contrast) {
    return lcd_display_set_contrast(contrast);
}

