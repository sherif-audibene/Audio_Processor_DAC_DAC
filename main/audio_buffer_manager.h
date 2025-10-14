#ifndef AUDIO_BUFFER_MANAGER_H
#define AUDIO_BUFFER_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "i2s_config.h"

/**
 * @brief Initialize audio buffer manager
 * @return ESP_OK on success, error code on failure
 */
esp_err_t audio_buffer_manager_init(void);

/**
 * @brief Get pointer to the audio buffer
 * @return Pointer to audio buffer, NULL if not initialized
 */
int32_t* audio_buffer_manager_get_buffer(void);

/**
 * @brief Get the size of the audio buffer in bytes
 * @return Buffer size in bytes
 */
size_t audio_buffer_manager_get_buffer_size(void);

/**
 * @brief Cleanup and free audio buffer
 */
void audio_buffer_manager_cleanup(void);

#endif // AUDIO_BUFFER_MANAGER_H

