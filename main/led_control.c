#include "led_control.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static const char *TAG = "LED_CONTROL";

// Private variables
static bool led_initialized = false;
static led_control_config_t led_config;
static float smoothed_intensity = 0.0f;

// Default configuration
#define DEFAULT_LOW_THRESHOLD      0.1f    // 10% for green
#define DEFAULT_MEDIUM_THRESHOLD   0.4f    // 40% for yellow
#define DEFAULT_HIGH_THRESHOLD     0.7f    // 70% for red
#define DEFAULT_SMOOTHING_FACTOR   0.3f    // 30% smoothing (lower = faster response)

/**
 * @brief Calculate RMS (Root Mean Square) intensity from audio samples
 */
static float calculate_rms_intensity(const int32_t *samples, size_t count) {
    if (samples == NULL || count == 0) {
        return 0.0f;
    }

    int64_t sum_squares = 0;
    int32_t peak = 0;
    
    for (size_t i = 0; i < count; i++) {
        int32_t sample = samples[i];
        // Use absolute value for peak detection
        int32_t abs_sample = (sample < 0) ? -sample : sample;
        if (abs_sample > peak) {
            peak = abs_sample;
        }
        sum_squares += (int64_t)sample * (int64_t)sample;
    }

    // Calculate RMS
    float rms = sqrtf((float)sum_squares / (float)count);
    
    // Normalize using both RMS and peak for better sensitivity
    // For 24-bit audio: max value is 8388607 (2^23 - 1)
    // For 32-bit signed: max value is 2147483647 (2^31 - 1)
    // Use the larger range to be safe
    float normalized_rms = rms / 2147483647.0f;
    float normalized_peak = (float)peak / 2147483647.0f;
    
    // Use the larger of RMS or peak (scaled) for more responsive LEDs
    float normalized = (normalized_rms > normalized_peak * 0.5f) ? normalized_rms : (normalized_peak * 0.5f);
    
    // Apply logarithmic scaling for better visual response
    if (normalized > 0.0f) {
        normalized = log10f(1.0f + normalized * 9.0f);  // Scale 0-1 to log10(1) to log10(10)
    }
    
    // Clamp to 0.0 - 1.0
    if (normalized > 1.0f) {
        normalized = 1.0f;
    }
    if (normalized < 0.0f) {
        normalized = 0.0f;
    }

    return normalized;
}

/**
 * @brief Configure GPIO pins for LED control
 */
static esp_err_t configure_led_gpios(void) {
    // Configure each GPIO individually to catch any pin-specific issues
    gpio_num_t led_pins[] = {
        LED_GREEN_1_GPIO, LED_GREEN_2_GPIO,
        LED_YELLOW_1_GPIO, LED_YELLOW_2_GPIO,
        LED_RED_1_GPIO, LED_RED_2_GPIO
    };
    
    for (int i = 0; i < 6; i++) {
        gpio_reset_pin(led_pins[i]);
        
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << led_pins[i]),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };

        esp_err_t ret = gpio_config(&io_conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure GPIO%d: %s", led_pins[i], esp_err_to_name(ret));
            return ret;
        }
        
        // Set initial state to OFF
        gpio_set_level(led_pins[i], 0);
        ESP_LOGI(TAG, "GPIO%d configured as LED output", led_pins[i]);
    }

    ESP_LOGI(TAG, "All LED GPIOs configured successfully");
    return ESP_OK;
}

esp_err_t led_control_init(const led_control_config_t *config) {
    if (led_initialized) {
        ESP_LOGW(TAG, "LED control already initialized");
        return ESP_OK;
    }

    // Set default configuration
    if (config != NULL) {
        memcpy(&led_config, config, sizeof(led_control_config_t));
    } else {
        led_config.low_threshold = DEFAULT_LOW_THRESHOLD;
        led_config.medium_threshold = DEFAULT_MEDIUM_THRESHOLD;
        led_config.high_threshold = DEFAULT_HIGH_THRESHOLD;
        led_config.smoothing_factor = DEFAULT_SMOOTHING_FACTOR;
        led_config.enable_leds = true;
    }

    // Validate thresholds
    if (led_config.low_threshold >= led_config.medium_threshold ||
        led_config.medium_threshold >= led_config.high_threshold) {
        ESP_LOGE(TAG, "Invalid threshold configuration");
        return ESP_ERR_INVALID_ARG;
    }

    // Configure GPIO pins
    esp_err_t ret = configure_led_gpios();
    if (ret != ESP_OK) {
        return ret;
    }

    smoothed_intensity = 0.0f;
    led_initialized = true;

    ESP_LOGI(TAG, "LED control initialized");
    ESP_LOGI(TAG, "  - Green threshold: %.1f%%", led_config.low_threshold * 100.0f);
    ESP_LOGI(TAG, "  - Yellow threshold: %.1f%%", led_config.medium_threshold * 100.0f);
    ESP_LOGI(TAG, "  - Red threshold: %.1f%%", led_config.high_threshold * 100.0f);
    ESP_LOGI(TAG, "  - Smoothing factor: %.1f%%", led_config.smoothing_factor * 100.0f);

    return ESP_OK;
}

