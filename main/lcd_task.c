#include "lcd_task.h"
#include "lcd_display.h"
#include "fft_analyzer.h"
#include "i2s_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

static const char *TAG = "LCD_TASK";

// ============================================================================
// FFT FREQUENCY MAPPING
// The effective sample rate for FFT frequency calculation
// Adjust this multiplier if the frequency display is off
// ============================================================================
#define FFT_EFFECTIVE_SAMPLE_RATE  (SAMPLE_RATE)  // Use full sample rate

// Task handle
static TaskHandle_t lcd_task_handle = NULL;

// Configuration mutex
static SemaphoreHandle_t config_mutex = NULL;
static waveform_config_t current_waveform_config;

// Direct audio buffer access (no queue, no copying!)
static int32_t *audio_source_buffer = NULL;
static size_t audio_source_size = 0;
static SemaphoreHandle_t audio_buffer_mutex = NULL;

// Waveform buffer for display
#define MAX_WAVEFORM_SAMPLES 4000
static int16_t waveform_buffer[MAX_WAVEFORM_SAMPLES];
static size_t waveform_buffer_index = 0;
static bool waveform_buffer_filled = false;  // Track if we have valid samples

// FFT spectrum buffer
static float spectrum_magnitude[FFT_OUTPUT_SIZE];      // Raw FFT output
static float spectrum_smoothed[FFT_OUTPUT_SIZE];       // Smoothed for display
static bool spectrum_valid = false;

// Spectrum smoothing parameters
#define SPECTRUM_SMOOTHING_FACTOR 0.3f   // Lower = smoother (0.1-0.5 typical)
#define SPECTRUM_DECAY_FACTOR 0.92f       // Peak decay rate (0.9-0.98 typical)

// Message display
#define MAX_MESSAGE_LEN 32
static char message_buffer[MAX_MESSAGE_LEN] = {0};
static TickType_t message_end_time = 0;
static SemaphoreHandle_t message_mutex = NULL;
#define MESSAGE_DISPLAY_TIME_MS 3000  // Show message for 3 seconds

/**
 * @brief Convert 24-bit I2S sample to 16-bit for display
 */
static inline int16_t convert_sample_to_display(int32_t sample) {
    // I2S samples are 24-bit in 32-bit container, MSB aligned
    // Shift right by 8 to get 24-bit value, then scale to 16-bit
    return (int16_t)((sample >> 16) & 0xFFFF);
}

/**
 * @brief Draw grid on display
 */
static void draw_grid(void) {
    // Vertical lines every 32 pixels
    for (uint8_t x = 32; x < LCD_WIDTH; x += 32) {
        for (uint8_t y = 0; y < LCD_HEIGHT; y += 4) {
            lcd_display_set_pixel(x, y, 1);
        }
    }
    
    // Horizontal lines every 16 pixels
    for (uint8_t y = 16; y < LCD_HEIGHT; y += 16) {
        for (uint8_t x = 0; x < LCD_WIDTH; x += 4) {
            lcd_display_set_pixel(x, y, 1);
        }
    }
}

/**
 * @brief Draw center reference line
 */
static void draw_center_line(void) {
    uint8_t center_y = LCD_HEIGHT / 2;
    for (uint8_t x = 0; x < LCD_WIDTH; x += 2) {
        lcd_display_set_pixel(x, center_y, 1);
    }
}

/**
 * @brief Simple 5x7 font data for basic characters
 * Each character is 5 pixels wide, 7 pixels tall
 */
static const uint8_t font_5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // space (0x20)
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x00, 0x07, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x08, 0x2A, 0x1C, 0x2A, 0x08}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x59, 0x51, 0x3E}, // @
    {0x7C, 0x12, 0x11, 0x12, 0x7C}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
};

/**
 * @brief Draw a single character using 5x7 font
 */
