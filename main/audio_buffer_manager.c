#include "audio_buffer_manager.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "AUDIO_BUFFER_MGR";

// Private variables
static int32_t *audio_buffer = NULL;
static size_t buffer_size = 0;
static bool buffer_initialized = false;

esp_err_t audio_buffer_manager_init(void) {
    if (buffer_initialized) {
        ESP_LOGW(TAG, "Audio buffer already initialized");
        return ESP_OK;
    }

    // Calculate buffer size (24-bit samples stored in 32-bit containers)
    buffer_size = BUFFER_SIZE * CHANNELS * sizeof(int32_t);
    
    audio_buffer = (int32_t *)malloc(buffer_size);
    if (audio_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer (%u bytes)", (unsigned int)buffer_size);
        return ESP_ERR_NO_MEM;
    }

    buffer_initialized = true;
    ESP_LOGI(TAG, "Audio buffer allocated: %u bytes (%u KB)", 
             (unsigned int)buffer_size, (unsigned int)(buffer_size / 1024));
    return ESP_OK;
}

int32_t* audio_buffer_manager_get_buffer(void) {
    return audio_buffer;
}

size_t audio_buffer_manager_get_buffer_size(void) {
    return buffer_size;
}

void audio_buffer_manager_cleanup(void) {
    if (audio_buffer != NULL) {
        free(audio_buffer);
        audio_buffer = NULL;
        buffer_size = 0;
        buffer_initialized = false;
        ESP_LOGI(TAG, "Audio buffer freed");
    }
}

