#ifndef AUDIO_TASK_H
#define AUDIO_TASK_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Audio task configuration
#define AUDIO_TASK_STACK_SIZE 4096
#define AUDIO_TASK_PRIORITY 5
#define AUDIO_READ_TIMEOUT_MS 1000
#define AUDIO_WRITE_TIMEOUT_MS 100

/**
 * @brief Start audio processing task
 * @return ESP_OK on success, error code on failure
 */
esp_err_t audio_task_start(void);

/**
 * @brief Stop audio processing task
 */
void audio_task_stop(void);

/**
 * @brief Check if audio task is running
 * @return true if running, false otherwise
 */
bool audio_task_is_running(void);

#endif // AUDIO_TASK_H

