#include "audio_task.h"
#include "audio_effects.h"
#include "audio_buffer_manager.h"
#include "i2s_config.h"
#include "lcd_task.h"
#include "esp_log.h"
#include "driver/i2s.h"
#include "freertos/task.h"

static const char *TAG = "AUDIO_TASK";

// Private variables
static TaskHandle_t audio_task_handle = NULL;

/**
 * @brief Audio passthrough task implementation
 */
static void audio_passthrough_task(void *pvParameters) {
    size_t bytes_read;
    size_t bytes_written;
    esp_err_t ret;
    
    int32_t *audio_buffer = audio_buffer_manager_get_buffer();
    const size_t buffer_size = audio_buffer_manager_get_buffer_size();

    ESP_LOGI(TAG, "Audio passthrough task started");

    // Initialize I2S peripherals
    if (i2s_adc_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S ADC");
        vTaskDelete(NULL);
        return;
    }
    
    if (i2s_dac_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S DAC");
        i2s_adc_cleanup();
        vTaskDelete(NULL);
        return;
    }

    // Start I2S peripherals
    if (i2s_adc_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start I2S ADC");
        i2s_dac_cleanup();
        i2s_adc_cleanup();
        vTaskDelete(NULL);
        return;
    }
    
    if (i2s_dac_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start I2S DAC");
        i2s_stop(I2S_ADC_NUM);
        i2s_dac_cleanup();
        i2s_adc_cleanup();
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Audio passthrough started successfully");

    // Main audio processing loop
    while (1) {
        // Read from ADC
        ret = i2s_read(I2S_ADC_NUM, audio_buffer, buffer_size, &bytes_read, 
                      pdMS_TO_TICKS(AUDIO_READ_TIMEOUT_MS));

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S ADC read failed: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (bytes_read == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Process audio samples with effects
        size_t sample_count = bytes_read / sizeof(int32_t);
        audio_effects_process(audio_buffer, sample_count);

        // LCD task reads directly from audio_buffer - no need to send data!
        // The buffer is shared and protected by mutex

        // Write to DAC
        ret = i2s_write(I2S_NUM, audio_buffer, bytes_read, &bytes_written, 
                       pdMS_TO_TICKS(AUDIO_WRITE_TIMEOUT_MS));

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S DAC write failed: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (bytes_written != bytes_read) {
            ESP_LOGW(TAG, "Partial write: %u bytes written, %u bytes read", 
                    (unsigned int)bytes_written, (unsigned int)bytes_read);
        }
    }
}

esp_err_t audio_task_start(void) {
    if (audio_task_handle != NULL) {
        ESP_LOGW(TAG, "Audio task already running");
        return ESP_OK;
    }

    BaseType_t ret = xTaskCreate(audio_passthrough_task, "audio_passthrough", 
                                AUDIO_TASK_STACK_SIZE, NULL, 
                                AUDIO_TASK_PRIORITY, &audio_task_handle);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio passthrough task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Audio task started successfully");
    return ESP_OK;
}

void audio_task_stop(void) {
    if (audio_task_handle != NULL) {
        vTaskDelete(audio_task_handle);
        audio_task_handle = NULL;
        ESP_LOGI(TAG, "Audio task stopped");
    }
}

bool audio_task_is_running(void) {
    return (audio_task_handle != NULL);
}

