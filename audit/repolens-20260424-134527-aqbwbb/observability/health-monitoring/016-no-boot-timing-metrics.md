---
title: "[LOW] No boot timing metrics for initialization performance"
severity: LOW
domain: observability
lens: health-monitoring
labels:
  - "audit:observability/health-monitoring"
---

## Summary

The boot sequence in `main/main.cpp` initializes many components (I2C, power manager, sleep controller, WiFi, Bluetooth, keypad, secure element, display, modules) but **does not track how long each stage takes**. This makes it difficult to:

- Identify slow initialization bottlenecks
- Track boot performance over time
- Detect when a specific component starts taking longer than expected

**Current behavior:**
- Each component logs "Initializing X..." and "X ready"
- No timing information between these messages
- No cumulative boot time measurement

## Impact

1. **Debugging slow boots**: Cannot tell which component is causing delays
2. **Performance regression detection**: Cannot track if boot time increases over firmware versions
3. **Optimization difficulty**: Cannot measure the impact of optimization changes
4. **User experience**: No way to show progress during long boot sequences

## Evidence

**Boot sequence** (`main/main.cpp:54-240`):
```cpp
void app_main(void) {
    // Stage 0: Hardware minimum
    nvs_flash_init();

    // Stage 1: Core Services
    EventBus::instance().init();
    usb_cdc_init();
    log_init();
    LOG_I(TAG, "USB CDC ready");

    // I2C
    LOG_I(TAG, "Initializing I2C bus...");
    s_i2cBus = cdc::hal::getI2cBus0();
    if (s_i2cBus && s_i2cBus->init()) {
        LOG_I(TAG, "I2C bus ready");
    }

    // Power Manager
    LOG_I(TAG, "Initializing Power Management...");
    s_powerManager = cdc::hal::getPowerManagerInstance();
    if (s_powerManager && s_powerManager->init() && s_powerManager->start()) {
        LOG_I(TAG, "Power Management ready (BQ25895)");
    }

    // ... many more components ...

    LOG_I(TAG, "System ready. Entering main loop.");
}
```

**No timing code exists:**
```bash
grep -n "esp_timer_get_time" main/main.cpp
# Only one result at line 249 (in main loop, not boot)
```

**Current STATUS command** (`components/serial_cmd/src/SerialCmd.cpp:427-434`):
```cpp
static void cmdStatus(const char* args) {
    Console::printf("=== System Status ===\r\n");
    Console::printf("Free heap: %lu bytes\r\n", (unsigned long)esp_get_free_heap_size());
    Console::printf("Min free heap: %lu bytes\r\n", (unsigned long)esp_get_minimum_free_heap_size());
    Console::printf("Uptime: %llu ms\r\n", esp_timer_get_time() / 1000ULL);
    Console::flush();
}
```
Only shows total uptime, not boot timing breakdown.

## Recommended Fix

Add boot timing instrumentation:

**Step 1: Add timing helper macro** (`main/main.cpp`):
```cpp
static uint32_t s_boot_start_ms = 0;
static uint32_t s_last_stage_ms = 0;

#define BOOT_STAGE(name) \
    do { \
        uint32_t now = esp_timer_get_time() / 1000; \
        if (s_boot_start_ms == 0) s_boot_start_ms = now; \
        uint32_t stage_ms = now - s_last_stage_ms; \
        uint32_t total_ms = now - s_boot_start_ms; \
        LOG_I(TAG, "[%lu ms | %lu ms] %s", (unsigned long)total_ms, (unsigned long)stage_ms, name); \
        s_last_stage_ms = now; \
    } while(0)
```

**Step 2: Instrument boot stages**:
```cpp
void app_main(void) {
    BOOT_STAGE("Start");

    nvs_flash_init();
    BOOT_STAGE("NVS");

    EventBus::instance().init();
    BOOT_STAGE("EventBus");

    usb_cdc_init();
    BOOT_STAGE("USB CDC");

    log_init();
    BOOT_STAGE("Logging");

    // I2C
    BOOT_STAGE("I2C init");
    s_i2cBus = cdc::hal::getI2cBus0();
    if (s_i2cBus && s_i2cBus->init()) {
        BOOT_STAGE("I2C ready");
    }

    // Power Manager
    BOOT_STAGE("Power init");
    s_powerManager = cdc::hal::getPowerManagerInstance();
    if (s_powerManager && s_powerManager->init() && s_powerManager->start()) {
        BOOT_STAGE("Power ready");
    }

    // ... continue for all stages ...

    BOOT_STAGE("Modules");
    modules_register_all();
    cdc::core::ModuleRegistry::instance().runAllInitializers();

    LOG_I(TAG, "Boot complete in %lu ms", (unsigned long)(esp_timer_get_time() / 1000 - s_boot_start_ms));
}
```

**Step 3: Add BOOTTIME command** (`components/serial_cmd/src/SerialCmd.cpp`):
```cpp
// Store boot timing data
static struct {
    uint32_t total_ms;
    uint8_t stage_count;
    struct { const char* name; uint32_t ms; } stages[16];
} s_bootTimings = {};

// Update boot code to populate s_bootTimings...

static void cmdBoottime(const char* args) {
    Console::printf("=== Boot Timing ===\r\n");
    Console::printf("Total: %lu ms\r\n", (unsigned long)s_bootTimings.total_ms);
    Console::printf("Stages:\r\n");
    for (uint8_t i = 0; i < s_bootTimings.stage_count; i++) {
        Console::printf("  %s: %lu ms\r\n",
            s_bootTimings.stages[i].name,
            (unsigned long)s_bootTimings.stages[i].ms);
    }
}
```

**Step 4: Register command**:
```cpp
reg.registerCommand({"BOOTTIME", "Show boot timing breakdown", cmdBoottime, "system", false});
```

**Expected output during boot**:
```
[   0 ms |    0 ms] Start
[   5 ms |    5 ms] NVS
[  10 ms |    5 ms] EventBus
[  50 ms |   40 ms] USB CDC
[  55 ms |    5 ms] Logging
[ 100 ms |   45 ms] I2C init
[ 120 ms |   20 ms] I2C ready
...
[ 850 ms |   30 ms] Modules
Boot complete in 850 ms
```

**Expected BOOTTIME command output**:
```
$ BOOTTIME
=== Boot Timing ===
Total: 850 ms
Stages:
  Start: 0 ms
  NVS: 5 ms
  EventBus: 5 ms
  USB CDC: 40 ms
  I2C init: 45 ms
  ...
```

## References

- ESP32 boot time optimization: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/startup.html
- ESP timer API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_timer.html

</content>