---
title: "[LOW] Sleep Controller Callbacks Have No Error Handling for Module Callbacks"
severity: LOW
domain: power-management
lens: graceful-degradation
labels:
  - "audit:error-handling/graceful-degradation"
---

## Summary
In `components/cdc_hal/src/SleepController.cpp`, when entering sleep, callbacks are invoked without error handling. If a module's pre-sleep callback fails (e.g., needs to flush data but I2C is busy), there's no mechanism to handle the failure or continue with other modules. Similarly, wakeup callbacks are called without checking if they succeed.

Lines of interest:
- `SleepController.cpp:118-158` - `enterLightSleep()` invokes callbacks without error handling
- `SleepController.cpp:352-360` - `invokeCallbacks()` has no return value or error tracking

## Impact
Callback failures during sleep transitions cause:
1. **Silent data loss** - e.g., module fails to flush data but sleep continues
2. **No rollback** - can't prevent sleep if critical module isn't ready
3. **Cascading failures** - module wakes up in inconsistent state
4. **Debugging difficulty** - hard to know which callback failed

## Evidence
```cpp
// SleepController.cpp:352-360
void Esp32SleepController::invokeCallbacks(SleepCallbackEntry* callbacks, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (callbacks[i].callback) {
            LOG_D(TAG, "Invoking callback: %s", callbacks[i].moduleName);
            callbacks[i].callback(callbacks[i].context);  // No error handling
        }
    }
}

// SleepController.cpp:118-158
void Esp32SleepController::enterLightSleep() {
    // ...
    // Invoke pre-sleep callbacks (modules can prepare for sleep)
    invokeCallbacks(preSleepCallbacks_, preSleepCount_);  // No check for failures
    
    // Prepare GPIO before sleep
    prepareGpioForSleep();
    
    LOG_D(TAG, "Entering light sleep...");
    
    // Enter light sleep (no way to abort if callback failed)
    esp_light_sleep_start();
    // ...
}
```

## Recommended Fix
Add error handling to sleep callbacks:

1. **Change callback signature to return status**:
   ```cpp
   // SleepController.h
   using SleepCallback = bool(*)(void* context);  // Return true on success
   
   struct SleepCallbackEntry {
       SleepCallback callback;
       void* context;
       const char* moduleName;
       uint8_t priority;
   };
   ```

2. **Track callback results**:
   ```cpp
   // SleepController.cpp
   bool Esp32SleepController::invokeCallbacks(SleepCallbackEntry* callbacks, size_t count) {
       bool allOk = true;
       for (size_t i = 0; i < count; i++) {
           if (callbacks[i].callback) {
               LOG_D(TAG, "Invoking callback: %s", callbacks[i].moduleName);
               if (!callbacks[i].callback(callbacks[i].context)) {
                   LOG_W(TAG, "Callback failed: %s", callbacks[i].moduleName);
                   allOk = false;
               }
           }
       }
       return allOk;
   }
   ```

3. **Allow sleep to be aborted if critical callbacks fail**:
   ```cpp
   void Esp32SleepController::enterLightSleep() {
       // Invoke pre-sleep callbacks
       if (!invokeCallbacks(preSleepCallbacks_, preSleepCount_)) {
           LOG_W(TAG, "Some callbacks failed - sleep may be incomplete");
           // Continue but log warning (or abort if critical callbacks fail)
       }
       // ...
   }
   ```

4. **Add critical callback flag**:
   ```cpp
   struct SleepCallbackEntry {
       SleepCallback callback;
       void* context;
       const char* moduleName;
       uint8_t priority;
       bool critical;  // If true, abort sleep on failure
   };
   ```

## References
- [Embedded Sleep State Management](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/sleep_modes.html)
