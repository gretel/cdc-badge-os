---
title: "[MEDIUM] No continuous resource utilization metrics"
severity: MEDIUM
domain: observability/metrics
lens: resource-metrics
labels:
  - "metrics"
  - "observability"
  - "memory"
  - "battery"
---

## Summary
Resource metrics (heap, PSRAM, battery, uptime) are only available via one-shot serial commands (STATUS, MEM). There is no continuous tracking, no historical data, and no way to monitor resource trends over time. Critical resources like battery level and memory pressure are not exposed as metrics.

**Evidence:**
- `components/serial_cmd/src/SerialCmd.cpp:424-456` - `cmdStatus()` and `cmdMem()` provide snapshot data only
- `components/cdc_hal/src/BQ25895Power.cpp` - Power manager has battery data but no metrics export
- `main/main.cpp:243` - Main loop calls `s_powerManager->update()` but no metrics collection
- No periodic metrics collection task or background exporter

## Impact
Without continuous resource metrics:
- **Memory leaks hard to detect**: Cannot track heap/PSRAM trends over days/weeks
- **Battery monitoring blind**: No way to track discharge rates or predict battery life
- **Capacity planning missing**: Cannot predict when memory will be exhausted
- **No proactive alerts**: Cannot alert before running out of memory or battery

## Evidence
**Current memory reporting** (`components/serial_cmd/src/SerialCmd.cpp:438-452`):
```cpp
static void cmdMem(const char* args) {
    (void)args;
    Console::printf("=== Memory Usage ===\r\n");
    Console::printf("Heap: %lu / %lu bytes free\r\n",
                   (unsigned long)esp_get_free_heap_size(),
                   (unsigned long)heap_caps_get_total_size(MALLOC_CAP_DEFAULT));

    size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psramTotal = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (psramTotal > 0) {
        Console::printf("PSRAM: %lu / %lu bytes free\r\n",
                       (unsigned long)psramFree,
                       (unsigned long)psramTotal);
    }
    Console::flush();
}
```
- Manual command only, no automation
- No historical data

**Battery data available but not tracked** (`main/main.cpp:120-122`):
```cpp
LOG_I(TAG, "Battery: %d%% (%dmV)", s_powerManager->getBatteryPercent(),
      s_powerManager->getBatteryVoltage());
```
- Logged at boot only
- No continuous monitoring

## Recommended Fix
1. **Add resource metrics** (`components/cdc_metrics/include/cdc_metrics.h`):
   ```cpp
   // Resource gauges
   #define METRIC_HEAP_FREE "heap_free_bytes"
   #define METRIC_PSRAM_FREE "psram_free_bytes"
   #define METRIC_BATTERY_PCT "battery_percent"
   #define METRIC_BATTERY_MV "battery_millivolts"
   #define METRIC_UPTIME_MS "uptime_ms"
   ```

2. **Create metrics collection task** (new file `components/cdc_metrics/src/MetricsCollector.cpp`):
   ```cpp
   void metricsCollectorTask(void* param) {
       while (true) {
           // Collect every 10 seconds
           metrics_set_gauge(METRIC_HEAP_FREE, esp_get_free_heap_size());
           metrics_set_gauge(METRIC_PSRAM_FREE, heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
           
           auto* power = hal::getPowerManagerInstance();
           if (power) {
               metrics_set_gauge(METRIC_BATTERY_PCT, power->getBatteryPercent());
               metrics_set_gauge(METRIC_BATTERY_MV, power->getBatteryVoltage());
           }
           
           metrics_set_gauge(METRIC_UPTIME_MS, esp_timer_get_time() / 1000);
           
           vTaskDelay(pdMS_TO_TICKS(10000));
       }
   }
   ```

3. **Start collection task in main.cpp**:
   ```cpp
   // After log_init()
   xTaskCreate(metricsCollectorTask, "metrics_collector", 2048, NULL, 5, NULL);
   ```

4. **Export in METRICS command**:
   ```
   heap_free_bytes 150000
   psram_free_bytes 200000
   battery_percent 85
   battery_millivolts 3800
   uptime_ms 86400000
   ```

## References
- ESP32-S3 memory APIs: `esp_get_free_heap_size()`, `heap_caps_get_free_size()`
- Power manager: `components/cdc_hal/include/cdc_hal/IPowerManager.h`
- Current implementation: `components/serial_cmd/src/SerialCmd.cpp:438-452`
