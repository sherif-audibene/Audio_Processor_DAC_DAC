/**
 * @file fft_analyzer.c
 * @brief FFT analyzer implementation using esp-dsp library
 * 
 * This module provides real-time FFT analysis of audio samples using
 * the Espressif DSP library for optimized performance on ESP32.
 */

#include "fft_analyzer.h"
#include "esp_log.h"
#include "esp_dsp.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "FFT_ANALYZER";

// ============================================================================
// SPECTRUM DISPLAY TUNING PARAMETERS
// Adjust these to change noise sensitivity
// ============================================================================

// Dynamic range in dB (signals below this threshold from max are ignored)
// Lower value = less sensitive to noise (try 30-50)
// Higher value = more sensitive, shows quieter signals (try 50-80)
#define FFT_DYNAMIC_RANGE_DB    40.0f

// Noise floor threshold (normalized 0.0-1.0)
// Signals below this level are set to zero
// Higher value = more noise rejection (try 0.05-0.15)
#define FFT_NOISE_FLOOR         0.05f

// ============================================================================

// FFT working buffers - aligned for SIMD operations
__attribute__((aligned(16)))
static float fft_window[FFT_SIZE];

__attribute__((aligned(16)))
static float fft_input[FFT_SIZE];

// Complex FFT output buffer (interleaved real/imag)
__attribute__((aligned(16)))
static float fft_complex[FFT_SIZE * 2];

static bool fft_initialized = false;

esp_err_t fft_analyzer_init(void) {
    if (fft_initialized) {
        ESP_LOGW(TAG, "FFT analyzer already initialized");
        return ESP_OK;
    }
    
    // Initialize esp-dsp FFT
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, FFT_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize esp-dsp FFT: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Generate Hann window coefficients
    dsps_wind_hann_f32(fft_window, FFT_SIZE);
    
    // Clear buffers
    memset(fft_input, 0, sizeof(fft_input));
    memset(fft_complex, 0, sizeof(fft_complex));
    
    fft_initialized = true;
    ESP_LOGI(TAG, "FFT analyzer initialized (esp-dsp, size=%d, output_bins=%d)", 
             FFT_SIZE, FFT_OUTPUT_SIZE);
    
    return ESP_OK;
}

esp_err_t fft_analyzer_compute(const int16_t *samples, size_t num_samples, float *output_magnitude) {
    if (!fft_initialized) {
        ESP_LOGE(TAG, "FFT analyzer not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (samples == NULL || output_magnitude == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Limit to FFT_SIZE
    size_t samples_to_use = (num_samples > FFT_SIZE) ? FFT_SIZE : num_samples;
    
    // Convert int16 samples to float and apply window
    // Normalize to [-1.0, 1.0] range
    for (size_t i = 0; i < FFT_SIZE; i++) {
        if (i < samples_to_use) {
            // Normalize sample and apply Hann window
            float sample = (float)samples[i] / 32768.0f;
            fft_input[i] = sample * fft_window[i];
        } else {
            // Zero-pad if we have fewer samples
            fft_input[i] = 0.0f;
        }
    }
    
    // Prepare complex input (real samples with zero imaginary parts)
    // esp-dsp expects interleaved complex data: [Re0, Im0, Re1, Im1, ...]
    for (size_t i = 0; i < FFT_SIZE; i++) {
        fft_complex[i * 2 + 0] = fft_input[i];  // Real part
        fft_complex[i * 2 + 1] = 0.0f;          // Imaginary part
    }
    
    // Perform FFT using esp-dsp
    dsps_fft2r_fc32(fft_complex, FFT_SIZE);
    
    // Bit-reverse the output
    dsps_bit_rev_fc32(fft_complex, FFT_SIZE);
    
    // Compute magnitude spectrum for positive frequencies only
    // Output is in dB, normalized to 0.0-1.0 range for display
    for (size_t i = 0; i < FFT_OUTPUT_SIZE; i++) {
        float real = fft_complex[i * 2 + 0];
        float imag = fft_complex[i * 2 + 1];
        
        // Compute magnitude squared
        float magnitude_sq = real * real + imag * imag;
        
        // Convert to dB scale (with floor to avoid log(0))
        // Normalize by FFT size for proper scaling
        float magnitude_db = 10.0f * log10f((magnitude_sq / (float)FFT_SIZE) + 1e-10f);
        
        // Normalize to 0.0-1.0 range for display
        // Map from -FFT_DYNAMIC_RANGE_DB to 0dB
        float normalized = (magnitude_db + FFT_DYNAMIC_RANGE_DB) / FFT_DYNAMIC_RANGE_DB;
        
        // Clamp to valid range
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 1.0f) normalized = 1.0f;
        
        // Apply noise floor threshold
        if (normalized < FFT_NOISE_FLOOR) {
            normalized = 0.0f;
        }
        
        output_magnitude[i] = normalized;
    }
    
    return ESP_OK;
}

float fft_analyzer_bin_to_frequency(uint16_t bin_index, uint32_t sample_rate) {
    // Frequency for bin = (bin_index * sample_rate) / FFT_SIZE
    return ((float)bin_index * (float)sample_rate) / (float)FFT_SIZE;
}

void fft_analyzer_cleanup(void) {
    // esp-dsp doesn't require explicit cleanup for FFT
    // Just reset our state
    fft_initialized = false;
    ESP_LOGI(TAG, "FFT analyzer cleaned up");
}
