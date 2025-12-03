#ifndef AUDIO_EFFECTS_H
#define AUDIO_EFFECTS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Audio effects configuration structure
 */
typedef struct {
    float volume_scale;
    bool enable_channel_swap;
    bool enable_delay;
    float delay_time_ms;
    float delay_mix;
    float delay_feedback;
    bool enable_pitch_shift;
    float pitch_ratio;  // 1.0 = no change, 2.0 = octave up, 0.5 = octave down
} audio_effects_config_t;

/**
 * @brief Initialize audio effects module
 * @param config Effects configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t audio_effects_init(const audio_effects_config_t *config);

/**
 * @brief Update audio effects configuration
 * @param config New effects configuration
 */
void audio_effects_update_config(const audio_effects_config_t *config);

/**
 * @brief Process audio samples with configured effects
 * @param samples Pointer to audio samples buffer (24-bit in 32-bit containers)
 * @param sample_count Number of samples to process
 */
void audio_effects_process(int32_t *samples, size_t sample_count);

/**
 * @brief Cleanup audio effects resources
 */
void audio_effects_cleanup(void);

#endif // AUDIO_EFFECTS_H

