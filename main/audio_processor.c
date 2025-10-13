#include "audio_processor.h"
#include "i2s_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "AUDIO_PROCESSOR";

// Private variables
static TaskHandle_t audio_task_handle = NULL;
static int32_t *audio_buffer = NULL;
static audio_config_t current_config;
static bool audio_initialized = false;

// Delay effect variables
static int32_t *delay_buffer = NULL;
static size_t delay_buffer_size = 0;
static size_t delay_write_pos = 0;

/**
 * @brief Process audio samples with channel swap, volume scaling, and delay effect
 * Note: 24-bit samples are stored in 32-bit containers (int32_t)
 */
void audio_process_samples(int32_t *samples, size_t sample_count, const audio_config_t *config) {
    if (!samples || sample_count == 0 || !config) {
        return;
    }

    // Process stereo samples (2 channels)
    for (size_t i = 0; i < sample_count; i += 2) {
        if (i + 1 >= sample_count) break; // Ensure we have a stereo pair
        
        int32_t left_sample = samples[i];
        int32_t right_sample = samples[i + 1];
        
        // Apply delay effect if enabled
        if (config->enable_delay && delay_buffer != NULL && delay_buffer_size > 0) {
            // Calculate delay offset in samples (stereo pairs)
            size_t delay_samples = (size_t)((config->delay_time_ms / 1000.0f) * 48000.0f * 2); // *2 for stereo
            delay_samples = (delay_samples / 2) * 2; // Ensure even number (stereo pairs)
            
            if (delay_samples > delay_buffer_size) {
                delay_samples = delay_buffer_size;
            }
            
            // Only apply delay if we have enough samples
            if (delay_samples > 0) {
                // Calculate read position (circular buffer)
                size_t delay_read_pos = (delay_write_pos >= delay_samples) ? 
                                        (delay_write_pos - delay_samples) : 
                                        (delay_buffer_size - (delay_samples - delay_write_pos));
                
                // Read delayed samples
                int32_t delayed_left = delay_buffer[delay_read_pos];
                int32_t delayed_right = delay_buffer[delay_read_pos + 1];
                
                // Mix delayed signal with input
                left_sample = (int32_t)(left_sample + (delayed_left * config->delay_mix));
                right_sample = (int32_t)(right_sample + (delayed_right * config->delay_mix));
            }
            
            // Write original input to delay buffer (not the mixed output!)
            // Store the dry signal plus feedback from the delayed signal
            int32_t delayed_left_fb = delay_buffer[delay_write_pos];
            int32_t delayed_right_fb = delay_buffer[delay_write_pos + 1];
            delay_buffer[delay_write_pos] = samples[i] + (int32_t)(delayed_left_fb * config->delay_feedback);
            delay_buffer[delay_write_pos + 1] = samples[i + 1] + (int32_t)(delayed_right_fb * config->delay_feedback);
            
            // Advance write position (circular buffer)
            delay_write_pos += 2;
            if (delay_write_pos >= delay_buffer_size) {
                delay_write_pos = 0;
            }
        }
        
        // Apply channel swap and volume
        if (config->enable_channel_swap) {
            // Swap channels: right becomes left, left becomes right
            samples[i] = (int32_t)(right_sample * config->volume_scale*2);
            samples[i + 1] = (int32_t)(left_sample * config->volume_scale);
        } else {
            // Apply volume scaling without channel swap
            samples[i] = (int32_t)(left_sample * config->volume_scale*2);
            samples[i + 1] = (int32_t)(right_sample * config->volume_scale);
        }
    }
}

/**
 * @brief Audio passthrough task implementation
 */
