#include "audio_effects.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "AUDIO_EFFECTS";

// Effect state
static audio_effects_config_t current_config;
static bool effects_initialized = false;

// Delay effect state
static int32_t *delay_buffer = NULL;
static size_t delay_buffer_size = 0;
static size_t delay_write_pos = 0;

// Pitch shift effect state
static int32_t *pitch_buffer = NULL;
static size_t pitch_buffer_size = 0;
static size_t pitch_write_pos = 0;
static float pitch_read_pos = 0.0f;
static bool pitch_buffer_primed = false;  // Track if buffer has enough samples
static size_t pitch_samples_written = 0;  // Count samples written to buffer
static bool pitch_first_call_logged = false;  // Track if first call was logged

/**
 * @brief Apply delay effect to audio samples
 */
static void apply_delay_effect(int32_t *left_sample, int32_t *right_sample, 
                               int32_t orig_left, int32_t orig_right) {
    if (!current_config.enable_delay || delay_buffer == NULL || delay_buffer_size == 0) {
        return;
    }

    // Calculate delay offset in samples (stereo pairs)
    size_t delay_samples = (size_t)((current_config.delay_time_ms / 1000.0f) * 48000.0f * 2); // *2 for stereo
    delay_samples = (delay_samples / 2) * 2; // Ensure even number (stereo pairs)
    
    if (delay_samples > delay_buffer_size) {
        delay_samples = delay_buffer_size;
    }
    
    // Only apply delay if we have enough samples
    if (delay_samples > 0) {
        // Calculate read position (circular buffer)
        size_t delay_read_pos = (delay_write_pos >= delay_samples) ? 
                                (delay_write_pos - delay_samples) : 
                                (delay_buffer_size - (delay_samples - delay_write_pos));
        
        // Read delayed samples
        int32_t delayed_left = delay_buffer[delay_read_pos];
        int32_t delayed_right = delay_buffer[delay_read_pos + 1];
        
        // Mix delayed signal with input
        *left_sample = (int32_t)(orig_left + (delayed_left * current_config.delay_mix));
        *right_sample = (int32_t)(orig_right + (delayed_right * current_config.delay_mix));
    }
    
    // Write original input to delay buffer with feedback
    int32_t delayed_left_fb = delay_buffer[delay_write_pos];
    int32_t delayed_right_fb = delay_buffer[delay_write_pos + 1];
    delay_buffer[delay_write_pos] = orig_left + (int32_t)(delayed_left_fb * current_config.delay_feedback);
    delay_buffer[delay_write_pos + 1] = orig_right + (int32_t)(delayed_right_fb * current_config.delay_feedback);
    
    // Advance write position (circular buffer)
    delay_write_pos += 2;
    if (delay_write_pos >= delay_buffer_size) {
        delay_write_pos = 0;
    }
}

/**
 * @brief Apply pitch shift effect using circular buffer with linear interpolation
 * @param left_sample Output left sample
 * @param right_sample Output right sample
 * @param input_left Input left sample
 * @param input_right Input right sample
 */
