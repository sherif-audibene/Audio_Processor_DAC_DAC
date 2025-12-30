#ifndef EFFECT_COMMON_H
#define EFFECT_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Common sample rate for effects (in Hz)
 */
#define EFFECT_SAMPLE_RATE 48000

/**
 * @brief Stereo sample pair structure for effect processing
 */
typedef struct {
    int32_t left;
    int32_t right;
} stereo_sample_t;

#endif // EFFECT_COMMON_H