static void audio_passthrough_task(void *pvParameters) {
    size_t bytes_read;
    size_t bytes_written;
    esp_err_t ret;
    const size_t read_write_size = BUFFER_SIZE * CHANNELS * sizeof(int32_t);
    static int debug_counter = 0;
    static int write_counter = 0;
    static int64_t last_time = 0;

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
        ret = i2s_read(I2S_ADC_NUM, audio_buffer, read_write_size, &bytes_read, 
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

        
        // Process audio samples
        int32_t *samples = (int32_t *)audio_buffer;
        size_t sample_count = bytes_read / sizeof(int32_t);
        audio_process_samples(samples, sample_count, &current_config);

        
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

        // Track i2s_write calls per second
/*         write_counter++;
        int64_t current_time = esp_timer_get_time(); // microseconds
        if (last_time == 0) {
            last_time = current_time;
        }
        if ((current_time - last_time) >= 1000000) { // 1 second elapsed
            ESP_LOGI(TAG, "i2s_write calls/sec: %d", write_counter);
            // Print the content of audio_buffer (interpreted as int32_t samples)
            {
                size_t num_samples = bytes_read / sizeof(int32_t);
                int32_t *samples = (int32_t *)audio_buffer;
                ESP_LOGI(TAG, "audio_buffer (first %u samples):", (unsigned int)num_samples < 16 ? (unsigned int)num_samples : 16);
                for (size_t i = 0; i < num_samples && i < 16; ++i) {
                    ESP_LOGI(TAG, "[%u]: %ld", (unsigned int)i, (long)samples[i]);
                }
            }
            write_counter = 0;
            last_time = current_time;
        } */

        //vTaskDelay(pdMS_TO_TICKS(1));
    }
}

esp_err_t audio_processor_init(const audio_config_t *config) {
    if (audio_initialized) {
        return ESP_OK;
    }

    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration provided");
        return ESP_ERR_INVALID_ARG;
    }

    // Allocate audio buffer for 24-bit samples (stored in 32-bit containers)
    audio_buffer = malloc(BUFFER_SIZE * CHANNELS * sizeof(int32_t));
    if (audio_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        return ESP_ERR_NO_MEM;
    }

    // Allocate delay buffer (500ms max delay at 48kHz stereo = ~96KB)
    const size_t max_delay_samples = 48000 / 2 * 2; // 500ms at 48kHz, stereo (48KB samples = 192KB)
    delay_buffer_size = max_delay_samples;
    delay_buffer = (int32_t *)calloc(delay_buffer_size, sizeof(int32_t));
    if (delay_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate delay buffer (%u bytes)", 
                 (unsigned int)(delay_buffer_size * sizeof(int32_t)));
        free(audio_buffer);
        return ESP_ERR_NO_MEM;
    }
    delay_write_pos = 0;
    ESP_LOGI(TAG, "Delay buffer allocated: %u samples (%u KB)", 
             (unsigned int)delay_buffer_size, 
             (unsigned int)(delay_buffer_size * sizeof(int32_t) / 1024));

    // Store configuration
    memcpy(&current_config, config, sizeof(audio_config_t));
    
    audio_initialized = true;
    ESP_LOGI(TAG, "Audio processor initialized successfully");
    return ESP_OK;
}

esp_err_t audio_processor_start(void) {
    if (!audio_initialized) {
        ESP_LOGE(TAG, "Audio processor not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (audio_task_handle != NULL) {
        ESP_LOGW(TAG, "Audio processor already running");
        return ESP_OK;
    }

    BaseType_t ret = xTaskCreate(audio_passthrough_task, "audio_passthrough", 
                                AUDIO_TASK_STACK_SIZE, NULL, 
                                AUDIO_TASK_PRIORITY, &audio_task_handle);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio passthrough task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Audio processor started successfully");
    return ESP_OK;
}

void audio_processor_stop(void) {
    if (audio_task_handle != NULL) {
        vTaskDelete(audio_task_handle);
        audio_task_handle = NULL;
        ESP_LOGI(TAG, "Audio processor stopped");
    }
}

void audio_processor_cleanup(void) {
    audio_processor_stop();
    
    if (audio_buffer != NULL) {
        free(audio_buffer);
        audio_buffer = NULL;
    }
    
    if (delay_buffer != NULL) {
        free(delay_buffer);
        delay_buffer = NULL;
        delay_buffer_size = 0;
        delay_write_pos = 0;
    }
    
    i2s_dac_cleanup();
    i2s_adc_cleanup();
    
    audio_initialized = false;
    ESP_LOGI(TAG, "Audio processor cleaned up");
}