esp_err_t led_control_update(const int32_t *audio_samples, size_t sample_count) {
    if (!led_initialized || !led_config.enable_leds) {
        return ESP_OK;
    }

    if (audio_samples == NULL || sample_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Calculate current intensity
    float current_intensity = calculate_rms_intensity(audio_samples, sample_count);

    // Apply smoothing (exponential moving average)
    smoothed_intensity = (led_config.smoothing_factor * smoothed_intensity) + 
                        ((1.0f - led_config.smoothing_factor) * current_intensity);

    // Update LEDs based on smoothed intensity
    bool green_on = smoothed_intensity >= led_config.low_threshold;
    bool yellow_on = smoothed_intensity >= led_config.medium_threshold;
    bool red_on = smoothed_intensity >= led_config.high_threshold;

    // Set green LEDs
    gpio_set_level(LED_GREEN_1_GPIO, green_on ? 1 : 0);
    gpio_set_level(LED_GREEN_2_GPIO, green_on ? 1 : 0);

    // Set yellow LEDs
    gpio_set_level(LED_YELLOW_1_GPIO, yellow_on ? 1 : 0);
    gpio_set_level(LED_YELLOW_2_GPIO, yellow_on ? 1 : 0);

    // Set red LEDs
    gpio_set_level(LED_RED_1_GPIO, red_on ? 1 : 0);
    gpio_set_level(LED_RED_2_GPIO, red_on ? 1 : 0);

    return ESP_OK;
}

esp_err_t led_control_set_manual(bool green_on, bool yellow_on, bool red_on) {
    if (!led_initialized) {
        ESP_LOGE(TAG, "LED control not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    gpio_set_level(LED_GREEN_1_GPIO, green_on ? 1 : 0);
    gpio_set_level(LED_GREEN_2_GPIO, green_on ? 1 : 0);
    gpio_set_level(LED_YELLOW_1_GPIO, yellow_on ? 1 : 0);
    gpio_set_level(LED_YELLOW_2_GPIO, yellow_on ? 1 : 0);
    gpio_set_level(LED_RED_1_GPIO, red_on ? 1 : 0);
    gpio_set_level(LED_RED_2_GPIO, red_on ? 1 : 0);

    return ESP_OK;
}

float led_control_get_intensity(void) {
    return smoothed_intensity;
}

void led_control_update_config(const led_control_config_t *config) {
    if (config == NULL || !led_initialized) {
        return;
    }

    // Validate thresholds
    if (config->low_threshold >= config->medium_threshold ||
        config->medium_threshold >= config->high_threshold) {
        ESP_LOGW(TAG, "Invalid threshold configuration, ignoring update");
        return;
    }

    memcpy(&led_config, config, sizeof(led_control_config_t));
    ESP_LOGI(TAG, "LED control configuration updated");
}

esp_err_t led_control_test_all(uint32_t delay_ms) {
    if (!led_initialized) {
        ESP_LOGE(TAG, "LED control not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting LED test sequence...");
    
    // Test green LEDs
    ESP_LOGI(TAG, "Testing green LEDs (GPIO%d, GPIO%d)...", LED_GREEN_1_GPIO, LED_GREEN_2_GPIO);
    gpio_set_level(LED_GREEN_1_GPIO, 1);
    gpio_set_level(LED_GREEN_2_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    gpio_set_level(LED_GREEN_1_GPIO, 0);
    gpio_set_level(LED_GREEN_2_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Test yellow LEDs
    ESP_LOGI(TAG, "Testing yellow LEDs (GPIO%d, GPIO%d)...", LED_YELLOW_1_GPIO, LED_YELLOW_2_GPIO);
    gpio_set_level(LED_YELLOW_1_GPIO, 1);
    gpio_set_level(LED_YELLOW_2_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    gpio_set_level(LED_YELLOW_1_GPIO, 0);
    gpio_set_level(LED_YELLOW_2_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Test red LEDs
    ESP_LOGI(TAG, "Testing red LEDs (GPIO%d, GPIO%d)...", LED_RED_1_GPIO, LED_RED_2_GPIO);
    gpio_set_level(LED_RED_1_GPIO, 1);
    gpio_set_level(LED_RED_2_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    gpio_set_level(LED_RED_1_GPIO, 0);
    gpio_set_level(LED_RED_2_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Test all LEDs together
    ESP_LOGI(TAG, "Testing all LEDs together...");
    gpio_set_level(LED_GREEN_1_GPIO, 1);
    gpio_set_level(LED_GREEN_2_GPIO, 1);
    gpio_set_level(LED_YELLOW_1_GPIO, 1);
    gpio_set_level(LED_YELLOW_2_GPIO, 1);
    gpio_set_level(LED_RED_1_GPIO, 1);
    gpio_set_level(LED_RED_2_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    
    // Turn all off
    gpio_set_level(LED_GREEN_1_GPIO, 0);
    gpio_set_level(LED_GREEN_2_GPIO, 0);
    gpio_set_level(LED_YELLOW_1_GPIO, 0);
    gpio_set_level(LED_YELLOW_2_GPIO, 0);
    gpio_set_level(LED_RED_1_GPIO, 0);
    gpio_set_level(LED_RED_2_GPIO, 0);
    
    ESP_LOGI(TAG, "LED test sequence completed");
    return ESP_OK;
}

void led_control_cleanup(void) {
    if (!led_initialized) {
        return;
    }

    // Turn off all LEDs
    led_control_set_manual(false, false, false);

    led_initialized = false;
    ESP_LOGI(TAG, "LED control cleaned up");
}