static void apply_pitch_shift(int32_t *left_sample, int32_t *right_sample,
                              int32_t input_left, int32_t input_right) {
    if (!current_config.enable_pitch_shift) {
        return;
    }
    
    if (pitch_buffer == NULL || pitch_buffer_size == 0) {
        ESP_LOGE(TAG, "Pitch shift enabled but buffer not allocated!");
        *left_sample = input_left;
        *right_sample = input_right;
        return;
    }
    
    if (current_config.pitch_ratio <= 0.0f) {
        ESP_LOGE(TAG, "Invalid pitch ratio: %.2f", current_config.pitch_ratio);
        *left_sample = input_left;
        *right_sample = input_right;
        return;
    }
    
    // Minimum buffer distance to prevent reading unwritten samples
    // For pitch up (ratio > 1.0), read advances faster, so we need more buffer
    // For pitch down (ratio < 1.0), read advances slower, but we still need buffer
    // Use a smaller fixed minimum distance that fits in the reduced buffer
    // With 25ms buffer (9600 samples), we can use a smaller minimum distance
    const size_t min_buffer_distance = 480;  // Fixed minimum distance (~1.9KB) - 2.5ms at 192kHz
    const size_t min_samples_to_prime = min_buffer_distance + 240;  // Extra safety margin

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
        if (pitch_samples_written < min_samples_to_prime) {
            // Buffer not ready yet, pass through input
            *left_sample = input_left;
            *right_sample = input_right;
            return;
        }
        
        // Initialize read position (behind write position)
        // For pitch up (ratio > 1.0), read advances faster, so we need larger initial distance
        // For pitch down (ratio < 1.0), read advances slower, so smaller distance is OK
        size_t initial_distance = min_buffer_distance;
        if (current_config.pitch_ratio > 1.0f) {
            // For pitch up, we need extra buffer because read advances faster
            // Calculate how much buffer we need: for ratio=2.0, read advances 2x faster
            // So we need 2x the distance to last the same amount of time
            initial_distance = (size_t)(min_buffer_distance * current_config.pitch_ratio);
            // Cap at reasonable maximum
            if (initial_distance > pitch_buffer_size / 4) {
                initial_distance = pitch_buffer_size / 4;
            }
        }
        
        if (pitch_write_pos >= initial_distance) {
            pitch_read_pos = (float)(pitch_write_pos - initial_distance);
        } else {
            pitch_read_pos = (float)(pitch_buffer_size - (initial_distance - pitch_write_pos));
        }
        // Align to stereo pair boundary
        pitch_read_pos = ((int)pitch_read_pos / 2) * 2;
        pitch_buffer_primed = true;
    }

    // Calculate read position based on pitch ratio
    // For pitch shifting: we read from buffer at a rate determined by pitch_ratio
    // pitch_ratio > 1.0: higher pitch - read position advances faster (read older samples faster)
    // pitch_ratio < 1.0: lower pitch - read position advances slower (read older samples slower)
    // 
    // Key insight: We write 2 samples per call, but read position advances at rate = 2.0 * pitch_ratio
    // This means for pitch_ratio = 2.0, read advances by 4 per write of 2 (2x faster consumption)
    // For pitch_ratio = 0.5, read advances by 1 per write of 2 (0.5x slower consumption)
    //
    // The read position should advance BEFORE we read, so we read from the correct position
    float read_advance = 2.0f * current_config.pitch_ratio;
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
    
    // Check if we have enough buffer distance
    // For pitch up (ratio > 1.0), read advances faster than write, so distance decreases over time
    // We need enough initial buffer to handle this
    if (distance < min_buffer_distance) {
        // Buffer underrun - read position is too close to write position
        // For pitch up, this is expected as read advances faster
        // Instead of failing, adjust read position to maintain safe distance
        if (distance < 100) {
            // Too close - reset read position to safe distance behind write
            if (pitch_write_pos >= min_buffer_distance) {
                pitch_read_pos = (float)(pitch_write_pos - min_buffer_distance);
            } else {
                pitch_read_pos = (float)(pitch_buffer_size - (min_buffer_distance - pitch_write_pos));
            }
            // Align to stereo pair
            pitch_read_pos = ((int)pitch_read_pos / 2) * 2;
            read_pos_int = (size_t)pitch_read_pos;
            distance = (pitch_write_pos >= read_pos_int) ? 
                      (pitch_write_pos - read_pos_int) : 
                      (pitch_buffer_size - read_pos_int + pitch_write_pos);
        }
        
        // If still too close, use input directly (no pitch shift)
        if (distance < 100) {
            *left_sample = input_left;
            *right_sample = input_right;
            return;
        }
        
        // Otherwise, continue with reduced distance (might have slight artifacts but should work)
    }

    // Normal operation: read from buffer with interpolation
    size_t read_idx = (size_t)pitch_read_pos;
    float fraction = pitch_read_pos - (float)read_idx;
    
    // Align to stereo pair boundary
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
    *left_sample = (int32_t)(left1 + (left2 - left1) * fraction);

    int32_t right1 = pitch_buffer[read_idx + 1];
    int32_t right2 = pitch_buffer[next_idx + 1];
    *right_sample = (int32_t)(right1 + (right2 - right1) * fraction);
}

/**
 * @brief Apply volume scaling and channel swap
 */
