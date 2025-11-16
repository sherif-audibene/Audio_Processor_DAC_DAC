#include "audio_effects.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "AUDIO_EFFECTS";

// Effect state
static audio_effects_config_t current_config;
static bool effects_initialized = false;

// Delay effect state
static int32_t *delay_buffer = NULL;
static size_t delay_buffer_size = 0;
static size_t delay_write_pos = 0;

/**
 * @brief Apply delay effect to audio samples
 */
static void apply_delay_effect(int32_t *left_sample, int32_t *right_sample, 
                               int32_t orig_left, int32_t orig_right) {
    if (!current_config.enable_delay || delay_buffer == NULL || delay_buffer_size == 0) {
        return;
    }

    // Calculate delay offset in samples (stereo pairs)
    size_t delay_samples = (size_t)((current_config.delay_time_ms / 1000.0f) * 48000.0f * 2); // *2 for stereo
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
        *left_sample = (int32_t)(orig_left + (delayed_left * current_config.delay_mix));
        *right_sample = (int32_t)(orig_right + (delayed_right * current_config.delay_mix));
    }
    
    // Write original input to delay buffer with feedback
    int32_t delayed_left_fb = delay_buffer[delay_write_pos];
    int32_t delayed_right_fb = delay_buffer[delay_write_pos + 1];
    delay_buffer[delay_write_pos] = orig_left + (int32_t)(delayed_left_fb * current_config.delay_feedback);
    delay_buffer[delay_write_pos + 1] = orig_right + (int32_t)(delayed_right_fb * current_config.delay_feedback);
    
    // Advance write position (circular buffer)
    delay_write_pos += 2;
    if (delay_write_pos >= delay_buffer_size) {
        delay_write_pos = 0;
    }
}

/**
 * @brief Apply volume scaling and channel swap
 */
static void apply_volume_and_swap(int32_t *samples, size_t index, 
                                  int32_t left_sample, int32_t right_sample) {
    if (current_config.enable_channel_swap) {
        // Swap channels: right becomes left, left becomes right
        samples[index] = (int32_t)(right_sample * current_config.volume_scale * 2);
        samples[index + 1] = (int32_t)(left_sample * current_config.volume_scale);
    } else {
        // Apply volume scaling without channel swap
        samples[index] = (int32_t)(left_sample * current_config.volume_scale * 2);
        samples[index + 1] = (int32_t)(right_sample * current_config.volume_scale);
    }
}

void audio_effects_process(int32_t *samples, size_t sample_count) {
    if (!samples || sample_count == 0 || !effects_initialized) {
        return;
    }

    // Process stereo samples (2 channels)
    for (size_t i = 0; i < sample_count; i += 2) {
        if (i + 1 >= sample_count) break; // Ensure we have a stereo pair
        
        int32_t left_sample = samples[i];
        int32_t right_sample = samples[i + 1];
        
        // Apply delay effect if enabled
        apply_delay_effect(&left_sample, &right_sample, samples[i], samples[i + 1]);
        
        // Apply volume and channel swap
        apply_volume_and_swap(samples, i, left_sample, right_sample);
    }
}

esp_err_t audio_effects_init(const audio_effects_config_t *config) {
    if (effects_initialized) {
        ESP_LOGW(TAG, "Audio effects already initialized");
        return ESP_OK;
    }

    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration provided");
        return ESP_ERR_INVALID_ARG;
    }

    // Allocate delay buffer (500ms max delay at 48kHz stereo = ~96KB)
    const size_t max_delay_samples = 48000 / 2 ; // 500ms at 48kHz, stereo
    delay_buffer_size = max_delay_samples;
    delay_buffer = (int32_t *)calloc(delay_buffer_size, sizeof(int32_t));
    if (delay_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate delay buffer (%u bytes)", 
                 (unsigned int)(delay_buffer_size * sizeof(int32_t)));
        return ESP_ERR_NO_MEM;
    }
    delay_write_pos = 0;
    
    ESP_LOGI(TAG, "Delay buffer allocated: %u samples (%u KB)", 
             (unsigned int)delay_buffer_size, 
             (unsigned int)(delay_buffer_size * sizeof(int32_t) / 1024));

    // Store configuration
    memcpy(&current_config, config, sizeof(audio_effects_config_t));
    
    effects_initialized = true;
    ESP_LOGI(TAG, "Audio effects initialized successfully");
    return ESP_OK;
}

void audio_effects_update_config(const audio_effects_config_t *config) {
    if (!config || !effects_initialized) {
        return;
    }
    memcpy(&current_config, config, sizeof(audio_effects_config_t));
}

void audio_effects_cleanup(void) {
    if (delay_buffer != NULL) {
        free(delay_buffer);
        delay_buffer = NULL;
        delay_buffer_size = 0;
        delay_write_pos = 0;
    }
    
    effects_initialized = false;
    ESP_LOGI(TAG, "Audio effects cleaned up");
}

