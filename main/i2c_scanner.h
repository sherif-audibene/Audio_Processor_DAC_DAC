#ifndef I2C_SCANNER_H
#define I2C_SCANNER_H

#include "esp_err.h"
#include <stdint.h>

/**
 * @brief Scan I2C bus for devices
 * 
 * @param i2c_port I2C port number
 * @param found_addr Pointer to store found address (if any)
 * @return ESP_OK if device found, ESP_FAIL otherwise
 */
esp_err_t i2c_scan_bus(int i2c_port, uint8_t *found_addr);

/**
 * @brief Print all devices found on I2C bus
 * 
 * @param i2c_port I2C port number
 */
void i2c_print_devices(int i2c_port);

#endif // I2C_SCANNER_H

