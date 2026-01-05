#include "audio_buffer_manager.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <stdlib.h>

static const char *TAG = "AUDIO_BUFFER_MGR";

// Private variables - double buffering
static int32_t *audio_buffer_a = NULL;
static int32_t *audio_buffer_b = NULL;
static size_t buffer_size = 0;
static bool buffer_initialized = false;

esp_err_t audio_buffer_manager_init(void) {
    if (buffer_initialized) {
        ESP_LOGW(TAG, "Audio buffer already initialized");
        return ESP_OK;
    }

    // Calculate buffer size (24-bit samples stored in 32-bit containers)
    buffer_size = BUFFER_SIZE * CHANNELS * sizeof(int32_t);
    
    // Allocate buffer A - try DMA-capable internal RAM first for I2S
    audio_buffer_a = (int32_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (audio_buffer_a == NULL) {
        audio_buffer_a = (int32_t *)malloc(buffer_size);
    }
    
    if (audio_buffer_a == NULL) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer A (%u bytes)", (unsigned int)buffer_size);
        return ESP_ERR_NO_MEM;
    }

    // Allocate buffer B
    audio_buffer_b = (int32_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (audio_buffer_b == NULL) {
        audio_buffer_b = (int32_t *)malloc(buffer_size);
    }
    
    if (audio_buffer_b == NULL) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer B (%u bytes)", (unsigned int)buffer_size);
        free(audio_buffer_a);
        audio_buffer_a = NULL;
        return ESP_ERR_NO_MEM;
    }

    buffer_initialized = true;
    ESP_LOGI(TAG, "Double audio buffers allocated: 2 x %u bytes (%u KB total)", 
             (unsigned int)buffer_size, (unsigned int)(buffer_size * 2 / 1024));
    return ESP_OK;
}

int32_t* audio_buffer_manager_get_buffer(void) {
    return audio_buffer_a;  // Legacy: return buffer A
}

int32_t* audio_buffer_manager_get_buffer_by_index(int index) {
    if (index == BUFFER_B) {
        return audio_buffer_b;
    }
    return audio_buffer_a;
}

size_t audio_buffer_manager_get_buffer_size(void) {
    return buffer_size;
}

void audio_buffer_manager_cleanup(void) {
    if (audio_buffer_a != NULL) {
        free(audio_buffer_a);
        audio_buffer_a = NULL;
    }
    if (audio_buffer_b != NULL) {
        free(audio_buffer_b);
        audio_buffer_b = NULL;
    }
    buffer_size = 0;
    buffer_initialized = false;
    ESP_LOGI(TAG, "Audio buffers freed");
}

