#include "pitch_shift_effect.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "PITCH_SHIFT_EFFECT";

// High sample rate for pitch shift processing
#define PITCH_SAMPLE_RATE 192000

// Effect state
static pitch_shift_effect_config_t config;
static bool initialized = false;

// Pitch buffer
static int32_t *pitch_buffer = NULL;
static size_t pitch_buffer_size = 0;
static size_t pitch_write_pos = 0;
static float pitch_read_pos = 0.0f;
static bool pitch_buffer_primed = false;
static size_t pitch_samples_written = 0;

// Minimum buffer distance to prevent reading unwritten samples
static const size_t MIN_BUFFER_DISTANCE = 960;   // Increased for more headroom
static const size_t MIN_SAMPLES_TO_PRIME = 1440; // MIN_BUFFER_DISTANCE * 1.5

// Crossfade for click-free transitions
static const size_t CROSSFADE_SAMPLES = 64;  // Number of stereo pairs for crossfade
static float crossfade_progress = 1.0f;      // 0.0 = start of fade, 1.0 = complete
static int32_t crossfade_left_start = 0;
static int32_t crossfade_right_start = 0;

/**
 * @brief Allocate pitch buffer
 */
static esp_err_t allocate_pitch_buffer(void) {
    // Try larger buffer first (50ms), fall back to smaller (25ms) if allocation fails
    const size_t buffer_sizes[] = {
        (PITCH_SAMPLE_RATE / 20) * 2,  // 50ms - preferred (fewer clicks)
        (PITCH_SAMPLE_RATE / 40) * 2,  // 25ms - fallback
    };
    
    for (int i = 0; i < 2; i++) {
        pitch_buffer_size = buffer_sizes[i];
        size_t buffer_bytes = pitch_buffer_size * sizeof(int32_t);
        
        // Try PSRAM/SPIRAM first (ESP32-S3 typically has external RAM)
        pitch_buffer = (int32_t *)heap_caps_malloc(buffer_bytes, MALLOC_CAP_SPIRAM);
        if (pitch_buffer != NULL) {
            ESP_LOGI(TAG, "Allocated pitch buffer in PSRAM (%u bytes)", (unsigned int)buffer_bytes);
            break;
        }
        
        // Try internal RAM
        pitch_buffer = (int32_t *)heap_caps_malloc(buffer_bytes, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        if (pitch_buffer != NULL) {
            ESP_LOGI(TAG, "Allocated pitch buffer in internal RAM (%u bytes)", (unsigned int)buffer_bytes);
            break;
        }
        
        // Try default allocator
        pitch_buffer = (int32_t *)calloc(pitch_buffer_size, sizeof(int32_t));
        if (pitch_buffer != NULL) {
            ESP_LOGI(TAG, "Allocated pitch buffer via malloc (%u bytes)", (unsigned int)buffer_bytes);
            break;
        }
        
        ESP_LOGW(TAG, "Failed to allocate %u bytes, trying smaller buffer...", (unsigned int)buffer_bytes);
    }
    
    if (pitch_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate pitch buffer, free heap: %u bytes",
                 (unsigned int)esp_get_free_heap_size());
        return ESP_ERR_NO_MEM;
    }
    
    memset(pitch_buffer, 0, pitch_buffer_size * sizeof(int32_t));
    pitch_write_pos = 0;
    pitch_read_pos = 0.0f;
    pitch_buffer_primed = false;
    pitch_samples_written = 0;
    
    return ESP_OK;
}

/**
 * @brief Free pitch buffer
 */
static void free_pitch_buffer(void) {
    if (pitch_buffer != NULL) {
        free(pitch_buffer);
        pitch_buffer = NULL;
        pitch_buffer_size = 0;
        pitch_write_pos = 0;
        pitch_read_pos = 0.0f;
        pitch_buffer_primed = false;
        pitch_samples_written = 0;
    }
}

/**
 * @brief Reset pitch buffer state
 */
