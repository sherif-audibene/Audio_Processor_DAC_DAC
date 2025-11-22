#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * @brief WiFi manager configuration structure
 */
typedef struct {
    const char *ssid;
    const char *password;
    uint32_t timeout_ms;  // Connection timeout in milliseconds
} wifi_manager_config_t;

/**
 * @brief Initialize WiFi manager
 * @param config WiFi configuration (can be NULL for defaults)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t wifi_manager_init(const wifi_manager_config_t *config);

/**
 * @brief Start WiFi connection
 * @return ESP_OK on success, error code on failure
 */
esp_err_t wifi_manager_start(void);

/**
 * @brief Stop WiFi connection
 */
void wifi_manager_stop(void);

/**
 * @brief Check if WiFi is connected
 * @return true if connected, false otherwise
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Get current IP address
 * @param ip_str Buffer to store IP address string (must be at least 16 bytes)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t wifi_manager_get_ip(char *ip_str, size_t len);

/**
 * @brief Cleanup WiFi resources
 */
void wifi_manager_cleanup(void);

#endif // WIFI_MANAGER_H

