---
title: "[LOW] I2C bus initialization blocks sequentially before other hardware"
severity: LOW
domain: startup-performance
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
I2C bus initialization (`main.cpp:99-113`) is a synchronous blocking operation that runs before all other hardware controllers (Power, Sleep, WiFi, Bluetooth, Keypad, Secure Element). These controllers could potentially initialize in parallel since they all depend on the I2C bus being ready.

**Location:** `main/main.cpp:99-113`

## Impact
- **Sequential bottleneck**: All hardware controllers wait for I2C init to complete
- **No progress feedback**: User sees nothing until all hardware is initialized
- **Scalability issue**: Adding more I2C devices linearly increases boot time

## Evidence
From `main/main.cpp:99-113`:
```cpp
LOG_I(TAG, "Initializing I2C bus...");
s_i2cBus = cdc::hal::getI2cBus0();
if (s_i2cBus && s_i2cBus->init()) {
    LOG_I(TAG, "I2C bus ready");
} else {
    LOG_E(TAG, "I2C bus init failed!");
}

// Then sequentially:
// Power Management (lines 115-123)
// Sleep Controller (lines 126-134)
// WiFi Controller (lines 137-143)
// Bluetooth Controller (lines 146-152)
// Keypad (lines 155-161)
// Secure Element (lines 164-174)
```

Each controller's `init()` and `start()` is called synchronously.

## Recommended Fix
**Parallelize hardware initialization** after I2C bus is ready:

1. Initialize I2C bus first (it's the foundation)
2. Spawn parallel tasks for independent hardware controllers
3. Wait for all tasks to complete before proceeding

```cpp
// After I2C init
TaskHandle_t powerTask, sleepTask, wifiTask, btTask, keypadTask, seTask;

xTaskCreate([](void* arg) {
    auto* ctrl = cdc::hal::getPowerManagerInstance();
    if (ctrl) ctrl->init();
    vTaskDelete(nullptr);
}, "init_power", 2048, nullptr, 5, &powerTask);

// Repeat for other controllers...

// Wait for all
vTaskDelay(pdMS_TO_TICKS(100));  // Or use semaphores for precise sync
```

**Simpler alternative**: Batch init calls separately from start calls:
```cpp
// Phase 1: Init all (fast)
s_i2cBus->init();
s_powerManager->init();
s_sleepController->init();
// ...

// Phase 2: Start all (slower, can be parallel)
s_powerManager->start();
s_sleepController->start();
// ...
```

## References
- ESP-IDF: [I2C initialization](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2c.html)
- FreeRTOS: [Task creation for parallel execution](https://www.freertos.org/a00111.html)