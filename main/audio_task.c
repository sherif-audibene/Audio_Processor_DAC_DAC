#include "audio_task.h"
#include "audio_effects.h"
#include "audio_buffer_manager.h"
#include "esp_err.h"
#include "i2s_config.h"
#include "lcd_task.h"
#include "led_control.h"
#include "esp_log.h"
#include "driver/i2s.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "AUDIO_TASK";

// Private variables
static TaskHandle_t audio_io_task_handle = NULL;
static TaskHandle_t audio_process_task_handle = NULL;

// Double buffer state
static volatile int io_buffer_idx = BUFFER_A;      // Buffer being used for I/O
static volatile int process_buffer_idx = BUFFER_B; // Buffer being processed
static volatile size_t process_sample_count = 0;
static volatile bool process_pending = false;

// Synchronization
static SemaphoreHandle_t buffer_swap_mutex = NULL;
static SemaphoreHandle_t process_done_sem = NULL;

/**
 * @brief Audio effects processing task (runs on Core 0)
 * Processes audio effects while I/O task handles ADC/DAC
 */
static void audio_process_task(void *pvParameters) {
    ESP_LOGI(TAG, "Audio process task started on Core %d", xPortGetCoreID());
    
    while (1) {
        // Wait for notification that there's data to process
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        if (process_pending && process_sample_count > 0) {
            int32_t *buffer = audio_buffer_manager_get_buffer_by_index(process_buffer_idx);
            
            // Process audio effects on this buffer
            audio_effects_process(buffer, process_sample_count);
            
            // Update LEDs based on audio intensity
            led_control_update(buffer, process_sample_count);
            
            process_pending = false;
        }
        
        // Signal that processing is complete
        xSemaphoreGive(process_done_sem);
    }
}

/**
 * @brief Audio I/O task implementation (runs on Core 1)
 * Handles time-critical I2S read/write operations
 */
static void audio_io_task(void *pvParameters) {
    size_t bytes_read;
    size_t bytes_written;
    esp_err_t ret;
    
    const size_t buffer_size = audio_buffer_manager_get_buffer_size();
    bool first_frame = true;

    ESP_LOGI(TAG, "Audio I/O task started on Core %d", xPortGetCoreID());

    // Initialize I2S peripherals
    if (i2s_adc_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S ADC");
        vTaskDelete(NULL);
        return;
    }
    
    if (i2s_dac_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S DAC");
        i2s_adc_cleanup();
        vTaskDelete(NULL);
        return;
    }

    // Start I2S peripherals
    if (i2s_adc_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start I2S ADC");
        i2s_dac_cleanup();
        i2s_adc_cleanup();
        vTaskDelete(NULL);
        return;
    }
    
    if (i2s_dac_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start I2S DAC");
        i2s_stop(I2S_ADC_NUM);
        i2s_dac_cleanup();
        i2s_adc_cleanup();
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Dual-core audio pipeline started successfully");

    // Main audio I/O loop
    while (1) {
        int32_t *io_buffer = audio_buffer_manager_get_buffer_by_index(io_buffer_idx);
        int32_t *output_buffer = audio_buffer_manager_get_buffer_by_index(process_buffer_idx);
        
        // Read from ADC into current I/O buffer
        ret = i2s_read(I2S_ADC_NUM, io_buffer, buffer_size, &bytes_read, 
                      pdMS_TO_TICKS(AUDIO_READ_TIMEOUT_MS));

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S ADC read failed: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (bytes_read == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Wait for previous processing to complete (if not first frame)
        if (!first_frame) {
            xSemaphoreTake(process_done_sem, pdMS_TO_TICKS(50));
            
            // Write previously processed buffer to DAC
            ret = i2s_write(I2S_NUM, output_buffer, bytes_read, &bytes_written, 
                           pdMS_TO_TICKS(AUDIO_WRITE_TIMEOUT_MS));

            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "I2S DAC write failed: %s", esp_err_to_name(ret));
            } else if (bytes_written != bytes_read) {
                ESP_LOGW(TAG, "Partial write: %u/%u bytes", 
                        (unsigned int)bytes_written, (unsigned int)bytes_read);
            }
        }
        first_frame = false;
        
        // Swap buffers
        xSemaphoreTake(buffer_swap_mutex, portMAX_DELAY);
        int temp = io_buffer_idx;
        io_buffer_idx = process_buffer_idx;
        process_buffer_idx = temp;
        process_sample_count = bytes_read / sizeof(int32_t);
        process_pending = true;
        xSemaphoreGive(buffer_swap_mutex);
        
        // Notify processing task that there's work to do
        xTaskNotifyGive(audio_process_task_handle);
    }
}

esp_err_t audio_task_start(void) {
    if (audio_io_task_handle != NULL) {
        ESP_LOGW(TAG, "Audio task already running");
        return ESP_OK;
    }

    // Create synchronization primitives
    buffer_swap_mutex = xSemaphoreCreateMutex();
    if (buffer_swap_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create buffer swap mutex");
        return ESP_FAIL;
    }
    
    process_done_sem = xSemaphoreCreateBinary();
    if (process_done_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create process done semaphore");
        vSemaphoreDelete(buffer_swap_mutex);
        buffer_swap_mutex = NULL;
        return ESP_FAIL;
    }
    
    // Give initial token so first frame doesn't block
    xSemaphoreGive(process_done_sem);

    // Create processing task on Core 0 first
    BaseType_t ret = xTaskCreatePinnedToCore(
        audio_process_task, 
        "audio_process", 
        AUDIO_PROCESS_TASK_STACK_SIZE, 
        NULL, 
        AUDIO_PROCESS_TASK_PRIORITY, 
        &audio_process_task_handle,
        AUDIO_PROCESS_CORE
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio process task");
        vSemaphoreDelete(buffer_swap_mutex);
        vSemaphoreDelete(process_done_sem);
        buffer_swap_mutex = NULL;
        process_done_sem = NULL;
        return ESP_FAIL;
    }

    // Create I/O task on Core 1
    ret = xTaskCreatePinnedToCore(
        audio_io_task, 
        "audio_io", 
        AUDIO_TASK_STACK_SIZE, 
        NULL, 
        AUDIO_TASK_PRIORITY, 
        &audio_io_task_handle,
        AUDIO_IO_CORE
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio I/O task");
        vTaskDelete(audio_process_task_handle);
        audio_process_task_handle = NULL;
        vSemaphoreDelete(buffer_swap_mutex);
        vSemaphoreDelete(process_done_sem);
        buffer_swap_mutex = NULL;
        process_done_sem = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Dual-core audio tasks started (I/O: Core %d, Process: Core %d)", 
             AUDIO_IO_CORE, AUDIO_PROCESS_CORE);
    return ESP_OK;
}

void audio_task_stop(void) {
    if (audio_io_task_handle != NULL) {
        vTaskDelete(audio_io_task_handle);
        audio_io_task_handle = NULL;
    }
    if (audio_process_task_handle != NULL) {
        vTaskDelete(audio_process_task_handle);
        audio_process_task_handle = NULL;
    }
    if (buffer_swap_mutex != NULL) {
        vSemaphoreDelete(buffer_swap_mutex);
        buffer_swap_mutex = NULL;
    }
    if (process_done_sem != NULL) {
        vSemaphoreDelete(process_done_sem);
        process_done_sem = NULL;
    }
    ESP_LOGI(TAG, "Audio tasks stopped");
}

bool audio_task_is_running(void) {
    return (audio_io_task_handle != NULL && audio_process_task_handle != NULL);
}

