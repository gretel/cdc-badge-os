---
title: "[MEDIUM] E-Paper display render task and mutex not cleaned up on stop"
severity: MEDIUM
domain: connection-mgmt
lens: performance/connection-mgmt
labels:
  - "audit:performance/connection-mgmt"
---

## Summary

In `components/cdc_hal/src/EpaperDisplay.cpp`, the `init()` method creates a FreeRTOS task and mutex using `xTaskCreate()` and `xSemaphoreCreateMutex()`, but the `stop()` method never cleans them up. The task handle `s_renderTask` and mutex handle `s_renderMutex` are stored but not destroyed, causing resource leaks on each init/stop cycle.

**Location:** `components/cdc_hal/src/EpaperDisplay.cpp:224-230` (creation) and `260-266` (stop method)

## Impact

1. **Task leak**: The render task continues running even after `stop()` is called, consuming:
   - Stack memory (8192 bytes as specified in `xTaskCreate`)
   - CPU time (task remains in ready state, waiting on notify)
   - TCB (Task Control Block) structure

2. **Mutex leak**: The render mutex uses heap memory (typically 16-32 bytes) that is never freed.

3. **Accumulated leaks**: If `init()`/`stop()` cycles occur multiple times (e.g., during sleep/wake cycles), multiple tasks and mutexes accumulate, eventually causing:
   - Heap exhaustion
   - Task table exhaustion (limited by `configMAX_TASKS`)
   - System instability or crash

4. **Stale task notifications**: The task waits on `ulTaskNotifyTake()` with `portMAX_DELAY`. Even after `stop()`, if `flush()` is called, `xTaskNotifyGive()` will notify the old task which might still be running.

5. **Display hardware state**: The display hardware might be in an undefined state if the render task tries to access it after `stop()`.

## Evidence

**File: `components/cdc_hal/src/EpaperDisplay.cpp`**

1. **Task and mutex created in init() (line 224-230):**
```cpp
// Create render task
s_renderMutex = xSemaphoreCreateMutex();  // Created
if (!s_renderMutex) {
    LOG_E(TAG, "Failed to create render mutex");
    state_ = core::ServiceState::ERROR;
    return false;
}

BaseType_t ret = xTaskCreate(renderTask, "epd_render", 8192, nullptr, 5, &s_renderTask);  // Created
if (ret != pdPASS) {
    LOG_E(TAG, "Failed to create render task");
    state_ = core::ServiceState::ERROR;
    return false;
}
```

2. **stop() doesn't clean up (line 260-266):**
```cpp
void EpaperDisplay::stop() {
    if (state_ == core::ServiceState::STARTED) {
        s_backlightOn = false;
        applyBacklight(0);
        state_ = core::ServiceState::STOPPED;
        // s_renderTask not deleted!
        // s_renderMutex not deleted!
    }
}
```

3. **Global static variables (from header):**
```cpp
static TaskHandle_t s_renderTask = nullptr;
static SemaphoreHandle_t s_renderMutex = nullptr;
```

4. **Task function runs indefinitely (line 105-122):**
```cpp
static void renderTask(void* arg) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // Waits forever

        xSemaphoreTake(s_renderMutex, portMAX_DELAY);
        bool doFull = s_renderFull;
        s_renderPending = false;
        xSemaphoreGive(s_renderMutex);

        if (s_epd_display) {
            // Update display
        }
    }
}
```

5. **flush() can still notify the task (line 288-299):**
```cpp
void EpaperDisplay::flush(RefreshMode mode) {
    // ...
    xSemaphoreTake(s_renderMutex, portMAX_DELAY);  // Uses the same mutex
    // ...
    xTaskNotifyGive(s_renderTask);  // Notifies the same task
}
```

6. **isBusy() checks the same state (line 138):**
```cpp
bool isBusy() const override { return s_renderPending; }
```

## Recommended Fix

Add proper cleanup in the `stop()` method:

1. **Update stop() method:**
```cpp
void EpaperDisplay::stop() {
    if (state_ == core::ServiceState::STARTED) {
        s_backlightOn = false;
        applyBacklight(0);
        state_ = core::ServiceState::STOPPED;
    }
    
    // Delete render task
    if (s_renderTask) {
        vTaskDelete(s_renderTask);
        s_renderTask = nullptr;
    }
    
    // Delete render mutex
    if (s_renderMutex) {
        vSemaphoreDelete(s_renderMutex);
        s_renderMutex = nullptr;
    }
    
    LOG_I(TAG, "Display stopped");
}
```

2. **Add guard in init() for re-initialization:**
```cpp
bool EpaperDisplay::init() {
    if (s_initialized) {
        return true;
    }
    
    // Clean up old resources if they exist
    if (s_renderTask) {
        vTaskDelete(s_renderTask);
        s_renderTask = nullptr;
    }
    if (s_renderMutex) {
        vSemaphoreDelete(s_renderMutex);
        s_renderMutex = nullptr;
    }
    
    // ... rest of init
}
```

3. **Add state check in flush() to prevent notifying dead task:**
```cpp
void EpaperDisplay::flush(RefreshMode mode) {
    // If no render task, fall back to sync
    if (!s_renderTask || state_ != core::ServiceState::STARTED) {
        flushSync(mode);
        return;
    }
    
    xSemaphoreTake(s_renderMutex, portMAX_DELAY);
    // ... rest of flush
}
```

4. **Alternatively, use a running flag to signal task to exit:**
```cpp
static volatile bool s_rendering = false;

static void renderTask(void* arg) {
    while (s_rendering) {  // Check flag
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));  // Use timeout
        // ... process
    }
    // Exit naturally
}

void EpaperDisplay::stop() {
    s_rendering = false;  // Signal task to exit
    // Wait for task to exit
    vTaskDelay(pdMS_TO_TICKS(100));
    // Then delete
    if (s_renderTask) {
        vTaskDelete(s_renderTask);
        s_renderTask = nullptr;
    }
}
```

## References

- [FreeRTOS Task API](https://www.freertos.org/tasks.html) - vTaskDelete
- [FreeRTOS Semaphore API](https://www.freertos.org/a00102.html) - vSemaphoreDelete
- [Task Notification](https://www.freertos.org/High-speed-communication-between-tasks-and-the-Tickless-idle-mode.html) - Task notify patterns
- [Resource Acquisition Is Initialization](https://en.wikipedia.org/wiki/Resource_acquisition_is_initialization) - RAII pattern

</content>