#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"

/**
 * @brief Initialize web server
 * @return ESP_OK on success, error code on failure
 */
esp_err_t web_server_init(void);

/**
 * @brief Start web server
 * @return ESP_OK on success, error code on failure
 */
esp_err_t web_server_start(void);

/**
 * @brief Stop web server
 */
void web_server_stop(void);

/**
 * @brief Cleanup web server resources
 */
void web_server_cleanup(void);

#endif // WEB_SERVER_H

