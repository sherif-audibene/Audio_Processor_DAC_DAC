#ifndef PITCH_SHIFT_EFFECT_H
#define PITCH_SHIFT_EFFECT_H

#include "effect_common.h"

/**
 * @brief Pitch shift effect configuration
 */
typedef struct {
    bool enable;
    float pitch_ratio;  // 1.0 = no change, 2.0 = octave up, 0.5 = octave down
} pitch_shift_effect_config_t;

/**
 * @brief Initialize pitch shift effect
 * @param config Initial configuration
 * @return ESP_OK on success
 */
esp_err_t pitch_shift_effect_init(const pitch_shift_effect_config_t *config);

/**
 * @brief Update pitch shift effect configuration
 * @param config New configuration
 */
void pitch_shift_effect_update_config(const pitch_shift_effect_config_t *config);

/**
 * @brief Process a stereo sample through the pitch shift effect
 * @param left_sample Pointer to left sample (output)
 * @param right_sample Pointer to right sample (output)
 * @param input_left Original left sample
 * @param input_right Original right sample
 */
void pitch_shift_effect_process(int32_t *left_sample, int32_t *right_sample,
                                int32_t input_left, int32_t input_right);

/**
 * @brief Cleanup pitch shift effect resources
 */
void pitch_shift_effect_cleanup(void);

/**
 * @brief Check if pitch shift effect is enabled
 * @return true if enabled
 */
bool pitch_shift_effect_is_enabled(void);

#endif // PITCH_SHIFT_EFFECT_H

