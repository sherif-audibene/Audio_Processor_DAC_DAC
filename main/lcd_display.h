#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// LCD specifications
#define LCD_WIDTH 128
#define LCD_HEIGHT 64
#define LCD_PAGES 8  // 64 pixels / 8 bits per page

// I2C Configuration
#define LCD_I2C_ADDRESS 0x3F  // Common ST7567S I2C address (may need adjustment)
#define LCD_I2C_FREQ_HZ 100000  // 100kHz (reduced for reliability)

// ST7567S Commands
#define ST7567_CMD_DISPLAY_OFF          0xAE
#define ST7567_CMD_DISPLAY_ON           0xAF
#define ST7567_CMD_SET_START_LINE       0x40
#define ST7567_CMD_SET_PAGE             0xB0
#define ST7567_CMD_SET_COLUMN_UPPER     0x10
#define ST7567_CMD_SET_COLUMN_LOWER     0x00
#define ST7567_CMD_SET_ADC_NORMAL       0xA0
#define ST7567_CMD_SET_ADC_REVERSE      0xA1
#define ST7567_CMD_DISPLAY_NORMAL       0xA6
#define ST7567_CMD_DISPLAY_REVERSE      0xA7
#define ST7567_CMD_SET_BIAS_9           0xA2
#define ST7567_CMD_SET_BIAS_7           0xA3
#define ST7567_CMD_RMW                  0xE0
#define ST7567_CMD_RMW_CLEAR            0xEE
#define ST7567_CMD_INTERNAL_RESET       0xE2
#define ST7567_CMD_COM_SCAN_INC         0xC0
#define ST7567_CMD_COM_SCAN_DEC         0xC8
#define ST7567_CMD_POWER_CONTROL        0x28
#define ST7567_CMD_REGULATION_RATIO     0x20
#define ST7567_CMD_SET_EV               0x81
#define ST7567_CMD_SET_BOOSTER          0xF8
#define ST7567_CMD_NOP                  0xE3

/**
 * @brief LCD display configuration structure
 */
typedef struct {
    int sda_pin;           // I2C SDA pin
    int scl_pin;           // I2C SCL pin
    uint8_t i2c_address;   // I2C device address
    uint32_t i2c_freq_hz;  // I2C frequency
    uint8_t contrast;      // Display contrast (0-63)
    bool flip_horizontal;  // Flip display horizontally
    bool flip_vertical;    // Flip display vertically
} lcd_config_t;

/**
 * @brief Initialize LCD display with I2C
 * 
 * @param config LCD configuration structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t lcd_display_init(const lcd_config_t *config);

/**
 * @brief Try to initialize LCD with auto-detected I2C address
 * 
 * @param config LCD configuration structure (address will be modified if detection succeeds)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t lcd_display_init_with_detection(lcd_config_t *config);

/**
 * @brief Clear the LCD display
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t lcd_display_clear(void);

/**
 * @brief Update the display with framebuffer content
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t lcd_display_update(void);

/**
 * @brief Set a pixel in the framebuffer
 * 
 * @param x X coordinate (0-127)
 * @param y Y coordinate (0-63)
 * @param color 1 for white, 0 for black
 */
void lcd_display_set_pixel(uint8_t x, uint8_t y, uint8_t color);

/**
 * @brief Draw a line in the framebuffer
 * 
 * @param x0 Start X coordinate
 * @param y0 Start Y coordinate
 * @param x1 End X coordinate
 * @param y1 End Y coordinate
 * @param color 1 for white, 0 for black
 */
void lcd_display_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);

/**
 * @brief Draw a rectangle in the framebuffer
 * 
 * @param x X coordinate of top-left corner
 * @param y Y coordinate of top-left corner
 * @param w Width
 * @param h Height
 * @param color 1 for white, 0 for black
 */
void lcd_display_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);

/**
 * @brief Fill a rectangle in the framebuffer
 * 
 * @param x X coordinate of top-left corner
 * @param y Y coordinate of top-left corner
 * @param w Width
 * @param h Height
 * @param color 1 for white, 0 for black
 */
void lcd_display_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);

/**
 * @brief Set display contrast
 * 
 * @param contrast Contrast value (0-63)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t lcd_display_set_contrast(uint8_t contrast);

/**
 * @brief Power on the display
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t lcd_display_power_on(void);

/**
 * @brief Power off the display
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t lcd_display_power_off(void);

/**
 * @brief Get pointer to framebuffer for direct manipulation
 * 
 * @return Pointer to framebuffer (128 * 8 bytes)
 */
uint8_t* lcd_display_get_framebuffer(void);

/**
 * @brief Draw letter 'S' on the display
 * 
 * @param x X coordinate of top-left corner
 * @param y Y coordinate of top-left corner
 * @param size Size/scale of the letter
 */
void lcd_display_draw_letter_s(uint8_t x, uint8_t y, uint8_t size);

/**
 * @brief Cleanup and deinitialize LCD display
 */
void lcd_display_cleanup(void);

#endif // LCD_DISPLAY_H

