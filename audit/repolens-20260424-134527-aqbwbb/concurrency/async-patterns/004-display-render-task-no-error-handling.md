---
title: "[LOW] E-Paper render task has no error handling or recovery mechanism"
severity: LOW
domain: concurrency/async-patterns
lens: async-patterns
labels:
  - "concurrency"
  - "task-lifecycle"
  - "display"
  - "error-handling"
---

## Summary
In `components/cdc_hal/src/EpaperDisplay.cpp`, the render task (`renderTask`) runs indefinitely with no error handling, no mechanism to signal completion/failure back to the caller, and no way to abort pending renders. The task uses `ulTaskNotifyTake(pdTRUE, portMAX_DELAY)` which blocks forever, and any errors during `update()` or `updateWindow()` are silently ignored.

**Location:** `components/cdc_hal/src/EpaperDisplay.cpp:104-119`

```cpp
static void renderTask(void* arg) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // Blocks forever

        xSemaphoreTake(s_renderMutex, portMAX_DELAY);
        bool doFull = s_renderFull;
        s_renderPending = false;
        xSemaphoreGive(s_renderMutex);

        if (s_epd_display) {
            if (doFull) {
                s_epd_display->update();  // No error check
            } else {
                s_epd_display->updateWindow(0, 0, HEIGHT, WIDTH, false);  // No error check
            }
        }
        // Task loops forever, no exit condition
    }
}
```

## Impact
- **Silent failures:** Display update errors are not logged or reported
- **No task recovery:** If the task crashes or hangs, there's no mechanism to restart it
- **No abort capability:** Long-running E-Paper updates (2-5 seconds) cannot be cancelled
- **Debug difficulty:** Hard to diagnose display issues without error feedback

## Evidence
File: `components/cdc_hal/src/EpaperDisplay.cpp`
- Lines 104-119: `renderTask()` with no error handling
- Line 105: `ulTaskNotifyTake(pdTRUE, portMAX_DELAY)` - infinite wait
- Line 107: `xSemaphoreTake(s_renderMutex, portMAX_DELAY)` - infinite wait on mutex
- Lines 112-117: `update()`/`updateWindow()` called without checking return values
- Line 298: `xTaskNotifyGive(s_renderTask)` - fire-and-forget notification
- No mechanism to stop the task or handle errors

## Recommended Fix
Add error handling and task lifecycle management:

```cpp
// Add state tracking
static volatile bool s_renderTaskAlive = false;
static volatile bool s_renderError = false;
static uint32_t s_renderErrors = 0;

// Updated render task with error handling
static void renderTask(void* arg) {
    s_renderTaskAlive = true;
    while (true) {
        // Wait for render request with timeout
        uint32_t notify = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));
        
        if (notify == 0) {
            // Timeout - check if task should exit
            continue;
        }

        // Take mutex with timeout
        if (xSemaphoreTake(s_renderMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            LOG_W(TAG, "Render mutex timeout");
            s_renderErrors++;
            continue;
        }
        
        bool doFull = s_renderFull;
        s_renderPending = false;
        xSemaphoreGive(s_renderMutex);

        if (!s_epd_display) {
            LOG_W(TAG, "Display not ready for render");
            s_renderErrors++;
            continue;
        }

        // Perform update with error handling
        bool success = false;
        if (doFull) {
            // Check return value if update() returns bool
            s_epd_display->update();
            success = true;  // Assume success if no exception
        } else {
            s_epd_display->updateWindow(0, 0, HEIGHT, WIDTH, false);
            success = true;
        }

        if (!success) {
            LOG_E(TAG, "Render failed (total errors: %lu)", s_renderErrors);
            s_renderErrors++;
            s_renderError = true;
        }
    }
    s_renderTaskAlive = false;
}

// Add task health check function
bool isRenderTaskHealthy() {
    return s_renderTaskAlive && (s_renderErrors < 10);
}

// Add error reset function
void resetRenderErrors() {
    s_renderErrors = 0;
    s_renderError = false;
}
```

**Alternative: Add abort capability for long updates**
```cpp
// Add abort flag
static volatile bool s_renderAbort = false;

// In flush():
void EpaperDisplay::flush(RefreshMode mode) {
    if (!s_renderTask) {
        flushSync(mode);
        return;
    }

    // Check for pending render
    xSemaphoreTake(s_renderMutex, portMAX_DELAY);
    if (s_renderPending) {
        if (mode == RefreshMode::FULL) s_renderFull = true;
    } else {
        s_renderPending = true;
        s_renderFull = (mode == RefreshMode::FULL);
    }
    xSemaphoreGive(s_renderMutex);

    xTaskNotifyGive(s_renderTask);
}

// Add abort function
void abortPendingRender() {
    s_renderAbort = true;
    // Task can check this flag and exit current operation
}
```

## References
- [FreeRTOS Task Notifications](https://www.freertos.org/High-speed-communication-between-tasks-and-ISR-using-task-notifications.html)
- [E-Paper Display Timing](https://www.waveshare.com/wiki/2.9inch_e-Paper_Module) (2-5 second refresh times)
- ESP-IDF Task Management: `components/freertos/include/freertos/task.h`
