#include "lcd_display.h"
#include "i2c_scanner.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "LCD_DISPLAY";

// Framebuffer: 128 columns x 8 pages (64 pixels / 8 bits per page)
static uint8_t framebuffer[LCD_WIDTH * LCD_PAGES];
static i2c_port_t i2c_port = I2C_NUM_0;
static uint8_t lcd_i2c_addr = LCD_I2C_ADDRESS;
static bool initialized = false;

/**
 * @brief Send command to ST7567S with retry logic
 */
static esp_err_t lcd_send_command(uint8_t cmd) {
    #define CMD_RETRY_COUNT 3
    
    for (int retry = 0; retry < CMD_RETRY_COUNT; retry++) {
        i2c_cmd_handle_t i2c_cmd = i2c_cmd_link_create();
        if (i2c_cmd == NULL) {
            ESP_LOGE(TAG, "Failed to create I2C command link");
            return ESP_ERR_NO_MEM;
        }
        
        i2c_master_start(i2c_cmd);
        i2c_master_write_byte(i2c_cmd, (lcd_i2c_addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(i2c_cmd, 0x00, true);  // Control byte: Co=0, D/C=0 (command)
        i2c_master_write_byte(i2c_cmd, cmd, true);   // Command byte
        i2c_master_stop(i2c_cmd);
        
        esp_err_t ret = i2c_master_cmd_begin(i2c_port, i2c_cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(i2c_cmd);
        
        if (ret == ESP_OK) {
            return ESP_OK;
        }
        
        // Only log on last retry
        if (retry == CMD_RETRY_COUNT - 1) {
            ESP_LOGE(TAG, "I2C command 0x%02X send failed after %d retries: %s", 
                     cmd, CMD_RETRY_COUNT, esp_err_to_name(ret));
        }
        
        vTaskDelay(pdMS_TO_TICKS(2));  // Wait before retry
    }
    
    return ESP_FAIL;
}

/**
 * @brief Send data to ST7567S - hybrid approach with small chunks
 */
static esp_err_t lcd_send_data(const uint8_t *data, size_t len) {
    // Send in very small chunks with control byte for each chunk
    #define CHUNK_SIZE 4  // Even smaller chunks for better reliability
    #define DATA_RETRY_COUNT 3  // More retries
    
    for (size_t offset = 0; offset < len; offset += CHUNK_SIZE) {
        size_t chunk_len = (len - offset) < CHUNK_SIZE ? (len - offset) : CHUNK_SIZE;
        esp_err_t ret = ESP_FAIL;
        
        for (int retry = 0; retry < DATA_RETRY_COUNT; retry++) {
            i2c_cmd_handle_t i2c_cmd = i2c_cmd_link_create();
            if (i2c_cmd == NULL) {
                ESP_LOGE(TAG, "Failed to create I2C command link");
                return ESP_ERR_NO_MEM;
            }
            
            i2c_master_start(i2c_cmd);
            i2c_master_write_byte(i2c_cmd, (lcd_i2c_addr << 1) | I2C_MASTER_WRITE, true);
            i2c_master_write_byte(i2c_cmd, 0x40, true);  // Data control byte
            
            // Send chunk of data bytes
            for (size_t i = 0; i < chunk_len; i++) {
                i2c_master_write_byte(i2c_cmd, data[offset + i], true);
            }
            
            i2c_master_stop(i2c_cmd);
            
            ret = i2c_master_cmd_begin(i2c_port, i2c_cmd, pdMS_TO_TICKS(150));  // Longer timeout
            i2c_cmd_link_delete(i2c_cmd);
            
            if (ret == ESP_OK) {
                break;
            }
            
            if (retry < DATA_RETRY_COUNT - 1) {
                vTaskDelay(pdMS_TO_TICKS(3));  // Longer delay between retries
            }
        }
        
        if (ret != ESP_OK) {
            // Only log errors occasionally to avoid spam
            static uint32_t error_count = 0;
            if (error_count++ % 10 == 0) {
                ESP_LOGE(TAG, "I2C data send failed at offset %d (chunk %d bytes) after %d retries: %s", 
                         offset, chunk_len, DATA_RETRY_COUNT, esp_err_to_name(ret));
            }
            return ret;
        }
        
        // Small delay between chunks (don't delay after last chunk)
        if (offset + CHUNK_SIZE < len) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    
    return ESP_OK;
}

esp_err_t lcd_display_init(const lcd_config_t *config) {
    if (initialized) {
        ESP_LOGW(TAG, "LCD already initialized");
        return ESP_OK;
    }
    
    if (config == NULL) {
        ESP_LOGE(TAG, "Config is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Configure I2C
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = config->sda_pin,
        .scl_io_num = config->scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = config->i2c_freq_hz,
    };
    
    esp_err_t ret = i2c_param_config(i2c_port, &i2c_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = i2c_driver_install(i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    lcd_i2c_addr = config->i2c_address;
    
    // Wait for display to power up
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Initialize ST7567S
    lcd_send_command(ST7567_CMD_INTERNAL_RESET);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    lcd_send_command(ST7567_CMD_SET_BIAS_7);  // Set LCD bias to 1/7
    
    // Set ADC and COM direction based on flip settings
    if (config->flip_horizontal) {
        lcd_send_command(ST7567_CMD_SET_ADC_REVERSE);
    } else {
        lcd_send_command(ST7567_CMD_SET_ADC_NORMAL);
    }
    
    if (config->flip_vertical) {
        lcd_send_command(ST7567_CMD_COM_SCAN_INC);
    } else {
        lcd_send_command(ST7567_CMD_COM_SCAN_DEC);
    }
    
    // Power control
    lcd_send_command(ST7567_CMD_POWER_CONTROL | 0x04);  // Booster circuit ON
    vTaskDelay(pdMS_TO_TICKS(2));
    lcd_send_command(ST7567_CMD_POWER_CONTROL | 0x06);  // Voltage regulator ON
    vTaskDelay(pdMS_TO_TICKS(2));
    lcd_send_command(ST7567_CMD_POWER_CONTROL | 0x07);  // Voltage follower ON
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Set regulation ratio
    lcd_send_command(ST7567_CMD_REGULATION_RATIO | 0x05);
    
    // Set contrast
    lcd_send_command(ST7567_CMD_SET_EV);
    lcd_send_command(config->contrast & 0x3F);
    
    lcd_send_command(ST7567_CMD_DISPLAY_NORMAL);  // Normal display mode
    lcd_send_command(ST7567_CMD_SET_START_LINE | 0);  // Start line 0
    lcd_send_command(ST7567_CMD_DISPLAY_ON);  // Display ON
    
    // Clear framebuffer
    memset(framebuffer, 0, sizeof(framebuffer));
    lcd_display_clear();
    
    initialized = true;
    ESP_LOGI(TAG, "LCD display initialized successfully");
    
    return ESP_OK;
}

esp_err_t lcd_display_clear(void) {
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    memset(framebuffer, 0, sizeof(framebuffer));
    return lcd_display_update();
}

esp_err_t lcd_display_update(void) {
    if (!initialized) {
        ESP_LOGE(TAG, "Display not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret;
    
    // Reset start line to 0 every update to prevent vertical drift
    // This fixes the issue where I2C noise can corrupt the start line register
    ret = lcd_send_command(ST7567_CMD_SET_START_LINE | 0);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to reset start line");
    }
    
    // Write framebuffer to display page by page
    for (uint8_t page = 0; page < LCD_PAGES; page++) {
        // Set page address
        ret = lcd_send_command(ST7567_CMD_SET_PAGE | page);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set page %d", page);
            return ret;
        }
        
        // Set column address to 0
        ret = lcd_send_command(ST7567_CMD_SET_COLUMN_UPPER | 0);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set column upper");
            return ret;
        }
        
        ret = lcd_send_command(ST7567_CMD_SET_COLUMN_LOWER | 0);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set column lower");
            return ret;
        }
        
        // Send page data (will be sent in chunks internally)
        ret = lcd_send_data(&framebuffer[page * LCD_WIDTH], LCD_WIDTH);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send data for page %d", page);
            return ret;
        }
    }
    
    return ESP_OK;
}

void lcd_display_set_pixel(uint8_t x, uint8_t y, uint8_t color) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        return;
    }
    
    uint8_t page = y / 8;
    uint8_t bit = y % 8;
    uint16_t index = page * LCD_WIDTH + x;
    
    if (color) {
        framebuffer[index] |= (1 << bit);
    } else {
        framebuffer[index] &= ~(1 << bit);
    }
}

void lcd_display_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color) {
    // Bresenham's line algorithm
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        lcd_display_set_pixel(x0, y0, color);
        
        if (x0 == x1 && y0 == y1) {
            break;
        }
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void lcd_display_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color) {
    // Top and bottom lines
    for (uint8_t i = 0; i < w; i++) {
        lcd_display_set_pixel(x + i, y, color);
        lcd_display_set_pixel(x + i, y + h - 1, color);
    }
    
    // Left and right lines
    for (uint8_t i = 0; i < h; i++) {
        lcd_display_set_pixel(x, y + i, color);
        lcd_display_set_pixel(x + w - 1, y + i, color);
    }
}

void lcd_display_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color) {
    for (uint8_t i = 0; i < w; i++) {
        for (uint8_t j = 0; j < h; j++) {
            lcd_display_set_pixel(x + i, y + j, color);
        }
    }
}

