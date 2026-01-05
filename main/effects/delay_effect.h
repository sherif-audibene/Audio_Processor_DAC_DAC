#ifndef DELAY_EFFECT_H
#define DELAY_EFFECT_H

#include "effect_common.h"

/**
 * @brief Delay effect configuration
 */
typedef struct {
    bool enable;
    float delay_time_ms;  // Delay time in milliseconds
    float mix;            // Wet/dry mix (0.0 to 1.0)
    float feedback;       // Feedback amount (0.0 to 1.0)
} delay_effect_config_t;

/**
 * @brief Initialize delay effect
 * @param config Initial configuration
 * @return ESP_OK on success
 */
esp_err_t delay_effect_init(const delay_effect_config_t *config);

/**
 * @brief Update delay effect configuration
 * @param config New configuration
 */
void delay_effect_update_config(const delay_effect_config_t *config);

/**
 * @brief Process a stereo sample through the delay effect
 * @param left_sample Pointer to left sample (input/output)
 * @param right_sample Pointer to right sample (input/output)
 * @param orig_left Original left sample
 * @param orig_right Original right sample
 */
void delay_effect_process(int32_t *left_sample, int32_t *right_sample,
                          int32_t orig_left, int32_t orig_right);

/**
 * @brief Process batch of stereo samples through delay effect (SIMD optimized)
 * @param samples Interleaved stereo samples [L0,R0,L1,R1,...] (input/output)
 * @param sample_count Total number of int32_t values (must be even for stereo pairs)
 */
void delay_effect_process_batch(int32_t *samples, size_t sample_count);

/**
 * @brief Cleanup delay effect resources
 */
void delay_effect_cleanup(void);

/**
 * @brief Check if delay effect is enabled
 * @return true if enabled
 */
bool delay_effect_is_enabled(void);

#endif // DELAY_EFFECT_H