static void apply_volume_and_swap(int32_t *samples, size_t index, 
                                  int32_t left_sample, int32_t right_sample) {
    if (current_config.enable_channel_swap) {
        // Swap channels: right becomes left, left becomes right
        samples[index] = (int32_t)(right_sample * current_config.volume_scale * 2);
        samples[index + 1] = (int32_t)(left_sample * current_config.volume_scale);
    } else {
        // Apply volume scaling without channel swap
        samples[index] = (int32_t)(left_sample * current_config.volume_scale * 2);
        samples[index + 1] = (int32_t)(right_sample * current_config.volume_scale);
    }
}

void audio_effects_process(int32_t *samples, size_t sample_count) {
    if (!samples || sample_count == 0 || !effects_initialized) {
        return;
    }

    // Process stereo samples (2 channels)
    for (size_t i = 0; i < sample_count; i += 2) {
        if (i + 1 >= sample_count) break; // Ensure we have a stereo pair
        
        int32_t left_sample = samples[i];
        int32_t right_sample = samples[i + 1];
        
        // Apply pitch shift first (needs original input)
        // Store original samples before pitch shift
        int32_t orig_left = left_sample;
        int32_t orig_right = right_sample;
        
        if (current_config.enable_pitch_shift) {
            apply_pitch_shift(&left_sample, &right_sample, orig_left, orig_right);
        }
        
        // Apply delay effect if enabled
        apply_delay_effect(&left_sample, &right_sample, left_sample, right_sample);
        
        // Apply volume and channel swap
        apply_volume_and_swap(samples, i, left_sample, right_sample);
    }
}

esp_err_t audio_effects_init(const audio_effects_config_t *config) {
    if (effects_initialized) {
        ESP_LOGW(TAG, "Audio effects already initialized");
        return ESP_OK;
    }

    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration provided");
        return ESP_ERR_INVALID_ARG;
    }

    // Allocate delay buffer (500ms max delay at 48kHz stereo = ~96KB)
    const size_t max_delay_samples = 48000 / 2 ; // 500ms at 48kHz, stereo
    delay_buffer_size = max_delay_samples;
    delay_buffer = (int32_t *)calloc(delay_buffer_size, sizeof(int32_t));
    if (delay_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate delay buffer (%u bytes)", 
                 (unsigned int)(delay_buffer_size * sizeof(int32_t)));
        return ESP_ERR_NO_MEM;
    }
    delay_write_pos = 0;
    
    ESP_LOGI(TAG, "Delay buffer allocated: %u samples (%u KB)", 
             (unsigned int)delay_buffer_size, 
             (unsigned int)(delay_buffer_size * sizeof(int32_t) / 1024));

    // Store configuration first to check if pitch shift is enabled
    memcpy(&current_config, config, sizeof(audio_effects_config_t));
    
    // Only allocate pitch shift buffer if pitch shift is enabled
    if (config->enable_pitch_shift) {
    // Allocate pitch shift buffer (25ms at 192kHz stereo)
    // Buffer needs to be large enough for:
    // - Pitch down (0.5x): needs 2x buffer = 50ms effective
    // - Pitch up (2.0x): read advances 2x faster, so needs buffer to prevent underrun
    // 25ms should be sufficient for real-time processing (~9.6KB)
    const size_t max_pitch_samples = (192000 / 40) * 2; // 25ms at 192kHz, stereo pairs
        pitch_buffer_size = max_pitch_samples;
        size_t buffer_bytes = pitch_buffer_size * sizeof(int32_t);
        // Try to allocate from internal RAM first, then fall back to regular malloc
        pitch_buffer = (int32_t *)heap_caps_malloc(buffer_bytes, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        if (pitch_buffer == NULL) {
            // Fall back to regular calloc
            pitch_buffer = (int32_t *)calloc(pitch_buffer_size, sizeof(int32_t));
        }
        if (pitch_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate pitch buffer (%u bytes), free heap: %u bytes", 
                     (unsigned int)buffer_bytes, (unsigned int)esp_get_free_heap_size());
            free(delay_buffer);
            delay_buffer = NULL;
            return ESP_ERR_NO_MEM;
        }
        // Zero the buffer
        memset(pitch_buffer, 0, buffer_bytes);
        pitch_write_pos = 0;
        pitch_read_pos = 0.0f;
        pitch_buffer_primed = false;
        pitch_samples_written = 0;
    } else {
        // Pitch shift disabled - don't allocate buffer
        pitch_buffer = NULL;
        pitch_buffer_size = 0;
        pitch_write_pos = 0;
        pitch_read_pos = 0.0f;
        pitch_buffer_primed = false;
        pitch_samples_written = 0;
        pitch_first_call_logged = false;
    }
    
    effects_initialized = true;
    ESP_LOGI(TAG, "Audio effects initialized successfully");
    return ESP_OK;
}

