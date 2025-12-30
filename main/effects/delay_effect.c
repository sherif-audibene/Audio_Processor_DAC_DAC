#include "delay_effect.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "DELAY_EFFECT";

// Effect state
static delay_effect_config_t config;
static bool initialized = false;

// Delay buffer
static int32_t *delay_buffer = NULL;
static size_t delay_buffer_size = 0;
static size_t delay_write_pos = 0;

esp_err_t delay_effect_init(const delay_effect_config_t *init_config) {
    if (initialized) {
        ESP_LOGW(TAG, "Delay effect already initialized");
        return ESP_OK;
    }

    if (!init_config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }

    // Allocate delay buffer (500ms max delay at 48kHz stereo)
    const size_t max_delay_samples = EFFECT_SAMPLE_RATE / 2;  // 500ms at 48kHz, stereo
    delay_buffer_size = max_delay_samples;
    delay_buffer = (int32_t *)calloc(delay_buffer_size, sizeof(int32_t));
    
    if (delay_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate delay buffer (%u bytes)",
                 (unsigned int)(delay_buffer_size * sizeof(int32_t)));
        return ESP_ERR_NO_MEM;
    }
    
    delay_write_pos = 0;
    memcpy(&config, init_config, sizeof(delay_effect_config_t));
    
    ESP_LOGI(TAG, "Delay buffer allocated: %u samples (%u KB)",
             (unsigned int)delay_buffer_size,
             (unsigned int)(delay_buffer_size * sizeof(int32_t) / 1024));

    initialized = true;
    return ESP_OK;
}

void delay_effect_update_config(const delay_effect_config_t *new_config) {
    if (!new_config || !initialized) {
        return;
    }
    memcpy(&config, new_config, sizeof(delay_effect_config_t));
}

void delay_effect_process(int32_t *left_sample, int32_t *right_sample,
                          int32_t orig_left, int32_t orig_right) {
    if (!config.enable || delay_buffer == NULL || delay_buffer_size == 0) {
        return;
    }

    // Calculate delay offset in samples (stereo pairs)
    size_t delay_samples = (size_t)((config.delay_time_ms / 1000.0f) * EFFECT_SAMPLE_RATE * 2);
    delay_samples = (delay_samples / 2) * 2;  // Ensure even number (stereo pairs)
    
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
        *left_sample = (int32_t)(orig_left + (delayed_left * config.mix));
        *right_sample = (int32_t)(orig_right + (delayed_right * config.mix));
    }
    
    // Write original input to delay buffer with feedback
    int32_t delayed_left_fb = delay_buffer[delay_write_pos];
    int32_t delayed_right_fb = delay_buffer[delay_write_pos + 1];
    delay_buffer[delay_write_pos] = orig_left + (int32_t)(delayed_left_fb * config.feedback);
    delay_buffer[delay_write_pos + 1] = orig_right + (int32_t)(delayed_right_fb * config.feedback);
    
    // Advance write position (circular buffer)
    delay_write_pos += 2;
    if (delay_write_pos >= delay_buffer_size) {
        delay_write_pos = 0;
    }
}

void delay_effect_cleanup(void) {
    if (delay_buffer != NULL) {
        free(delay_buffer);
        delay_buffer = NULL;
        delay_buffer_size = 0;
        delay_write_pos = 0;
    }
    initialized = false;
    ESP_LOGI(TAG, "Delay effect cleaned up");
}

bool delay_effect_is_enabled(void) {
    return config.enable;
}

