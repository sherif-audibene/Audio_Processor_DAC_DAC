#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Audio processing configuration structure
 */
typedef struct {
    float volume_scale;
    bool enable_debug;
    bool enable_channel_swap;
    bool enable_delay;
    float delay_time_ms;
    float delay_mix;
    float delay_feedback;
    bool enable_pitch_shift;
    float pitch_ratio;  // 1.0 = no change, 2.0 = octave up, 0.5 = octave down
} audio_config_t;

/**
 * @brief Initialize audio processing system
 * @param config Audio processing configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t audio_processor_init(const audio_config_t *config);

/**
 * @brief Start audio processing
 * @return ESP_OK on success, error code on failure
 */
esp_err_t audio_processor_start(void);

/**
 * @brief Stop audio processing
 */
void audio_processor_stop(void);

/**
 * @brief Cleanup audio processing resources
 */
void audio_processor_cleanup(void);

/**
 * @brief Update audio processing configuration
 * @param config New audio processing configuration
 */
void audio_processor_update_config(const audio_config_t *config);

#endif // AUDIO_PROCESSOR_H
