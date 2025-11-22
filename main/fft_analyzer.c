#include "fft_analyzer.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "FFT_ANALYZER";

// FFT working buffers
static float *fft_real = NULL;
static float *fft_imag = NULL;
static bool fft_initialized = false;

/**
 * @brief Bit-reverse function for FFT
 */
static uint16_t bit_reverse(uint16_t n, uint8_t bits) {
    uint16_t reversed = 0;
    for (uint8_t i = 0; i < bits; i++) {
        reversed = (reversed << 1) | (n & 1);
        n >>= 1;
    }
    return reversed;
}

/**
 * @brief Radix-2 FFT implementation
 */
static void fft_compute(float *real, float *imag, uint8_t log2n) {
    uint16_t n = 1 << log2n;
    
    // Bit-reverse permutation
    for (uint16_t i = 0; i < n; i++) {
        uint16_t j = bit_reverse(i, log2n);
        if (i < j) {
            float temp = real[i];
            real[i] = real[j];
            real[j] = temp;
            temp = imag[i];
            imag[i] = imag[j];
            imag[j] = temp;
        }
    }
    
    // FFT computation
    for (uint8_t stage = 1; stage <= log2n; stage++) {
        uint16_t m = 1 << stage;
        uint16_t m2 = m >> 1;
        
        float w_real = 1.0f;
        float w_imag = 0.0f;
        float angle = -M_PI / m2;
        float w_step_real = cosf(angle);
        float w_step_imag = sinf(angle);
        
        for (uint16_t j = 0; j < m2; j++) {
            for (uint16_t k = j; k < n; k += m) {
                uint16_t t = k + m2;
                float t_real = w_real * real[t] - w_imag * imag[t];
                float t_imag = w_real * imag[t] + w_imag * real[t];
                
                real[t] = real[k] - t_real;
                imag[t] = imag[k] - t_imag;
                real[k] += t_real;
                imag[k] += t_imag;
            }
            
            // Update twiddle factor
            float temp = w_real * w_step_real - w_imag * w_step_imag;
            w_imag = w_real * w_step_imag + w_imag * w_step_real;
            w_real = temp;
        }
    }
}

esp_err_t fft_analyzer_init(void) {
    if (fft_initialized) {
        ESP_LOGW(TAG, "FFT analyzer already initialized");
        return ESP_OK;
    }
    
    // Allocate buffers
    fft_real = (float *)malloc(FFT_SIZE * sizeof(float));
    fft_imag = (float *)malloc(FFT_SIZE * sizeof(float));
    
    if (fft_real == NULL || fft_imag == NULL) {
        ESP_LOGE(TAG, "Failed to allocate FFT buffers");
        if (fft_real) free(fft_real);
        if (fft_imag) free(fft_imag);
        fft_real = NULL;
        fft_imag = NULL;
        return ESP_ERR_NO_MEM;
    }
    
    fft_initialized = true;
    ESP_LOGI(TAG, "FFT analyzer initialized (size=%d)", FFT_SIZE);
    return ESP_OK;
}

esp_err_t fft_analyzer_compute(const int16_t *samples, size_t num_samples, float *output_magnitude) {
    if (!fft_initialized || fft_real == NULL || fft_imag == NULL) {
        ESP_LOGE(TAG, "FFT analyzer not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (samples == NULL || output_magnitude == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Limit to FFT_SIZE
    size_t samples_to_use = (num_samples > FFT_SIZE) ? FFT_SIZE : num_samples;
    
    // Convert samples to float and apply window function (Hanning window)
    for (size_t i = 0; i < FFT_SIZE; i++) {
        if (i < samples_to_use) {
            // Normalize to [-1.0, 1.0] and apply Hanning window
            float sample = (float)samples[i] / 32768.0f;
            float window = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (FFT_SIZE - 1)));
            fft_real[i] = sample * window;
        } else {
            // Zero-pad if we have fewer samples
            fft_real[i] = 0.0f;
        }
        fft_imag[i] = 0.0f;
    }
    
    // Compute FFT (log2(128) = 7)
    fft_compute(fft_real, fft_imag, 7);
    
    // Compute magnitude spectrum (only first half, up to Nyquist)
    for (size_t i = 0; i < FFT_OUTPUT_SIZE; i++) {
        float magnitude = sqrtf(fft_real[i] * fft_real[i] + fft_imag[i] * fft_imag[i]);
        // Normalize and convert to dB scale (with minimum to avoid log(0))
        float magnitude_db = 20.0f * log10f(magnitude + 1e-10f);
        // Normalize to 0-1 range (assuming -60dB to 0dB range)
        output_magnitude[i] = (magnitude_db + 60.0f) / 60.0f;
        if (output_magnitude[i] < 0.0f) output_magnitude[i] = 0.0f;
        if (output_magnitude[i] > 1.0f) output_magnitude[i] = 1.0f;
    }
    
    return ESP_OK;
}

void fft_analyzer_cleanup(void) {
    if (fft_real != NULL) {
        free(fft_real);
        fft_real = NULL;
    }
    
    if (fft_imag != NULL) {
        free(fft_imag);
        fft_imag = NULL;
    }
    
    fft_initialized = false;
    ESP_LOGI(TAG, "FFT analyzer cleaned up");
}