static void reset_buffer_state(void) {
    pitch_write_pos = 0;
    pitch_read_pos = 0.0f;
    pitch_buffer_primed = false;
    pitch_samples_written = 0;
    crossfade_progress = 1.0f;
    crossfade_left_start = 0;
    crossfade_right_start = 0;
    if (pitch_buffer != NULL) {
        memset(pitch_buffer, 0, pitch_buffer_size * sizeof(int32_t));
    }
}

esp_err_t pitch_shift_effect_init(const pitch_shift_effect_config_t *init_config) {
    if (initialized) {
        ESP_LOGW(TAG, "Pitch shift effect already initialized");
        return ESP_OK;
    }

    if (!init_config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&config, init_config, sizeof(pitch_shift_effect_config_t));
    
    // Only allocate buffer if enabled
    if (config.enable) {
        esp_err_t ret = allocate_pitch_buffer();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    initialized = true;
    ESP_LOGI(TAG, "Pitch shift effect initialized");
    return ESP_OK;
}

void pitch_shift_effect_update_config(const pitch_shift_effect_config_t *new_config) {
    if (!new_config || !initialized) {
        return;
    }
    
    bool was_enabled = config.enable;
    bool will_be_enabled = new_config->enable;
    float old_ratio = config.pitch_ratio;
    
    memcpy(&config, new_config, sizeof(pitch_shift_effect_config_t));
    
    // Handle buffer allocation/deallocation
    if (will_be_enabled && !was_enabled) {
        if (allocate_pitch_buffer() != ESP_OK) {
            config.enable = false;
        }
    } else if (!will_be_enabled && was_enabled) {
        free_pitch_buffer();
    } else if (will_be_enabled && was_enabled) {
        // Reset buffer if ratio changed significantly
        if (fabsf(config.pitch_ratio - old_ratio) > 0.1f) {
            reset_buffer_state();
        }
    }
}

void pitch_shift_effect_process(int32_t *left_sample, int32_t *right_sample,
                                int32_t input_left, int32_t input_right) {
    if (!config.enable) {
        return;
    }
    
    if (pitch_buffer == NULL || pitch_buffer_size == 0) {
        ESP_LOGE(TAG, "Pitch shift enabled but buffer not allocated!");
        *left_sample = input_left;
        *right_sample = input_right;
        return;
    }
    
    if (config.pitch_ratio <= 0.0f) {
        ESP_LOGE(TAG, "Invalid pitch ratio: %.2f", config.pitch_ratio);
        *left_sample = input_left;
        *right_sample = input_right;
        return;
    }

    // Write input samples to pitch buffer
    pitch_buffer[pitch_write_pos] = input_left;
    pitch_buffer[pitch_write_pos + 1] = input_right;
    pitch_write_pos += 2;
    pitch_samples_written += 2;
    if (pitch_write_pos >= pitch_buffer_size) {
        pitch_write_pos = 0;
    }

    // Wait until buffer is properly filled before starting to read
    if (!pitch_buffer_primed) {
        if (pitch_samples_written < MIN_SAMPLES_TO_PRIME) {
            *left_sample = input_left;
            *right_sample = input_right;
            return;
        }
        
        // Initialize read position behind write position
        size_t initial_distance = MIN_BUFFER_DISTANCE;
        if (config.pitch_ratio > 1.0f) {
            initial_distance = (size_t)(MIN_BUFFER_DISTANCE * config.pitch_ratio);
            if (initial_distance > pitch_buffer_size / 4) {
                initial_distance = pitch_buffer_size / 4;
            }
        }
        
        if (pitch_write_pos >= initial_distance) {
            pitch_read_pos = (float)(pitch_write_pos - initial_distance);
        } else {
            pitch_read_pos = (float)(pitch_buffer_size - (initial_distance - pitch_write_pos));
        }
        pitch_read_pos = (float)(((int)pitch_read_pos / 2) * 2);
        pitch_buffer_primed = true;
    }

    // Calculate read position based on pitch ratio
    float read_advance = 2.0f * config.pitch_ratio;
    pitch_read_pos += read_advance;

    // Wrap read position to buffer bounds
    while (pitch_read_pos >= (float)pitch_buffer_size) {
        pitch_read_pos -= (float)pitch_buffer_size;
    }
    while (pitch_read_pos < 0.0f) {
        pitch_read_pos += (float)pitch_buffer_size;
    }

    // Check minimum distance to avoid reading unwritten samples
    size_t read_pos_int = (size_t)pitch_read_pos;
    size_t distance;
    if (pitch_write_pos >= read_pos_int) {
        distance = pitch_write_pos - read_pos_int;
    } else {
        distance = pitch_buffer_size - read_pos_int + pitch_write_pos;
    }
    
    if (distance < MIN_BUFFER_DISTANCE) {
        if (distance < 100) {
            // Store current sample for crossfade before resetting
            if (crossfade_progress >= 1.0f) {
                // Read current position before reset for crossfade
                size_t curr_idx = ((size_t)pitch_read_pos / 2) * 2;
                if (curr_idx < pitch_buffer_size) {
                    crossfade_left_start = pitch_buffer[curr_idx];
                    crossfade_right_start = pitch_buffer[curr_idx + 1];
                } else {
                    crossfade_left_start = input_left;
                    crossfade_right_start = input_right;
                }
                crossfade_progress = 0.0f;  // Start crossfade
            }
            
            // Reset read position to safe distance
            if (pitch_write_pos >= MIN_BUFFER_DISTANCE) {
                pitch_read_pos = (float)(pitch_write_pos - MIN_BUFFER_DISTANCE);
            } else {
                pitch_read_pos = (float)(pitch_buffer_size - (MIN_BUFFER_DISTANCE - pitch_write_pos));
            }
            pitch_read_pos = (float)(((int)pitch_read_pos / 2) * 2);
            read_pos_int = (size_t)pitch_read_pos;
            distance = (pitch_write_pos >= read_pos_int) ?
                      (pitch_write_pos - read_pos_int) :
                      (pitch_buffer_size - read_pos_int + pitch_write_pos);
        }
        
        if (distance < 100) {
            *left_sample = input_left;
            *right_sample = input_right;
            return;
        }
    }

    // Normal operation: read from buffer with linear interpolation
    size_t read_idx = (size_t)pitch_read_pos;
    float fraction = pitch_read_pos - (float)read_idx;
    
    if (read_idx % 2 != 0) {
        read_idx = (read_idx / 2) * 2;
        fraction = pitch_read_pos - (float)read_idx;
    }
    
    size_t next_idx = read_idx + 2;
    if (next_idx >= pitch_buffer_size) {
        next_idx = 0;
    }

    // Linear interpolation
    int32_t left1 = pitch_buffer[read_idx];
    int32_t left2 = pitch_buffer[next_idx];
    int32_t out_left = (int32_t)(left1 + (left2 - left1) * fraction);

    int32_t right1 = pitch_buffer[read_idx + 1];
    int32_t right2 = pitch_buffer[next_idx + 1];
    int32_t out_right = (int32_t)(right1 + (right2 - right1) * fraction);

    // Apply crossfade if in progress (smooth transition after position reset)
    if (crossfade_progress < 1.0f) {
        float fade_in = crossfade_progress;
        float fade_out = 1.0f - crossfade_progress;
        
        out_left = (int32_t)(crossfade_left_start * fade_out + out_left * fade_in);
        out_right = (int32_t)(crossfade_right_start * fade_out + out_right * fade_in);
        
        // Advance crossfade (complete over CROSSFADE_SAMPLES)
        crossfade_progress += 1.0f / (float)CROSSFADE_SAMPLES;
        if (crossfade_progress > 1.0f) {
            crossfade_progress = 1.0f;
        }
    }

    *left_sample = out_left;
    *right_sample = out_right;
}

void pitch_shift_effect_cleanup(void) {
    free_pitch_buffer();
    initialized = false;
    ESP_LOGI(TAG, "Pitch shift effect cleaned up");
}

bool pitch_shift_effect_is_enabled(void) {
    return config.enable;
}

