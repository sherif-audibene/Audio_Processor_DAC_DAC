#ifndef CPU_MONITOR_H
#define CPU_MONITOR_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief CPU stats structure for display
 */
typedef struct {
    float core0_usage;      // Core 0 usage percentage (0-100)
    float core1_usage;      // Core 1 usage percentage (0-100)
    float combined_usage;   // Combined/average usage
    uint32_t free_heap;     // Free heap in bytes
    uint32_t min_free_heap; // Minimum free heap since boot
} cpu_stats_t;

/**
 * @brief Initialize CPU monitor
 * @return ESP_OK on success
 */
esp_err_t cpu_monitor_init(void);

/**
 * @brief Print CPU usage stats to console
 * Shows per-task CPU usage and per-core idle percentage
 */
void cpu_monitor_print_stats(void);

/**
 * @brief Get CPU usage for a specific core
 * @param core_id 0 or 1
 * @return Usage percentage (0-100), or -1 on error
 */
float cpu_monitor_get_core_usage(int core_id);

/**
 * @brief Get all CPU stats in a structure (for LCD display)
 * @param stats Pointer to stats structure to fill
 * @return ESP_OK on success
 */
esp_err_t cpu_monitor_get_stats(cpu_stats_t *stats);

/**
 * @brief Start periodic CPU monitoring (prints every N seconds)
 * @param interval_sec Interval in seconds between prints
 */
void cpu_monitor_start_periodic(int interval_sec);

/**
 * @brief Stop periodic monitoring
 */
void cpu_monitor_stop_periodic(void);

#endif // CPU_MONITOR_H