static void draw_char(uint8_t x, uint8_t y, char c, uint8_t color) {
    if (c < 0x20 || c > 0x5A) {
        c = 0x20; // Use space for unsupported characters
    }
    
    const uint8_t *char_data = font_5x7[c - 0x20];
    
    for (uint8_t col = 0; col < 5; col++) {
        uint8_t col_data = char_data[col];
        for (uint8_t row = 0; row < 7; row++) {
            if (col_data & (1 << row)) {
                lcd_display_set_pixel(x + col, y + row, color);
            }
        }
    }
}

/**
 * @brief Draw a text string
 */
static void draw_text(uint8_t x, uint8_t y, const char *text, uint8_t color) {
    uint8_t pos_x = x;
    while (*text && pos_x < LCD_WIDTH - 5) {
        draw_char(pos_x, y, *text, color);
        pos_x += 6; // 5 pixels for char + 1 pixel spacing
        text++;
    }
}

/**
 * @brief Draw message overlay on display
 */
static void draw_message_overlay(void) {
    if (message_mutex == NULL) {
        return;
    }
    
    TickType_t current_time = xTaskGetTickCount();
    
    xSemaphoreTake(message_mutex, portMAX_DELAY);
    bool show_message = (current_time < message_end_time) && (message_buffer[0] != '\0');
    char msg_copy[MAX_MESSAGE_LEN];
    if (show_message) {
        strncpy(msg_copy, message_buffer, sizeof(msg_copy) - 1);
        msg_copy[sizeof(msg_copy) - 1] = '\0';
    }
    xSemaphoreGive(message_mutex);
    
    if (show_message) {
        // Draw message box background
        lcd_display_fill_rect(10, 20, 108, 24, 0); // Black background
        lcd_display_draw_rect(10, 20, 108, 24, 1); // White border
        
        // Draw text (white on black)
        draw_text(15, 26, msg_copy, 1);
    }
}

/**
 * @brief Calculate frequency for a given FFT bin index
 * @param bin_index FFT bin index (0 to FFT_OUTPUT_SIZE-1)
 * @return Frequency in Hz
 */
static uint32_t calculate_frequency_for_bin(uint8_t bin_index) {
    // Frequency per bin = sample_rate / FFT_SIZE
    // Each bin represents a frequency range
    float freq_per_bin = (float)FFT_EFFECTIVE_SAMPLE_RATE / (float)FFT_SIZE;
    return (uint32_t)(bin_index * freq_per_bin);
}

/**
 * @brief Draw frequency legend below spectral analyzer
 * Shows frequency range from 100 Hz to 16 kHz
 */
