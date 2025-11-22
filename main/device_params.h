#ifndef DEVICE_PARAMS_H
#define DEVICE_PARAMS_H

#include "esp_err.h"
#include "audio_processor.h"
#include "lcd_task.h"
#include "led_control.h"
#include <stdbool.h>

/**
 * @brief Complete device parameters structure
 */
typedef struct {
    // Audio parameters
    audio_config_t audio;
    
    // LCD parameters
    lcd_task_config_t lcd;
    
    // LED parameters
    led_control_config_t led;
} device_params_t;

/**
 * @brief Initialize device parameters with defaults
 * @return ESP_OK on success, error code on failure
 */
esp_err_t device_params_init(void);

/**
 * @brief Get current device parameters
 * @param params Pointer to store parameters
 * @return ESP_OK on success, error code on failure
 */
esp_err_t device_params_get(device_params_t *params);

/**
 * @brief Update device parameters
 * @param params New parameters
 * @return ESP_OK on success, error code on failure
 */
esp_err_t device_params_update(const device_params_t *params);

/**
 * @brief Update audio parameters only
 * @param audio_config Audio configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t device_params_update_audio(const audio_config_t *audio_config);

/**
 * @brief Update LCD parameters only
 * @param lcd_config LCD configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t device_params_update_lcd(const lcd_task_config_t *lcd_config);

/**
 * @brief Update LED parameters only
 * @param led_config LED configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t device_params_update_led(const led_control_config_t *led_config);

/**
 * @brief Get default parameters
 * @param params Pointer to store default parameters
 */
void device_params_get_defaults(device_params_t *params);

#endif // DEVICE_PARAMS_H

