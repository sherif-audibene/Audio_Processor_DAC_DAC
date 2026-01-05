#include "cpu_monitor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "CPU_MONITOR";

// Periodic monitoring
static esp_timer_handle_t monitor_timer = NULL;

// Cached CPU usage values
static float cached_cpu_usage[2] = {0.0f, 0.0f};
static int64_t last_update_time = 0;

esp_err_t cpu_monitor_init(void) {
    last_update_time = esp_timer_get_time();
    ESP_LOGI(TAG, "CPU monitor initialized (using FreeRTOS runtime stats)");
    return ESP_OK;
}

/**
 * @brief Parse IDLE task percentages from FreeRTOS runtime stats
 */
static void update_cpu_usage_internal(void) {
    int64_t now = esp_timer_get_time();
    if (now - last_update_time < 1000000) {  // Update every 1 second max
        return;
    }
    last_update_time = now;
    
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    // Get runtime stats and parse IDLE percentages
    char *stats_buffer = malloc(2048);
    if (stats_buffer == NULL) {
        return;
    }
    
    vTaskGetRunTimeStats(stats_buffer);
    
    // Parse IDLE0 and IDLE1 percentages from the stats string
    // Format: "taskname\t\truntime\t\tpercentage%"
    char *line = stats_buffer;
    while (line && *line) {
        char *next_line = strchr(line, '\n');
        if (next_line) *next_line = '\0';
        
        // Look for IDLE0 and IDLE1 lines
        if (strstr(line, "IDLE0") != NULL) {
            // Find the percentage (last number before %)
            char *pct = strrchr(line, '%');
            if (pct) {
                // Walk back to find the number
                char *num_start = pct - 1;
                while (num_start > line && ((*num_start >= '0' && *num_start <= '9') || *num_start == '<')) {
                    num_start--;
                }
                num_start++;
                int idle_pct = atoi(num_start);
                if (idle_pct > 0 && idle_pct <= 100) {
                    cached_cpu_usage[0] = 100.0f - (float)idle_pct;
                }
            }
        } else if (strstr(line, "IDLE1") != NULL) {
            char *pct = strrchr(line, '%');
            if (pct) {
                char *num_start = pct - 1;
                while (num_start > line && ((*num_start >= '0' && *num_start <= '9') || *num_start == '<')) {
                    num_start--;
                }
                num_start++;
                int idle_pct = atoi(num_start);
                if (idle_pct > 0 && idle_pct <= 100) {
                    cached_cpu_usage[1] = 100.0f - (float)idle_pct;
                }
            }
        }
        
        if (next_line) {
            line = next_line + 1;
        } else {
            break;
        }
    }
    
    free(stats_buffer);
#else
    // Fallback: show that stats aren't available
    cached_cpu_usage[0] = -1.0f;
    cached_cpu_usage[1] = -1.0f;
#endif
}

float cpu_monitor_get_core_usage(int core_id) {
    if (core_id < 0 || core_id > 1) {
        return -1.0f;
    }
    
    update_cpu_usage_internal();
    return cached_cpu_usage[core_id];
}

esp_err_t cpu_monitor_get_stats(cpu_stats_t *stats) {
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    update_cpu_usage_internal();
    
    stats->core0_usage = cached_cpu_usage[0];
    stats->core1_usage = cached_cpu_usage[1];
    stats->combined_usage = (cached_cpu_usage[0] + cached_cpu_usage[1]) / 2.0f;
    stats->free_heap = esp_get_free_heap_size();
    stats->min_free_heap = esp_get_minimum_free_heap_size();
    
    return ESP_OK;
}

void cpu_monitor_print_stats(void) {
    cpu_stats_t stats;
    if (cpu_monitor_get_stats(&stats) != ESP_OK) {
        ESP_LOGW(TAG, "Not enough data yet, wait a moment...");
        return;
    }
    
    ESP_LOGI(TAG, "=== CPU Usage ===");
    ESP_LOGI(TAG, "  Core 0 (Process): %.1f%%", stats.core0_usage);
    ESP_LOGI(TAG, "  Core 1 (I/O):     %.1f%%", stats.core1_usage);
    ESP_LOGI(TAG, "  Combined:         %.1f%%", stats.combined_usage);
    ESP_LOGI(TAG, "  Free heap: %lu bytes (min: %lu)", 
             (unsigned long)stats.free_heap, (unsigned long)stats.min_free_heap);
    
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    // If runtime stats are enabled, print detailed task info
    char *task_stats = malloc(2048);
    if (task_stats) {
        vTaskGetRunTimeStats(task_stats);
        ESP_LOGI(TAG, "Task Runtime Stats:\n%s", task_stats);
        free(task_stats);
    }
#endif
}

static void periodic_timer_callback(void *arg) {
    cpu_monitor_print_stats();
}

void cpu_monitor_start_periodic(int interval_sec) {
    if (monitor_timer != NULL) {
        ESP_LOGW(TAG, "Periodic monitoring already running");
        return;
    }
    
    esp_timer_create_args_t timer_args = {
        .callback = periodic_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "cpu_monitor"
    };
    
    esp_err_t ret = esp_timer_create(&timer_args, &monitor_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create timer");
        return;
    }
    
    ret = esp_timer_start_periodic(monitor_timer, interval_sec * 1000000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start timer");
        esp_timer_delete(monitor_timer);
        monitor_timer = NULL;
        return;
    }
    
    ESP_LOGI(TAG, "Periodic CPU monitoring started (every %d sec)", interval_sec);
}

void cpu_monitor_stop_periodic(void) {
    if (monitor_timer != NULL) {
        esp_timer_stop(monitor_timer);
        esp_timer_delete(monitor_timer);
        monitor_timer = NULL;
        ESP_LOGI(TAG, "Periodic monitoring stopped");
    }
}