static void draw_frequency_legend(void) {
    // Legend area: bottom 10 pixels (y: 54-63)
    const uint8_t legend_y = LCD_HEIGHT - 10;
    
    // Frequency range to display
    const uint32_t freq_min = 100;   // 100 Hz
    const uint32_t freq_max = 16000; // 16 kHz
    
    // Draw separator line above legend
    for (uint8_t x = 0; x < LCD_WIDTH; x++) {
        lcd_display_set_pixel(x, legend_y - 1, 1);
    }
    
    // Calculate frequency ranges for key points in the 100 Hz - 16 kHz range
    // Show labels at: 100, 1k, 2k, 4k, 8k, 16k Hz
    uint32_t label_frequencies[] = {100, 1000, 2000, 4000, 8000, 16000};
    uint8_t num_labels = sizeof(label_frequencies) / sizeof(label_frequencies[0]);
    
    // Calculate x positions for labels based on frequency range
    float freq_range = (float)(freq_max - freq_min);
    float freq_per_pixel = freq_range / (float)(LCD_WIDTH - 1);
    uint8_t last_text_end = 0;  // Track last text position to avoid overlap
    
    for (uint8_t i = 0; i < num_labels; i++) {
        uint32_t freq = label_frequencies[i];
        if (freq < freq_min || freq > freq_max) continue;  // Skip if outside range
        
        // Calculate x position for tick mark
        // Map frequency to pixel: x = (freq - freq_min) / freq_per_pixel
        float x_pos_float = (float)(freq - freq_min) / freq_per_pixel;
        uint8_t x_pos = (uint8_t)(x_pos_float + 0.5f);  // Round to nearest
        if (x_pos >= LCD_WIDTH) continue;
        
        // Draw tick mark
        for (uint8_t y = legend_y; y < legend_y + 3; y++) {
            lcd_display_set_pixel(x_pos, y, 1);
        }
        
        // Draw frequency label (format: "100", "1k", "2k", etc.)
        char label[8];
        if (freq >= 1000) {
            snprintf(label, sizeof(label), "%uk", (unsigned int)(freq / 1000));
        } else {
            snprintf(label, sizeof(label), "%u", (unsigned int)freq);
        }
        
        // Calculate text width and position
        uint8_t text_width = strlen(label) * 6;
        uint8_t text_x;
        
        if (i == 0) {
            // First label: align left with tick
            text_x = x_pos;
        } else {
            // Center text above tick mark, but avoid overlap
            text_x = (x_pos >= text_width / 2) ? x_pos - text_width / 2 : 0;
            if (text_x < last_text_end + 2) {
                // Shift right if would overlap
                text_x = last_text_end + 2;
            }
        }
        
        // Ensure text doesn't go off screen
        if (text_x + text_width > LCD_WIDTH) {
            text_x = LCD_WIDTH - text_width;
        }
        
        // Only draw if we have space
        if (text_x + text_width <= LCD_WIDTH && text_x >= last_text_end) {
            draw_text(text_x, legend_y + 4, label, 1);
            last_text_end = text_x + text_width;
        }
    }
    
    // Draw Hz unit label at the end if there's space
    if (last_text_end < LCD_WIDTH - 12) {
        draw_text(LCD_WIDTH - 12, legend_y + 4, "Hz", 1);
    }
}

/**
 * @brief Draw spectral analyzer on display
 * Shows frequency range from 100 Hz to 16 kHz
 */
static void draw_spectral_analyzer(void) {
    if (!spectrum_valid) {
        return;  // No valid spectrum data yet
    }
    
    // Reserve space for legend at bottom (10 pixels)
    const uint8_t legend_height = 10;
    const uint8_t spectrum_height = LCD_HEIGHT - legend_height;
    const uint8_t spectrum_bottom = LCD_HEIGHT - legend_height - 1;
    
    // Frequency range to display
    const uint32_t freq_min = 100;   // 100 Hz
    const uint32_t freq_max = 16000; // 16 kHz
    
    // Calculate frequency per bin and per pixel
    // With FFT_SIZE=256 and effective rate=96kHz: 96000/256 = 375 Hz per bin
    float freq_per_bin = (float)FFT_EFFECTIVE_SAMPLE_RATE / (float)FFT_SIZE;
    float freq_range = (float)(freq_max - freq_min);
    float freq_per_pixel = freq_range / (float)(LCD_WIDTH - 1);
    
    // Draw spectrum bars - map each pixel to a frequency in the 100 Hz - 16 kHz range
    for (uint8_t x = 0; x < LCD_WIDTH; x++) {
        // Calculate frequency for this pixel
        float freq = (float)freq_min + (float)x * freq_per_pixel;
        
        // Find corresponding FFT bin (interpolate between bins)
        float bin_float = freq / freq_per_bin;
        uint8_t bin_low = (uint8_t)bin_float;
        uint8_t bin_high = bin_low + 1;
        float bin_fraction = bin_float - (float)bin_low;
        
        // Get magnitude from smoothed FFT bins (with interpolation if needed)
        float magnitude = 0.0f;
        if (bin_low < FFT_OUTPUT_SIZE) {
            if (bin_high < FFT_OUTPUT_SIZE && bin_fraction > 0.01f) {
                // Interpolate between two bins (only if fraction is significant)
                magnitude = spectrum_smoothed[bin_low] * (1.0f - bin_fraction) + 
                           spectrum_smoothed[bin_high] * bin_fraction;
            } else {
                // Use only lower bin
                magnitude = spectrum_smoothed[bin_low];
            }
        } else {
            // Clamp to last available bin if beyond range
            magnitude = spectrum_smoothed[FFT_OUTPUT_SIZE - 1];
        }
        
        // Calculate bar height from magnitude (0.0 to 1.0)
        uint8_t bar_height = (uint8_t)(magnitude * spectrum_height);
        
        // Clamp bar height
        if (bar_height > spectrum_height) {
            bar_height = spectrum_height;
        }
        
        // Draw bar from bottom up
        uint8_t y_bottom = spectrum_bottom;
        uint8_t y_top = y_bottom - bar_height;
        
        // Draw vertical line for this pixel
        for (uint8_t y = y_top; y <= y_bottom; y++) {
            lcd_display_set_pixel(x, y, 1);
        }
    }
    
    // Draw baseline at bottom of spectrum area
    for (uint8_t x = 0; x < LCD_WIDTH; x += 2) {
        lcd_display_set_pixel(x, spectrum_bottom, 1);
    }
    
    // Draw frequency legend
    draw_frequency_legend();
}

