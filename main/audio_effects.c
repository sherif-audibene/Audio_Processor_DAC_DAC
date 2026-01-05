#include "audio_effects.h"
#include "effects/delay_effect.h"
#include "effects/pitch_shift_effect.h"
#include "effects/volume_effect.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "AUDIO_EFFECTS";

// Effect state
static audio_effects_config_t current_config;
static bool effects_initialized = false;

void audio_effects_process(int32_t *samples, size_t sample_count) {
    if (!samples || sample_count == 0 || !effects_initialized) {
        return;
    }

    // Check if pitch shift is enabled (requires sample-by-sample processing)
    if (current_config.enable_pitch_shift) {
        // Fall back to sample-by-sample processing when pitch shift is active
        for (size_t i = 0; i < sample_count; i += 2) {
            if (i + 1 >= sample_count) break;
            
            int32_t left_sample = samples[i];
            int32_t right_sample = samples[i + 1];
            
            int32_t orig_left = left_sample;
            int32_t orig_right = right_sample;
            
            // Apply pitch shift (sample-by-sample due to variable-rate interpolation)
            pitch_shift_effect_process(&left_sample, &right_sample, orig_left, orig_right);
            
            // Apply delay effect
            delay_effect_process(&left_sample, &right_sample, left_sample, right_sample);
            
            // Apply volume and channel swap
            volume_effect_process(samples, i, left_sample, right_sample);
        }
    } else {
        // OPTIMIZED PATH: Use SIMD batch processing when pitch shift is disabled
        
        // Apply delay effect in batch (SIMD optimized)
        delay_effect_process_batch(samples, sample_count);
        
        // Apply volume scaling in batch (SIMD optimized)
        volume_effect_process_batch(samples, sample_count);
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

    // Store configuration
    memcpy(&current_config, config, sizeof(audio_effects_config_t));

    // Initialize delay effect
    delay_effect_config_t delay_config = {
        .enable = config->enable_delay,
        .delay_time_ms = config->delay_time_ms,
        .mix = config->delay_mix,
        .feedback = config->delay_feedback
    };
    esp_err_t ret = delay_effect_init(&delay_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize delay effect");
        return ret;
    }

    // Initialize pitch shift effect
    pitch_shift_effect_config_t pitch_config = {
        .enable = config->enable_pitch_shift,
        .pitch_ratio = config->pitch_ratio
    };
    ret = pitch_shift_effect_init(&pitch_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize pitch shift effect");
        delay_effect_cleanup();
        return ret;
    }

    // Initialize volume effect
    volume_effect_config_t volume_config = {
        .volume_scale = config->volume_scale,
        .enable_channel_swap = config->enable_channel_swap
    };
    ret = volume_effect_init(&volume_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize volume effect");
        delay_effect_cleanup();
        pitch_shift_effect_cleanup();
        return ret;
    }

    effects_initialized = true;
    ESP_LOGI(TAG, "Audio effects initialized successfully");
    return ESP_OK;
}

void audio_effects_update_config(const audio_effects_config_t *config) {
    if (!config || !effects_initialized) {
        return;
    }
    
    memcpy(&current_config, config, sizeof(audio_effects_config_t));
    
    // Update delay effect
    delay_effect_config_t delay_config = {
        .enable = config->enable_delay,
        .delay_time_ms = config->delay_time_ms,
        .mix = config->delay_mix,
        .feedback = config->delay_feedback
    };
    delay_effect_update_config(&delay_config);

    // Update pitch shift effect
    pitch_shift_effect_config_t pitch_config = {
        .enable = config->enable_pitch_shift,
        .pitch_ratio = config->pitch_ratio
    };
    pitch_shift_effect_update_config(&pitch_config);

    // Update volume effect
    volume_effect_config_t volume_config = {
        .volume_scale = config->volume_scale,
        .enable_channel_swap = config->enable_channel_swap
    };
    volume_effect_update_config(&volume_config);
}

void audio_effects_cleanup(void) {
    delay_effect_cleanup();
    pitch_shift_effect_cleanup();
    volume_effect_cleanup();
    
    effects_initialized = false;
    ESP_LOGI(TAG, "Audio effects cleaned up");
}
