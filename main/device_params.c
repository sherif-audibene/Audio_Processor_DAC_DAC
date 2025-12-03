#include "device_params.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "DEVICE_PARAMS";

// Private variables
static device_params_t current_params;
static bool params_initialized = false;

void device_params_get_defaults(device_params_t *params) {
    if (!params) return;
    
    // Default audio configuration
    params->audio.volume_scale = 1.5f;
    params->audio.enable_debug = false;
    params->audio.enable_channel_swap = true;
    params->audio.enable_delay = true;
    params->audio.delay_time_ms = 50.0f;
    params->audio.delay_mix = 0.9f;
    params->audio.delay_feedback = 0.4f;
    params->audio.enable_pitch_shift = false;
    params->audio.pitch_ratio = 1.0f;  // 1.0 = no pitch change
    
    // Default LCD configuration
    params->lcd.sda_pin = 6;
    params->lcd.scl_pin = 7;
    params->lcd.lcd_contrast = 20;
    params->lcd.waveform.samples_per_screen = 256;
    params->lcd.waveform.time_scale = 1;
    params->lcd.waveform.amplitude_scale = 100;
    params->lcd.waveform.show_grid = true;
    params->lcd.waveform.show_center_line = true;
    params->lcd.waveform.mode = WAVEFORM_MODE_OSCILLOSCOPE;
    
    // Default LED configuration
    params->led.low_threshold = 0.15f;
    params->led.medium_threshold = 0.35f;
    params->led.high_threshold = 0.65f;
    params->led.smoothing_factor = 0.2f;
    params->led.enable_leds = true;
}

esp_err_t device_params_init(void) {
    if (params_initialized) {
        ESP_LOGW(TAG, "Device parameters already initialized");
        return ESP_OK;
    }
    
    device_params_get_defaults(&current_params);
    params_initialized = true;
    
    ESP_LOGI(TAG, "Device parameters initialized with defaults");
    return ESP_OK;
}

esp_err_t device_params_get(device_params_t *params) {
    if (!params) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!params_initialized) {
        ESP_LOGE(TAG, "Device parameters not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    memcpy(params, &current_params, sizeof(device_params_t));
    return ESP_OK;
}

esp_err_t device_params_update(const device_params_t *params) {
    if (!params) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!params_initialized) {
        ESP_LOGE(TAG, "Device parameters not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Update all parameters
    memcpy(&current_params, params, sizeof(device_params_t));
    
    // Apply changes to running systems
    device_params_update_audio(&current_params.audio);
    device_params_update_lcd(&current_params.lcd);
    device_params_update_led(&current_params.led);
    
    ESP_LOGI(TAG, "Device parameters updated");
    return ESP_OK;
}

esp_err_t device_params_update_audio(const audio_config_t *audio_config) {
    if (!audio_config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!params_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    current_params.audio = *audio_config;
    audio_processor_update_config(audio_config);
    
    ESP_LOGI(TAG, "Audio parameters updated");
    return ESP_OK;
}

esp_err_t device_params_update_lcd(const lcd_task_config_t *lcd_config) {
    if (!lcd_config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!params_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    current_params.lcd = *lcd_config;
    
    // Update LCD contrast if task is running
    if (lcd_task_is_running()) {
        lcd_task_set_contrast(lcd_config->lcd_contrast);
        lcd_task_set_waveform_config(&lcd_config->waveform);
    }
    
    ESP_LOGI(TAG, "LCD parameters updated");
    return ESP_OK;
}

esp_err_t device_params_update_led(const led_control_config_t *led_config) {
    if (!led_config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!params_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    current_params.led = *led_config;
    led_control_update_config(led_config);
    
    ESP_LOGI(TAG, "LED parameters updated");
    return ESP_OK;
}