/**
 * @brief Draw waveform on display
 */
static uint32_t draw_count = 0;
static void draw_waveform(void) {
    
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    waveform_config_t config = current_waveform_config;
    xSemaphoreGive(config_mutex);
    
    // Calculate how many samples to display
    // Once buffer is filled, we always have enough samples (it's circular)
    uint16_t samples_to_display = config.samples_per_screen;
    if (samples_to_display > MAX_WAVEFORM_SAMPLES) {
        samples_to_display = MAX_WAVEFORM_SAMPLES;
    }
    
    // If buffer is not yet completely filled (rare edge case at startup)
    if (!waveform_buffer_filled && samples_to_display > waveform_buffer_index) {
        samples_to_display = waveform_buffer_index;
    }
    
    
    // Calculate decimation factor
    float x_step = (float)LCD_WIDTH / samples_to_display;
    
    // Draw the waveform
    uint8_t center_y = LCD_HEIGHT / 2;
    
    for (uint16_t i = 0; i < samples_to_display - 1; i++) {
        // Get sample from circular buffer
        uint16_t index1 = (waveform_buffer_index - samples_to_display + i) % MAX_WAVEFORM_SAMPLES;
        uint16_t index2 = (waveform_buffer_index - samples_to_display + i + 1) % MAX_WAVEFORM_SAMPLES;
        
        int16_t sample1 = waveform_buffer[index1];
        int16_t sample2 = waveform_buffer[index2];
        
        // Apply amplitude scaling
        sample1 = (sample1 * config.amplitude_scale) / 100;
        sample2 = (sample2 * config.amplitude_scale) / 100;
        
        // Convert to display coordinates
        // Scale to fit display height
        int16_t y1 = center_y - ((sample1 * (LCD_HEIGHT / 2)) / 32768);
        int16_t y2 = center_y - ((sample2 * (LCD_HEIGHT / 2)) / 32768);
        
        // Clamp to display bounds
        if (y1 < 0) y1 = 0;
        if (y1 >= LCD_HEIGHT) y1 = LCD_HEIGHT - 1;
        if (y2 < 0) y2 = 0;
        if (y2 >= LCD_HEIGHT) y2 = LCD_HEIGHT - 1;
        
        // Calculate X positions
        uint8_t x1 = (uint8_t)(i * x_step);
        uint8_t x2 = (uint8_t)((i + 1) * x_step);
        
        if (x1 >= LCD_WIDTH) x1 = LCD_WIDTH - 1;
        if (x2 >= LCD_WIDTH) x2 = LCD_WIDTH - 1;
        
        // Draw line between points
        lcd_display_draw_line(x1, y1, x2, y2, 1);
    }
}

