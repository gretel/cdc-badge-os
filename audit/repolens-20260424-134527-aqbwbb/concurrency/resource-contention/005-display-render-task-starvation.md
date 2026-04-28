---
title: "[MEDIUM] Display Render Task Starvation Due to Blocking Mutex"
severity: MEDIUM
domain: resource-contention
lens: concurrency
labels:
  - audit:concurrency/resource-contention
---

## Summary

The E-Paper display uses a render task that blocks indefinitely on a mutex (`s_renderMutex`) and uses `ulTaskNotifyTake()` for signaling. The render operation can take 100-500ms, during which the task holds the mutex, potentially starving other operations that need to queue render updates.

**Location**: `components/cdc_hal/src/EpaperDisplay.cpp:100-120`

## Impact

**Resource Contention Risk**: The render task's blocking mutex can cause:

1. **UI lag**: Multiple flush requests queue up but only one executes at a time
2. **Starvation**: `flush()` calls block waiting for the mutex while render is slow
3. **Missed updates**: `flush()` with 0ms timeout may fail if render is busy

**Evidence**:
- `EpaperDisplay.cpp:100-110`: Render task blocks forever on mutex
- `EpaperDisplay.cpp:179-190`: `flushSync()` uses blocking mutex
- E-Paper display update takes 100-500ms (hardware limitation)

## Evidence

**Render Task** (`components/cdc_hal/src/EpaperDisplay.cpp:100-120`):
```cpp
static SemaphoreHandle_t s_renderMutex = nullptr;
static TaskHandle_t s_renderTask = nullptr;
static volatile bool s_renderPending = false;
static volatile bool s_renderFull = false;

static void renderTask(void* arg) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // <-- Blocks forever

        xSemaphoreTake(s_renderMutex, portMAX_DELAY);  // <-- Blocks forever
        bool doFull = s_renderFull;
        s_renderPending = false;
        xSemaphoreGive(s_renderMutex);

        if (s_epd_display) {
            if (doFull) {
                s_epd_display->update();  // <-- 100-500ms!
            } else {
                s_epd_display->updateWindow(0, 0, HEIGHT, WIDTH, false);
            }
        }
    }
}
```

**Flush Sync** (`components/cdc_hal/src/EpaperDisplay.cpp:179-190`):
```cpp
void EpaperDisplay::flushSync(RefreshMode mode) {
    xSemaphoreTake(s_renderMutex, portMAX_DELAY);  // <-- Blocks forever
    s_renderFull = (mode == RefreshMode::FULL);
    s_renderPending = true;
    xSemaphoreGive(s_renderMutex);

    // Notify render task
    BaseType_t ret = xTaskNotifyGive(s_renderTask);
    if (ret != pdPASS) {
        LOG_W(TAG, "Failed to notify render task");
        return;
    }

    // Wait for completion (busy-wait)
    while (s_renderPending) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

**Flush Async** (`components/cdc_hal/src/EpaperDisplay.cpp:165-177`):
```cpp
void EpaperDisplay::flush(RefreshMode mode) {
    xSemaphoreTake(s_renderMutex, portMAX_DELAY);  // <-- Blocks forever
    if (s_renderPending) {
        s_renderFull = true;  // Mark as full refresh if pending
    } else {
        s_renderFull = (mode == RefreshMode::FULL);
        s_renderPending = true;
    }
    xSemaphoreGive(s_renderMutex);

    // Notify render task
    BaseType_t ret = xTaskNotifyGive(s_renderTask);
    if (ret != pdPASS) {
        LOG_W(TAG, "Failed to notify render task");
    }
}
```

**UI Process** (`main/main.cpp:245`):
```cpp
cdc::ui::ui_process(nowMs);  // May call flush() multiple times
```

If UI calls `flush()` while render task is busy (100-500ms), the UI thread blocks.

## Recommended Fix

1. **Use timeout on mutex** in flush operations:
```cpp
void EpaperDisplay::flushSync(RefreshMode mode) {
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout = pdMS_TO_TICKS(600);  // 600ms max for E-Paper

    if (xSemaphoreTake(s_renderMutex, timeout) != pdTRUE) {
        LOG_W(TAG, "Render mutex timeout, skipping flush");
        return;
    }

    s_renderFull = (mode == RefreshMode::FULL);
    s_renderPending = true;
    xSemaphoreGive(s_renderMutex);

    // Wait for completion with timeout
    uint32_t waitStart = esp_timer_get_time();
    while (s_renderPending && (esp_timer_get_time() - waitStart) < 600000) {  // 600ms
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

2. **Add non-blocking flush option**:
```cpp
bool EpaperDisplay::flushTry(RefreshMode mode);  // New method

bool EpaperDisplay::flushTry(RefreshMode mode) {
    if (xSemaphoreTake(s_renderMutex, 0) != pdTRUE) {
        return false;  // Render busy, try later
    }

    if (s_renderPending) {
        s_renderFull = true;
    } else {
        s_renderFull = (mode == RefreshMode::FULL);
        s_renderPending = true;
    }
    xSemaphoreGive(s_renderMutex);

    xTaskNotifyGive(s_renderTask);
    return true;
}
```

3. **Track render queue depth** for monitoring:
```cpp
static uint32_t s_renderQueueDepth = 0;

void EpaperDisplay::flush(RefreshMode mode) {
    xSemaphoreTake(s_renderMutex, portMAX_DELAY);
    if (s_renderPending) {
        s_renderFull = true;
        s_renderQueueDepth++;  // Track backlog
    } else {
        s_renderFull = (mode == RefreshMode::FULL);
        s_renderPending = true;
        s_renderQueueDepth = 1;
    }
    xSemaphoreGive(s_renderMutex);
}

uint32_t EpaperDisplay::getRenderQueueDepth() const {
    return s_renderQueueDepth;
}
```

## References

- FreeRTOS: Task notifications and semaphores
- E-Paper display: Gdey029T94 timing specifications
- CalEPD library: `update()` and `updateWindow()` methods
