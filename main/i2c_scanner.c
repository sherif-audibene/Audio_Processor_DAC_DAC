#include "i2c_scanner.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "I2C_SCANNER";

esp_err_t i2c_scan_bus(int i2c_port, uint8_t *found_addr) {
    esp_err_t ret = ESP_FAIL;
    
    ESP_LOGI(TAG, "Scanning I2C bus on port %d...", i2c_port);
    
    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        
        esp_err_t result = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        
        if (result == ESP_OK) {
            ESP_LOGI(TAG, "  Device found at address 0x%02X", addr);
            if (found_addr != NULL && ret != ESP_OK) {
                *found_addr = addr;
                ret = ESP_OK;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No I2C devices found!");
    }
    
    return ret;
}

void i2c_print_devices(int i2c_port) {
    uint8_t found_addr;
    i2c_scan_bus(i2c_port, &found_addr);
}