/**
 * @brief Process audio data directly from audio buffer and update waveform buffer
 */
static uint32_t debug_sample_count = 0;
static void process_audio_data_from_source(void) {    
    // Take mutex to safely read from audio buffer
    if (xSemaphoreTake(audio_buffer_mutex, pdMS_TO_TICKS(5)) != pdTRUE) {
        return;  // Skip this update if buffer is busy
    }
    
    size_t sample_count = audio_source_size / sizeof(int32_t);
    
    // Get current display mode
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    waveform_mode_t display_mode = current_waveform_config.mode;
    xSemaphoreGive(config_mutex);
    
    // Prepare samples for FFT if in spectrum mode
    if (display_mode == WAVEFORM_MODE_SPECTRUM) {
        // Collect FFT_SIZE samples (left channel only)
        static int16_t fft_samples[FFT_SIZE];
        static size_t fft_sample_index = 0;
        
        // Collect samples for FFT (left channel only, every other sample)
        for (size_t i = 0; i < sample_count && fft_sample_index < FFT_SIZE; i += 2) {
            fft_samples[fft_sample_index] = convert_sample_to_display(audio_source_buffer[i]);
            fft_sample_index++;
            
            // When we have enough samples, compute FFT
            if (fft_sample_index >= FFT_SIZE) {
                if (fft_analyzer_compute(fft_samples, FFT_SIZE, spectrum_magnitude) == ESP_OK) {
                    // Apply smoothing: exponential moving average with peak hold
                    for (size_t j = 0; j < FFT_OUTPUT_SIZE; j++) {
                        float new_val = spectrum_magnitude[j];
                        float old_val = spectrum_smoothed[j];
                        
                        if (new_val > old_val) {
                            // Rising: use faster attack
                            spectrum_smoothed[j] = old_val + SPECTRUM_SMOOTHING_FACTOR * (new_val - old_val);
                        } else {
                            // Falling: use slower decay for smooth drop
                            spectrum_smoothed[j] = old_val * SPECTRUM_DECAY_FACTOR;
                            // But don't let it go below the new value
                            if (spectrum_smoothed[j] < new_val) {
                                spectrum_smoothed[j] = new_val;
                            }
                        }
                    }
                    spectrum_valid = true;
                }
                // Keep last half of samples for overlap (better frequency resolution)
                memmove(fft_samples, fft_samples + FFT_SIZE / 2, (FFT_SIZE / 2) * sizeof(int16_t));
                fft_sample_index = FFT_SIZE / 2;  // Start from middle for overlap
            }
        }
    } else {
        // Reset FFT collection when not in spectrum mode
        spectrum_valid = false;
    }
    
    // Decimate samples to fit into waveform buffer
    // Take only left channel (every other sample for stereo)
    size_t decimation = 1;
    if (sample_count > MAX_WAVEFORM_SAMPLES * 2) {
        decimation = sample_count / (MAX_WAVEFORM_SAMPLES * 2);
    }
    
    
    size_t samples_added = 0;
    for (size_t i = 0; i < sample_count && i < audio_source_size / sizeof(int32_t); i += (decimation * 2)) {
        // Convert and store sample (left channel only)
        int16_t converted = convert_sample_to_display(audio_source_buffer[i]);
        waveform_buffer[waveform_buffer_index] = converted;
        waveform_buffer_index = (waveform_buffer_index + 1) % MAX_WAVEFORM_SAMPLES;
        samples_added++;
    }
    
    // Mark buffer as filled once we've wrapped around at least once, or have enough samples
    if (samples_added > 0) {
        // If we added samples and now index is small, we must have wrapped
        if (waveform_buffer_index < samples_added) {
            waveform_buffer_filled = true;  // Wrapped around = buffer full
        } else if (waveform_buffer_index >= 128) {
            waveform_buffer_filled = true;  // Half full is good enough
        }
    }
    
    xSemaphoreGive(audio_buffer_mutex);
}

