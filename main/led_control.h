#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief LED GPIO pin definitions
 */
#define LED_GREEN_1_GPIO    39
#define LED_GREEN_2_GPIO    40
#define LED_YELLOW_1_GPIO   41
#define LED_YELLOW_2_GPIO   42
#define LED_RED_1_GPIO      2
#define LED_RED_2_GPIO      1

/**
 * @brief LED control configuration structure
 */
typedef struct {
    float low_threshold;      // Threshold for green LEDs (0.0 - 1.0)
    float medium_threshold;   // Threshold for yellow LEDs (0.0 - 1.0)
    float high_threshold;     // Threshold for red LEDs (0.0 - 1.0)
    float smoothing_factor;   // Smoothing factor for intensity (0.0 - 1.0, higher = more smoothing)
    bool enable_leds;         // Enable/disable LED control
} led_control_config_t;

/**
 * @brief Initialize LED control system
 * @param config LED control configuration (can be NULL for defaults)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t led_control_init(const led_control_config_t *config);

/**
 * @brief Update LEDs based on audio signal intensity
 * @param audio_samples Pointer to audio sample buffer
 * @param sample_count Number of samples in the buffer
 * @return ESP_OK on success, error code on failure
 */
esp_err_t led_control_update(const int32_t *audio_samples, size_t sample_count);

/**
 * @brief Set LED state manually (for testing)
 * @param green_on Enable green LEDs
 * @param yellow_on Enable yellow LEDs
 * @param red_on Enable red LEDs
 * @return ESP_OK on success, error code on failure
 */
esp_err_t led_control_set_manual(bool green_on, bool yellow_on, bool red_on);

/**
 * @brief Get current calculated intensity value
 * @return Current intensity (0.0 - 1.0)
 */
float led_control_get_intensity(void);

/**
 * @brief Update LED control configuration
 * @param config New configuration
 */
void led_control_update_config(const led_control_config_t *config);

/**
 * @brief Test all LEDs by turning them on sequentially
 * @param delay_ms Delay between each LED test in milliseconds
 * @return ESP_OK on success, error code on failure
 */
esp_err_t led_control_test_all(uint32_t delay_ms);

/**
 * @brief Cleanup LED control resources
 */
void led_control_cleanup(void);

#endif // LED_CONTROL_H

