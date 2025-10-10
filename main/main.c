#include "esp_log.h"
#include "audio_processor.h"
#include "i2s_config.h"

static const char *TAG = "MAIN";




/**
 * @brief Main application entry point
 */
void app_main(void) {
    ESP_LOGI(TAG, "Starting I2S Audio Passthrough Application");

    // Configure audio processing
    audio_config_t audio_config = {
        .volume_scale = 0.5f,
        .enable_debug = true,
        .enable_channel_swap = true
    };

    // Initialize audio processor
    esp_err_t ret = audio_processor_init(&audio_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize audio processor: %s", esp_err_to_name(ret));
        return;
    }

    // Start audio processing
    ret = audio_processor_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start audio processor: %s", esp_err_to_name(ret));
        audio_processor_cleanup();
        return;
    }

    ESP_LOGI(TAG, "Application started successfully!");
}