/**
 * @brief LCD display task implementation
 */
static void lcd_display_task(void *pvParameters) {
    ESP_LOGI(TAG, "LCD display task started");
    
    TickType_t last_update_time = xTaskGetTickCount();
    const TickType_t update_interval = pdMS_TO_TICKS(1000 / LCD_UPDATE_RATE_HZ);
    
    uint32_t loop_count = 0;
    
    while (1) {
        
        // Process audio data directly from source buffer
        process_audio_data_from_source();
        
        // Update display at fixed rate
        TickType_t current_time = xTaskGetTickCount();
        if ((current_time - last_update_time) >= update_interval) {
            last_update_time = current_time;
            
            // Clear display
            memset(lcd_display_get_framebuffer(), 0, LCD_WIDTH * LCD_PAGES);
            
            xSemaphoreTake(config_mutex, portMAX_DELAY);
            bool show_grid = current_waveform_config.show_grid;
            bool show_center = current_waveform_config.show_center_line;
            xSemaphoreGive(config_mutex);
            
            // Draw grid if enabled
            if (show_grid) {
                draw_grid();
            }
            
            // Get display mode
            xSemaphoreTake(config_mutex, portMAX_DELAY);
            waveform_mode_t display_mode = current_waveform_config.mode;
            xSemaphoreGive(config_mutex);
            
            // Draw based on mode
            if (display_mode == WAVEFORM_MODE_SPECTRUM) {
                // Draw spectral analyzer
                draw_spectral_analyzer();
            } else {
                // Draw center line if enabled (only for waveform mode)
                if (show_center) {
                    draw_center_line();
                }
                
                // Draw waveform
                draw_waveform();
            }
            
            // Draw message overlay if active
            draw_message_overlay();
            
            // Update display
            esp_err_t ret = lcd_display_update();
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Display update failed: %s", esp_err_to_name(ret));
            }
        }
        
        // Small delay to prevent tight loop
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

esp_err_t lcd_task_start(const lcd_task_config_t *config) {
    if (lcd_task_handle != NULL) {
        ESP_LOGW(TAG, "LCD task already running");
        return ESP_OK;
    }
    
    if (config == NULL) {
        ESP_LOGE(TAG, "Config is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Create mutex for configuration protection
    config_mutex = xSemaphoreCreateMutex();
    if (config_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create config mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Create mutex for audio buffer access
    audio_buffer_mutex = xSemaphoreCreateMutex();
    if (audio_buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create audio buffer mutex");
        vSemaphoreDelete(config_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Create mutex for message display
    message_mutex = xSemaphoreCreateMutex();
    if (message_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create message mutex");
        vSemaphoreDelete(audio_buffer_mutex);
        vSemaphoreDelete(config_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Store initial waveform configuration
    current_waveform_config = config->waveform;
    
    // Initialize LCD display
    lcd_config_t lcd_config = {
        .sda_pin = config->sda_pin,
        .scl_pin = config->scl_pin,
        .i2c_address = LCD_I2C_ADDRESS,
        .i2c_freq_hz = LCD_I2C_FREQ_HZ,
        .contrast = config->lcd_contrast,
        .flip_horizontal = false,
        .flip_vertical = false
    };
    
    esp_err_t ret = lcd_display_init(&lcd_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LCD display: %s", esp_err_to_name(ret));
        vSemaphoreDelete(audio_buffer_mutex);
        vSemaphoreDelete(config_mutex);
        return ret;
    }
    
    // Clear waveform buffer
    memset(waveform_buffer, 0, sizeof(waveform_buffer));
    waveform_buffer_index = 0;
    waveform_buffer_filled = false;
    
    // Initialize FFT analyzer
    ret = fft_analyzer_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize FFT analyzer: %s", esp_err_to_name(ret));
        // Continue anyway - waveform mode will still work
    }
    
    // Initialize spectrum buffers
    memset(spectrum_magnitude, 0, sizeof(spectrum_magnitude));
    memset(spectrum_smoothed, 0, sizeof(spectrum_smoothed));
    spectrum_valid = false;
    
    // Create LCD task
    BaseType_t task_ret = xTaskCreate(lcd_display_task, "lcd_display", 
                                     LCD_TASK_STACK_SIZE, NULL, 
                                     LCD_TASK_PRIORITY, &lcd_task_handle);
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LCD display task");
        lcd_display_cleanup();
        vSemaphoreDelete(audio_buffer_mutex);
        vSemaphoreDelete(config_mutex);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "LCD task started successfully");
    return ESP_OK;
}

void lcd_task_stop(void) {
    if (lcd_task_handle != NULL) {
        vTaskDelete(lcd_task_handle);
        lcd_task_handle = NULL;
        
        lcd_display_cleanup();
        
        if (audio_buffer_mutex != NULL) {
            vSemaphoreDelete(audio_buffer_mutex);
            audio_buffer_mutex = NULL;
        }
        
        if (message_mutex != NULL) {
            vSemaphoreDelete(message_mutex);
            message_mutex = NULL;
        }
        
        if (config_mutex != NULL) {
            vSemaphoreDelete(config_mutex);
            config_mutex = NULL;
        }
        
        // Cleanup FFT analyzer
        fft_analyzer_cleanup();
        
        audio_source_buffer = NULL;
        audio_source_size = 0;
        
        ESP_LOGI(TAG, "LCD task stopped");
    }
}

bool lcd_task_is_running(void) {
    return (lcd_task_handle != NULL);
}

esp_err_t lcd_task_set_waveform_config(const waveform_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (config_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    current_waveform_config = *config;
    xSemaphoreGive(config_mutex);
    
    ESP_LOGI(TAG, "Waveform config updated: samples=%d, amp_scale=%d%%", 
             config->samples_per_screen, config->amplitude_scale);
    
    return ESP_OK;
}

esp_err_t lcd_task_get_waveform_config(waveform_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (config_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    *config = current_waveform_config;
    xSemaphoreGive(config_mutex);
    
    return ESP_OK;
}

esp_err_t lcd_task_get_audio_source(int32_t **buffer, size_t *size) {
    if (buffer == NULL || size == NULL) {
        ESP_LOGE(TAG, "Invalid arguments: buffer=%p, size=%p", buffer, size);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (audio_buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Audio buffer mutex not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Set the audio source buffer pointer (no copying!)
    xSemaphoreTake(audio_buffer_mutex, portMAX_DELAY);
    audio_source_buffer = *buffer;
    audio_source_size = *size;
    xSemaphoreGive(audio_buffer_mutex);
    
    ESP_LOGI(TAG, "LCD task configured to read from audio buffer:");
    ESP_LOGI(TAG, "  - Buffer address: %p", audio_source_buffer);
    ESP_LOGI(TAG, "  - Buffer size: %d bytes (%d samples)", audio_source_size, audio_source_size / sizeof(int32_t));
    ESP_LOGI(TAG, "  - First sample: 0x%08X (%d)", (unsigned int)audio_source_buffer[0], (int)audio_source_buffer[0]);
    
    return ESP_OK;
}

esp_err_t lcd_task_set_contrast(uint8_t contrast) {
    return lcd_display_set_contrast(contrast);
}

esp_err_t lcd_task_show_message(const char *message) {
    if (message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (message_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(message_mutex, portMAX_DELAY);
    strncpy(message_buffer, message, MAX_MESSAGE_LEN - 1);
    message_buffer[MAX_MESSAGE_LEN - 1] = '\0';
    message_end_time = xTaskGetTickCount() + pdMS_TO_TICKS(MESSAGE_DISPLAY_TIME_MS);
    xSemaphoreGive(message_mutex);
    
    ESP_LOGI(TAG, "Displaying message: %s", message);
    return ESP_OK;
}

