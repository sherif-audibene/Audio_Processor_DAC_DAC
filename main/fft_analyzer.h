#ifndef FFT_ANALYZER_H
#define FFT_ANALYZER_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// FFT configuration
#define FFT_SIZE 128  // Must be power of 2, matches LCD width for 1:1 mapping
#define FFT_OUTPUT_SIZE (FFT_SIZE / 2)  // Only real frequencies (Nyquist)

/**
 * @brief Initialize FFT analyzer
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fft_analyzer_init(void);

/**
 * @brief Compute FFT of audio samples and return magnitude spectrum
 * 
 * @param samples Input audio samples (16-bit signed)
 * @param num_samples Number of samples (should be FFT_SIZE or less)
 * @param output_magnitude Output magnitude spectrum (FFT_OUTPUT_SIZE elements)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fft_analyzer_compute(const int16_t *samples, size_t num_samples, float *output_magnitude);

/**
 * @brief Cleanup FFT analyzer resources
 */
void fft_analyzer_cleanup(void);

#endif // FFT_ANALYZER_H

