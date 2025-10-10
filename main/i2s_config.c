#include "i2s_config.h"
#include "esp_log.h"
#include "esp_intr_alloc.h"

static const char *TAG = "I2S_CONFIG";

// Private state variables
static bool i2s_dac_initialized = false;
static bool i2s_adc_initialized = false;

esp_err_t i2s_dac_init(void) {
    if (i2s_dac_initialized) {
        return ESP_OK;
    }

    i2s_config_t i2s_config = {
        .mode = I2S_MODE_SLAVE | I2S_MODE_TX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = NUM_BUFFERS,
        .dma_buf_len = BUFFER_SIZE,
        .use_apll = true,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = DAC_BCK_PIN,
        .ws_io_num = DAC_LRCK_PIN,
        .data_out_num = DAC_DATA_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    esp_err_t ret = i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2S DAC driver: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_set_pin(I2S_NUM, &pin_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set I2S DAC pins: %s", esp_err_to_name(ret));
        i2s_driver_uninstall(I2S_NUM);
        return ret;
    }

    i2s_set_clk(I2S_NUM, SAMPLE_RATE, 16, I2S_CHANNEL_STEREO);
    
    ret = i2s_zero_dma_buffer(I2S_NUM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to zero DAC DMA buffer: %s", esp_err_to_name(ret));
        i2s_driver_uninstall(I2S_NUM);
        return ret;
    }

    i2s_dac_initialized = true;
    ESP_LOGI(TAG, "I2S DAC initialized successfully (Master)");
    return ESP_OK;
}

esp_err_t i2s_adc_init(void) {
    if (i2s_adc_initialized) {
        return ESP_OK;
    }

    i2s_config_t i2s_adc_config = {
        .mode = I2S_MODE_SLAVE | I2S_MODE_RX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = BITS_PER_SAMPLE,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = NUM_BUFFERS,
        .dma_buf_len = BUFFER_SIZE,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t slave_pin_config = {
        .bck_io_num = ADC_BCK_PIN,
        .ws_io_num = ADC_LRCK_PIN,
        .mck_io_num = -1,
        .data_out_num = -1,
        .data_in_num = ADC_DATA_PIN
    };

    esp_err_t ret = i2s_driver_install(I2S_ADC_NUM, &i2s_adc_config, 0, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2S ADC driver: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_set_pin(I2S_ADC_NUM, &slave_pin_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set I2S ADC pins: %s", esp_err_to_name(ret));
        i2s_driver_uninstall(I2S_ADC_NUM);
        return ret;
    }

    i2s_set_clk(I2S_ADC_NUM, SAMPLE_RATE, BITS_PER_SAMPLE, I2S_CHANNEL_STEREO);
    
    ret = i2s_zero_dma_buffer(I2S_ADC_NUM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to zero I2S ADC DMA buffer: %s", esp_err_to_name(ret));
        i2s_driver_uninstall(I2S_ADC_NUM);
        return ret;
    }

    i2s_adc_initialized = true;
    ESP_LOGI(TAG, "I2S ADC initialized successfully (Slave)");
    return ESP_OK;
}

esp_err_t i2s_dac_start(void) {
    if (!i2s_dac_initialized) {
        ESP_LOGE(TAG, "I2S DAC not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    return i2s_start(I2S_NUM);
}

esp_err_t i2s_adc_start(void) {
    if (!i2s_adc_initialized) {
        ESP_LOGE(TAG, "I2S ADC not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    return i2s_start(I2S_ADC_NUM);
}

void i2s_dac_cleanup(void) {
    if (i2s_dac_initialized) {
        i2s_stop(I2S_NUM);
        i2s_driver_uninstall(I2S_NUM);
        i2s_dac_initialized = false;
    }
}

void i2s_adc_cleanup(void) {
    if (i2s_adc_initialized) {
        i2s_stop(I2S_ADC_NUM);
        i2s_driver_uninstall(I2S_ADC_NUM);
        i2s_adc_initialized = false;
    }
}
