#include "volume_effect.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "VOLUME_EFFECT";

// Effect state
static volume_effect_config_t config;
static bool initialized = false;

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

void volume_effect_cleanup(void) {
    initialized = false;
    ESP_LOGI(TAG, "Volume effect cleaned up");
}

