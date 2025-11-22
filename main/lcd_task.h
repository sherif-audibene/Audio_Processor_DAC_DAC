#ifndef LCD_TASK_H
#define LCD_TASK_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// LCD task configuration
#define LCD_TASK_STACK_SIZE 4096
#define LCD_TASK_PRIORITY 3
#define LCD_UPDATE_RATE_HZ 15  // Display refresh rate (reduced from 30 for I2C reliability)

/**
 * @brief Waveform display mode
 */
typedef enum {
    WAVEFORM_MODE_OSCILLOSCOPE,  // Traditional oscilloscope view
    WAVEFORM_MODE_SPECTRUM,      // Spectrum analyzer (future implementation)
    WAVEFORM_MODE_VU_METER       // VU meter (future implementation)
} waveform_mode_t;

/**
 * @brief Waveform resolution configuration
 */
typedef struct {
    uint16_t samples_per_screen;  // Number of audio samples to display (resolution)
    uint8_t time_scale;           // Time scale multiplier (1-10)
    uint8_t amplitude_scale;      // Amplitude scale percentage (10-200)
    bool show_grid;               // Show grid lines
    bool show_center_line;        // Show center reference line
    waveform_mode_t mode;         // Display mode
} waveform_config_t;

/**
 * @brief LCD task configuration structure
 */
typedef struct {
    int sda_pin;              // I2C SDA pin
    int scl_pin;              // I2C SCL pin
    uint8_t lcd_contrast;     // LCD contrast (0-63)
    waveform_config_t waveform;  // Waveform display configuration
} lcd_task_config_t;

/**
 * @brief Initialize and start LCD display task
 * 
 * @param config LCD task configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t lcd_task_start(const lcd_task_config_t *config);

/**
 * @brief Stop LCD display task
 */
void lcd_task_stop(void);

/**
 * @brief Check if LCD task is running
 * 
 * @return true if running, false otherwise
 */
bool lcd_task_is_running(void);

/**
 * @brief Update waveform configuration
 * 
 * @param config New waveform configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t lcd_task_set_waveform_config(const waveform_config_t *config);

/**
 * @brief Get current waveform configuration
 * 
 * @param config Pointer to store current configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t lcd_task_get_waveform_config(waveform_config_t *config);

/**
 * @brief Get pointer to audio buffer for direct access
 * LCD task will read from this buffer directly
 * 
 * @param buffer Pointer to store audio buffer address
 * @param size Pointer to store buffer size
 * @return ESP_OK on success, error code on failure
 */
esp_err_t lcd_task_get_audio_source(int32_t **buffer, size_t *size);

/**
 * @brief Set LCD contrast
 * 
 * @param contrast Contrast value (0-63)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t lcd_task_set_contrast(uint8_t contrast);

/**
 * @brief Display a temporary message on the LCD
 * Message will be displayed for a few seconds as an overlay
 * 
 * @param message Message text to display (max 20 characters)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t lcd_task_show_message(const char *message);

#endif // LCD_TASK_H

