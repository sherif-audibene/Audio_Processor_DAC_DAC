#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Audio processing configuration
#define AUDIO_TASK_STACK_SIZE 4096
#define AUDIO_TASK_PRIORITY 5
#define AUDIO_READ_TIMEOUT_MS 1000
#define AUDIO_WRITE_TIMEOUT_MS 100
#define AUDIO_DEBUG_INTERVAL 100

/**
 * @brief Audio processing configuration structure
 */
typedef struct {
    float volume_scale;
    bool enable_debug;
    bool enable_channel_swap;
} audio_config_t;

/**
 * @brief Initialize audio processing system
 * @param config Audio processing configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t audio_processor_init(const audio_config_t *config);

/**
 * @brief Start audio processing task
 * @return ESP_OK on success, error code on failure
 */
esp_err_t audio_processor_start(void);

/**
 * @brief Stop audio processing task
 */
void audio_processor_stop(void);

/**
 * @brief Cleanup audio processing resources
 */
void audio_processor_cleanup(void);

/**
 * @brief Process audio samples (channel swap and volume scaling)
 * @param samples Pointer to audio samples buffer
 * @param sample_count Number of samples to process
 * @param config Audio processing configuration
 */
void audio_process_samples(int16_t *samples, size_t sample_count, const audio_config_t *config);

#endif // AUDIO_PROCESSOR_H
