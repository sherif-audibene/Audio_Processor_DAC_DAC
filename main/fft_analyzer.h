#ifndef FFT_ANALYZER_H
#define FFT_ANALYZER_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// FFT configuration - using esp-dsp library
// 256 samples provides good frequency resolution at 192kHz sample rate
// Each bin = 192000 / 256 = 750 Hz resolution
#define FFT_SIZE 512  // Must be power of 2
#define FFT_OUTPUT_SIZE (FFT_SIZE / 2)  // Only positive frequencies (Nyquist)

/**
 * @brief Initialize FFT analyzer using esp-dsp library
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fft_analyzer_init(void);

/**
 * @brief Compute FFT of audio samples and return magnitude spectrum
 * Uses esp-dsp optimized FFT implementation
 * 
 * @param samples Input audio samples (16-bit signed)
 * @param num_samples Number of samples (should be FFT_SIZE or less)
 * @param output_magnitude Output magnitude spectrum (FFT_OUTPUT_SIZE elements)
 *                         Values are normalized 0.0 to 1.0
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fft_analyzer_compute(const int16_t *samples, size_t num_samples, float *output_magnitude);

/**
 * @brief Get the frequency in Hz for a given FFT bin index
 * 
 * @param bin_index FFT bin index (0 to FFT_OUTPUT_SIZE-1)
 * @param sample_rate Audio sample rate in Hz
 * @return Frequency in Hz for the center of this bin
 */
float fft_analyzer_bin_to_frequency(uint16_t bin_index, uint32_t sample_rate);

/**
 * @brief Cleanup FFT analyzer resources
 */
void fft_analyzer_cleanup(void);

#endif // FFT_ANALYZER_H

