#ifndef AUDIO_BUFFER_MANAGER_H
#define AUDIO_BUFFER_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "i2s_config.h"

// Double buffer indices
#define BUFFER_A 0
#define BUFFER_B 1

/**
 * @brief Initialize audio buffer manager (with double buffering)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t audio_buffer_manager_init(void);

/**
 * @brief Get pointer to the audio buffer (legacy - returns buffer A)
 * @return Pointer to audio buffer, NULL if not initialized
 */
int32_t* audio_buffer_manager_get_buffer(void);

/**
 * @brief Get pointer to a specific buffer (A or B)
 * @param index BUFFER_A or BUFFER_B
 * @return Pointer to audio buffer, NULL if not initialized
 */
int32_t* audio_buffer_manager_get_buffer_by_index(int index);

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