esp_err_t lcd_display_set_contrast(uint8_t contrast) {
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = lcd_send_command(ST7567_CMD_SET_EV);
    if (ret == ESP_OK) {
        ret = lcd_send_command(contrast & 0x3F);
    }
    
    return ret;
}

esp_err_t lcd_display_power_on(void) {
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return lcd_send_command(ST7567_CMD_DISPLAY_ON);
}

esp_err_t lcd_display_power_off(void) {
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return lcd_send_command(ST7567_CMD_DISPLAY_OFF);
}

uint8_t* lcd_display_get_framebuffer(void) {
    return framebuffer;
}

void lcd_display_draw_letter_s(uint8_t x, uint8_t y, uint8_t size) {
    // Draw a large letter 'S' using curves and lines
    // S shape: top arc, middle section, bottom arc
    
    uint8_t thickness = size / 10;
    if (thickness < 1) thickness = 1;
    
    // Top horizontal line
    lcd_display_fill_rect(x, y, size, thickness * 2, 1);
    
    // Top-left vertical segment
    lcd_display_fill_rect(x, y, thickness * 2, size / 3, 1);
    
    // Middle horizontal line
    lcd_display_fill_rect(x, y + size / 3, size, thickness * 2, 1);
    
    // Bottom-right vertical segment
    lcd_display_fill_rect(x + size - thickness * 2, y + size / 3, thickness * 2, size / 3, 1);
    
    // Bottom horizontal line
    lcd_display_fill_rect(x, y + size - thickness * 2, size, thickness * 2, 1);
    
    // Bottom-left vertical segment
    lcd_display_fill_rect(x, y + size * 2 / 3, thickness * 2, size / 3, 1);
}

esp_err_t lcd_display_init_with_detection(lcd_config_t *config) {
    if (config == NULL) {
        ESP_LOGE(TAG, "Config is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Common ST7567S I2C addresses to try
    uint8_t addresses[] = {0x3F, 0x3C, 0x3D};
    
    ESP_LOGI(TAG, "Attempting auto-detection of LCD I2C address...");
    
    for (int i = 0; i < sizeof(addresses); i++) {
        config->i2c_address = addresses[i];
        ESP_LOGI(TAG, "Trying address 0x%02X...", addresses[i]);
        
        esp_err_t ret = lcd_display_init(config);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "LCD successfully initialized at address 0x%02X", addresses[i]);
            return ESP_OK;
        }
        
        // Cleanup failed attempt
        if (initialized) {
            lcd_display_cleanup();
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGE(TAG, "Failed to detect LCD at any known address");
    return ESP_FAIL;
}

void lcd_display_cleanup(void) {
    if (initialized) {
        lcd_display_power_off();
        i2c_driver_delete(i2c_port);
        initialized = false;
        ESP_LOGI(TAG, "LCD display cleaned up");
    }
}