void audio_effects_update_config(const audio_effects_config_t *config) {
    if (!config || !effects_initialized) {
        return;
    }
    
    // Check if pitch shift is being toggled
    bool was_enabled = current_config.enable_pitch_shift;
    bool will_be_enabled = config->enable_pitch_shift;
    
    memcpy(&current_config, config, sizeof(audio_effects_config_t));
    
    // Handle pitch shift buffer allocation/deallocation
    if (will_be_enabled && !was_enabled) {
        // Pitch shift just enabled - allocate buffer
        // Use 25ms buffer (9.6KB) - sufficient for real-time pitch shifting
        const size_t max_pitch_samples = (192000 / 40) * 2; // 25ms at 192kHz, stereo pairs
        pitch_buffer_size = max_pitch_samples;
        size_t buffer_bytes = pitch_buffer_size * sizeof(int32_t);
        // Try to allocate from internal RAM first, then fall back to regular malloc
        pitch_buffer = (int32_t *)heap_caps_malloc(buffer_bytes, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        if (pitch_buffer == NULL) {
            // Fall back to regular calloc
            pitch_buffer = (int32_t *)calloc(pitch_buffer_size, sizeof(int32_t));
        }
        if (pitch_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate pitch buffer on enable (%u bytes), free heap: %u bytes", 
                     (unsigned int)buffer_bytes, (unsigned int)esp_get_free_heap_size());
            current_config.enable_pitch_shift = false; // Disable if allocation fails
        } else {
            // Zero the buffer
            memset(pitch_buffer, 0, buffer_bytes);
            pitch_write_pos = 0;
            pitch_read_pos = 0.0f;
            pitch_buffer_primed = false;
            pitch_samples_written = 0;
            pitch_first_call_logged = false;
        }
    } else if (!will_be_enabled && was_enabled) {
        // Pitch shift just disabled - deallocate buffer
        if (pitch_buffer != NULL) {
            free(pitch_buffer);
            pitch_buffer = NULL;
            pitch_buffer_size = 0;
            pitch_write_pos = 0;
            pitch_read_pos = 0.0f;
            pitch_buffer_primed = false;
            pitch_samples_written = 0;
            pitch_first_call_logged = false;
        }
    } else if (will_be_enabled && was_enabled) {
        // Pitch shift already enabled - reset buffer if ratio changed significantly
        if (fabsf(current_config.pitch_ratio - config->pitch_ratio) > 0.1f) {
            pitch_write_pos = 0;
            pitch_read_pos = 0.0f;
            pitch_buffer_primed = false;
            pitch_samples_written = 0;
            pitch_first_call_logged = false;  // Reset first call log
            // Clear buffer to avoid clicks
            if (pitch_buffer != NULL) {
                memset(pitch_buffer, 0, pitch_buffer_size * sizeof(int32_t));
            }
        }
    }
}

void audio_effects_cleanup(void) {
    if (delay_buffer != NULL) {
        free(delay_buffer);
        delay_buffer = NULL;
        delay_buffer_size = 0;
        delay_write_pos = 0;
    }
    
    if (pitch_buffer != NULL) {
        free(pitch_buffer);
        pitch_buffer = NULL;
        pitch_buffer_size = 0;
        pitch_write_pos = 0;
        pitch_read_pos = 0.0f;
        pitch_buffer_primed = false;
        pitch_samples_written = 0;
    }
    
    effects_initialized = false;
    ESP_LOGI(TAG, "Audio effects cleaned up");
}

