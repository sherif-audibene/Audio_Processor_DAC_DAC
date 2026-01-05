#include "volume_effect.h"
#include "esp_log.h"
#include "dsps_mulc.h"
#include <string.h>

static const char *TAG = "VOLUME_EFFECT";

// Effect state
static volume_effect_config_t config;
static bool initialized = false;

// SIMD working buffer for batch processing
#define VOLUME_BATCH_SIZE 512
__attribute__((aligned(16)))
static float volume_float_buffer[VOLUME_BATCH_SIZE];

esp_err_t volume_effect_init(const volume_effect_config_t *init_config) {
    if (initialized) {
        ESP_LOGW(TAG, "Volume effect already initialized");
        return ESP_OK;
    }

    if (!init_config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&config, init_config, sizeof(volume_effect_config_t));
    
    initialized = true;
    ESP_LOGI(TAG, "Volume effect initialized");
    return ESP_OK;
}

void volume_effect_update_config(const volume_effect_config_t *new_config) {
    if (!new_config || !initialized) {
        return;
    }
    memcpy(&config, new_config, sizeof(volume_effect_config_t));
}

void volume_effect_process(int32_t *samples, size_t index,
                           int32_t left_sample, int32_t right_sample) {
    if (!initialized) {
        return;
    }

    if (config.enable_channel_swap) {
        // Swap channels: right becomes left, left becomes right
        samples[index] = (int32_t)(right_sample * config.volume_scale * 2);
        samples[index + 1] = (int32_t)(left_sample * config.volume_scale);
    } else {
        // Apply volume scaling without channel swap
        samples[index] = (int32_t)(left_sample * config.volume_scale * 2);
        samples[index + 1] = (int32_t)(right_sample * config.volume_scale);
    }
}

void volume_effect_process_batch(int32_t *samples, size_t sample_count) {
    if (!initialized || sample_count == 0) {
        return;
    }

    // Calculate scale factors (left channel gets 2x boost like original)
    const float left_scale = config.volume_scale * 2.0f;
    const float right_scale = config.volume_scale;

    // Process in chunks for SIMD efficiency
    size_t processed = 0;
    while (processed < sample_count) {
        size_t chunk_size = sample_count - processed;
        if (chunk_size > VOLUME_BATCH_SIZE) {
            chunk_size = VOLUME_BATCH_SIZE;
        }
        // Ensure chunk is even (stereo pairs)
        chunk_size = (chunk_size / 2) * 2;
        if (chunk_size == 0) break;

        // Handle channel swap if enabled
        if (config.enable_channel_swap) {
            // Swap channels first, then apply volume
            for (size_t i = 0; i < chunk_size; i += 2) {
                int32_t left = samples[processed + i];
                int32_t right = samples[processed + i + 1];
                volume_float_buffer[i] = (float)right;      // Left gets right
                volume_float_buffer[i + 1] = (float)left;   // Right gets left
            }
        } else {
            // No swap, just convert to float
            for (size_t i = 0; i < chunk_size; i++) {
                volume_float_buffer[i] = (float)samples[processed + i];
            }
        }

        // SIMD: Apply volume scaling to left channel samples (step=2)
        dsps_mulc_f32(&volume_float_buffer[0], &volume_float_buffer[0], 
                      chunk_size / 2, left_scale, 2, 2);
        
        // SIMD: Apply volume scaling to right channel samples (step=2)
        dsps_mulc_f32(&volume_float_buffer[1], &volume_float_buffer[1], 
                      chunk_size / 2, right_scale, 2, 2);

        // Convert back to int32
        for (size_t i = 0; i < chunk_size; i++) {
            samples[processed + i] = (int32_t)volume_float_buffer[i];
        }

        processed += chunk_size;
    }
}

void volume_effect_cleanup(void) {
    initialized = false;
    ESP_LOGI(TAG, "Volume effect cleaned up");
}

