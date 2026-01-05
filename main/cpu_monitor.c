#include "cpu_monitor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include <string.h>

static const char *TAG = "CPU_MONITOR";

// Idle tick counters per core
static volatile uint32_t idle_tick_count[2] = {0, 0};
static uint32_t last_idle_count[2] = {0, 0};
static int64_t last_time_us = 0;

// Periodic monitoring
static esp_timer_handle_t monitor_timer = NULL;

/**
 * @brief Idle hook for Core 0
 */
static bool idle_hook_core0(void) {
    idle_tick_count[0]++;
    return false;  // Don't skip idle task
}

/**
 * @brief Idle hook for Core 1
 */
static bool idle_hook_core1(void) {
    idle_tick_count[1]++;
    return false;
}

esp_err_t cpu_monitor_init(void) {
    // Register idle hooks for both cores
    esp_err_t ret = esp_register_freertos_idle_hook_for_cpu(idle_hook_core0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register idle hook for Core 0");
        return ret;
    }
    
    ret = esp_register_freertos_idle_hook_for_cpu(idle_hook_core1, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register idle hook for Core 1");
        return ret;
    }
    
    last_time_us = esp_timer_get_time();
    ESP_LOGI(TAG, "CPU monitor initialized");
    return ESP_OK;
}

// Cached CPU usage values (updated by print_stats or get_stats)
static float cached_cpu_usage[2] = {0.0f, 0.0f};

static void update_cpu_usage_internal(void) {
    int64_t now = esp_timer_get_time();
    int64_t elapsed_us = now - last_time_us;
    
    if (elapsed_us < 100000) {  // Need at least 100ms of data
        return;
    }
    
    float elapsed_sec = (float)elapsed_us / 1000000.0f;
    
    for (int i = 0; i < 2; i++) {
        uint32_t delta = idle_tick_count[i] - last_idle_count[i];
        float rate = (float)delta / elapsed_sec;
        
        // Baseline calibration (adjust if needed)
        const float baseline = 3000000.0f;
        
        float usage = 100.0f - (rate / baseline * 100.0f);
        if (usage < 0) usage = 0;
        if (usage > 100) usage = 100;
        
        cached_cpu_usage[i] = usage;
        last_idle_count[i] = idle_tick_count[i];
    }
    
    last_time_us = now;
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

