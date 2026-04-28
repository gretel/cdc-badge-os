---
title: "[LOW] EpaperDisplay Render State Race - Non-Atomic Render Pending Flags"
severity: LOW
domain: concurrency
lens: race-conditions
labels:
  - "audit:concurrency/race-conditions"
---

## Summary

The `EpaperDisplay` class in `components/cdc_hal/src/EpaperDisplay.cpp` uses `volatile` flags for render state (`s_renderPending`, `s_renderFull`) that are accessed from multiple tasks (main task calling `flush()`, render task processing). While the mutex protects the critical section, the `isBusy()` check is not atomic with respect to the actual busy state.

**Evidence** - Static state variables (lines 56-59):
```cpp
/** \brief Render-task runtime state. */
static SemaphoreHandle_t s_renderMutex = nullptr;
static TaskHandle_t s_renderTask = nullptr;
static volatile bool s_renderPending = false;
static volatile bool s_renderFull = false;
```

**Evidence** - `flush()` (lines 277-291):
```cpp
void EpaperDisplay::flush(RefreshMode mode) {
    if (!s_renderTask) {
        flushSync(mode);
        return;
    }

    xSemaphoreTake(s_renderMutex, portMAX_DELAY);
    if (s_renderPending) {
        // Merge request: if FULL is requested, upgrade to FULL
        if (mode == RefreshMode::FULL) s_renderFull = true;
    } else {
        s_renderPending = true;
        s_renderFull = (mode == RefreshMode::FULL);
    }
    xSemaphoreGive(s_renderMutex);

    xTaskNotifyGive(s_renderTask);
}
```

**Evidence** - `isBusy()` (line 133):
```cpp
bool isBusy() const override { return s_renderPending; }
```

**Evidence** - `renderTask()` (lines 104-121):
```cpp
static void renderTask(void* arg) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        xSemaphoreTake(s_renderMutex, portMAX_DELAY);
        bool doFull = s_renderFull;
        s_renderPending = false;  // Clear pending
        xSemaphoreGive(s_renderMutex);

        if (s_epd_display) {
            if (doFull) {
                s_epd_display->update();
            } else {
                s_epd_display->updateWindow(0, 0, HEIGHT, WIDTH, false);
            }
        }
    }
}
```

**The Race**:
1. Task A calls `flush(RefreshMode::PARTIAL)`
2. Task A sets `s_renderPending = true`, `s_renderFull = false`
3. Task B calls `isBusy()` - returns true
4. Render task wakes up, takes mutex
5. Render task reads `s_renderFull = false`, sets `s_renderPending = false`
6. Task A calls `flush(RefreshMode::FULL)` - sets `s_renderPending = true`, `s_renderFull = true`
7. Render task releases mutex, starts display update with `doFull = false` (stale value!)

The issue is that `doFull` is read while holding the mutex, but the actual display update happens AFTER releasing the mutex. If another `flush()` happens in between, the mode can change.

## Impact

**Incorrect Refresh Mode**: The display might do a partial update when a full update was requested (or vice versa), causing visual artifacts on the E-Paper display.

**Lost Updates**: If `isBusy()` is checked between when `s_renderPending` is cleared and the actual update completes, the caller might think the display is ready when it's still updating.

## Recommended Fix

Move the display update inside the mutex or store the mode in a local variable before releasing the mutex:

```cpp
static void renderTask(void* arg) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        bool doFull;
        xSemaphoreTake(s_renderMutex, portMAX_DELAY);
        doFull = s_renderFull;  // Read while holding mutex
        s_renderPending = false;
        xSemaphoreGive(s_renderMutex);

        // Store the epd_display pointer while mutex is held
        Gdey029T94* display = s_epd_display;
        xSemaphoreTake(s_renderMutex, portMAX_DELAY);
        // Check if another flush upgraded to FULL while we released mutex
        if (s_renderFull && !s_renderPending) {
            doFull = true;
        }
        xSemaphoreGive(s_renderMutex);

        if (display) {
            if (doFull) {
                display->update();
            } else {
                display->updateWindow(0, 0, HEIGHT, WIDTH, false);
            }
        }
    }
}
```

Alternatively, use a task notification with the mode encoded:
```cpp
// In flush():
uint32_t modeValue = (mode == RefreshMode::FULL) ? 1 : 0;
xTaskNotifyGive(s_renderTask);  // Or use xTaskNotifyWithValue

// In renderTask():
uint32_t modeValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
bool doFull = (modeValue != 0);
```

## References

- E-Paper display timing requirements: https://www.waveshare.com/wiki/2.9inch_e-Paper_Module_(B)
- ESP-IDF task notifications: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/freertos.html#task-notifications
