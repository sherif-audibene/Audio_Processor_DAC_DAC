#include "audio_processor.h"
#include "audio_effects.h"
#include "audio_buffer_manager.h"
#include "audio_task.h"
#include "i2s_config.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "AUDIO_PROCESSOR";

// Private variables
static bool audio_initialized = false;

esp_err_t audio_processor_init(const audio_config_t *config) {
    if (audio_initialized) {
        ESP_LOGW(TAG, "Audio processor already initialized");
        return ESP_OK;
    }

    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration provided");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing audio processor...");

    // Initialize buffer manager
    esp_err_t ret = audio_buffer_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize buffer manager");
        return ret;
    }

    // Initialize audio effects
    audio_effects_config_t effects_config = {
        .volume_scale = config->volume_scale,
        .enable_channel_swap = config->enable_channel_swap,
        .enable_delay = config->enable_delay,
        .delay_time_ms = config->delay_time_ms,
        .delay_mix = config->delay_mix,
        .delay_feedback = config->delay_feedback,
        .enable_pitch_shift = config->enable_pitch_shift,
        .pitch_ratio = config->pitch_ratio
    };

    ret = audio_effects_init(&effects_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize audio effects");
        audio_buffer_manager_cleanup();
        return ret;
    }

    audio_initialized = true;
    ESP_LOGI(TAG, "Audio processor initialized successfully");
    return ESP_OK;
}

esp_err_t audio_processor_start(void) {
    if (!audio_initialized) {
        ESP_LOGE(TAG, "Audio processor not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (audio_task_is_running()) {
        ESP_LOGW(TAG, "Audio processor already running");
        return ESP_OK;
    }

    esp_err_t ret = audio_task_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start audio task");
        return ret;
    }

    ESP_LOGI(TAG, "Audio processor started successfully");
    return ESP_OK;
}

void audio_processor_stop(void) {
    audio_task_stop();
    ESP_LOGI(TAG, "Audio processor stopped");
}

void audio_processor_cleanup(void) {
    // Stop audio processing
    audio_processor_stop();
    
    // Cleanup I2S peripherals
    i2s_dac_cleanup();
    i2s_adc_cleanup();
    
    // Cleanup modules
    audio_effects_cleanup();
    audio_buffer_manager_cleanup();
    
    audio_initialized = false;
    ESP_LOGI(TAG, "Audio processor cleaned up");
}

void audio_processor_update_config(const audio_config_t *config) {
    if (!config || !audio_initialized) {
        return;
    }

    audio_effects_config_t effects_config = {
        .volume_scale = config->volume_scale,
        .enable_channel_swap = config->enable_channel_swap,
        .enable_delay = config->enable_delay,
        .delay_time_ms = config->delay_time_ms,
        .delay_mix = config->delay_mix,
        .delay_feedback = config->delay_feedback,
        .enable_pitch_shift = config->enable_pitch_shift,
        .pitch_ratio = config->pitch_ratio
    };

    audio_effects_update_config(&effects_config);
    ESP_LOGI(TAG, "Audio processor configuration updated");
}
