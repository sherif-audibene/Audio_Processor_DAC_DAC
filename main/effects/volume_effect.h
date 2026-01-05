#ifndef VOLUME_EFFECT_H
#define VOLUME_EFFECT_H

#include "effect_common.h"

/**
 * @brief Volume effect configuration
 */
typedef struct {
    float volume_scale;       // Volume multiplier (0.0 to N)
    bool enable_channel_swap; // Swap left and right channels
} volume_effect_config_t;

/**
 * @brief Initialize volume effect
 * @param config Initial configuration
 * @return ESP_OK on success
 */
esp_err_t volume_effect_init(const volume_effect_config_t *config);

/**
 * @brief Update volume effect configuration
 * @param config New configuration
 */
void volume_effect_update_config(const volume_effect_config_t *config);

/**
 * @brief Apply volume and channel swap to samples and write to output buffer
 * @param samples Output sample buffer
 * @param index Index in the output buffer
 * @param left_sample Left channel sample
 * @param right_sample Right channel sample
 */
void volume_effect_process(int32_t *samples, size_t index,
                           int32_t left_sample, int32_t right_sample);

/**
 * @brief Process batch of stereo samples with volume scaling (SIMD optimized)
 * @param samples Interleaved stereo samples [L0,R0,L1,R1,...] (input/output)
 * @param sample_count Total number of int32_t values (must be even for stereo pairs)
 */
void volume_effect_process_batch(int32_t *samples, size_t sample_count);

/**
 * @brief Cleanup volume effect resources
 */
void volume_effect_cleanup(void);

#endif // VOLUME_EFFECT_H

