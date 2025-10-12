#ifndef I2S_CONFIG_H
#define I2S_CONFIG_H

#include "esp_err.h"
#include "driver/i2s.h"

// I2S pin definitions
#define DAC_BCK_PIN 36
#define DAC_LRCK_PIN 37
#define DAC_DATA_PIN 38

// I2S ADC pin definitions (slave mode)
#define ADC_BCK_PIN 36
#define ADC_LRCK_PIN 37
#define ADC_DATA_PIN 35

// Audio configuration
#define SAMPLE_RATE 192000 
#define BITS_PER_SAMPLE (i2s_bits_per_sample_t) 24
#define BITS_PER_CHAN 32  // 24-bit samples transmitted in 32-bit slots
#define CHANNELS 2

// Buffer configuration
#define BUFFER_SIZE 256
#define NUM_BUFFERS 8

// I2S configuration
#define I2S_NUM I2S_NUM_0 // DAC (TX)
#define I2S_ADC_NUM I2S_NUM_1 // ADC (RX)

/**
 * @brief Initialize I2S peripheral for DAC in master mode
 * @return ESP_OK on success, error code on failure
 */
esp_err_t i2s_dac_init(void);

/**
 * @brief Initialize I2S peripheral for ADC in slave mode
 * @return ESP_OK on success, error code on failure
 */
esp_err_t i2s_adc_init(void);

/**
 * @brief Start I2S DAC
 * @return ESP_OK on success, error code on failure
 */
esp_err_t i2s_dac_start(void);

/**
 * @brief Start I2S ADC
 * @return ESP_OK on success, error code on failure
 */
esp_err_t i2s_adc_start(void);

/**
 * @brief Stop and cleanup I2S DAC
 */
void i2s_dac_cleanup(void);

/**
 * @brief Stop and cleanup I2S ADC
 */
void i2s_adc_cleanup(void);

#endif // I2S_CONFIG_H

