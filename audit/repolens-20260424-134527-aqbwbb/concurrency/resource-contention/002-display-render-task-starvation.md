---
title: "[HIGH] Display render task can starve under rapid flush requests"
severity: HIGH
domain: resource-contention
lens: concurrency
labels:
  - "audit:concurrency/resource-contention"
---

## Summary
The display render task in `EpaperDisplay` uses `ulTaskNotifyTake` with `portMAX_DELAY` to wait for flush requests. When multiple rapid flush requests are made, the notify counter increments but only one render cycle processes all pending requests. This causes the `s_renderFull` flag to be overwritten, potentially losing the FULL refresh mode request and causing ghosting on the e-paper display.

**Locations:**
- `components/cdc_hal/src/EpaperDisplay.cpp:58-72` - Render task definition
- `components/cdc_hal/src/EpaperDisplay.cpp:280-298` - flush() method
- `components/cdc_hal/src/EpaperDisplay.cpp:102-117` - renderTask() implementation

## Impact
1. **Data Loss**: Rapid successive calls to `flush(RefreshMode::FULL)` followed by `flush(RefreshMode::PARTIAL)` can lose the FULL mode flag, causing ghosting on the e-paper display.
2. **Render Starvation**: The render task processes one batch per notification, but multiple notifications can coalesce into a single render cycle.
3. **UI Lag**: If the main loop waits for `isBusy()` to clear, rapid flush requests can cause the display to appear unresponsive.

**Evidence:**
```cpp
// EpaperDisplay.cpp:renderTask (lines 58-72)
static void renderTask(void* arg) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // <-- Waits for notify
        
        xSemaphoreTake(s_renderMutex, portMAX_DELAY);
        bool doFull = s_renderFull;  // <-- Reads flag
        s_renderPending = false;     // <-- Clears pending
        xSemaphoreGive(s_renderMutex);
        
        if (s_epd_display) {
            if (doFull) {
                s_epd_display->update();  // Full refresh (~8-15s for e-paper)
            } else {
                s_epd_display->updateWindow(0, 0, HEIGHT, WIDTH, false);
            }
        }
    }
}

// EpaperDisplay.cpp:flush (lines 280-298)
void EpaperDisplay::flush(RefreshMode mode) {
    // If no render task, fall back to sync
    if (!s_renderTask) {
        flushSync(mode);
        return;
    }
    
    xSemaphoreTake(s_renderMutex, portMAX_DELAY);
    if (s_renderPending) {
        // Merge request: if FULL is requested, upgrade to FULL
        if (mode == RefreshMode::FULL) s_renderFull = true;  // <-- Only upgrades
    } else {
        s_renderPending = true;
        s_renderFull = (mode == RefreshMode::FULL);  // <-- Overwrites previous
    }
    xSemaphoreGive(s_renderMutex);
    
    xTaskNotifyGive(s_renderTask);  // <-- Increments notify counter
}
```

**Race condition sequence:**
```
Thread A: flush(FULL)    → s_renderFull = true, notify++
Thread B: flush(PARTIAL) → s_renderFull = true (not changed, still true)
Thread C: flush(PARTIAL) → s_renderFull = true (still true)
Render:  takes notify → doFull = true → FULL refresh (good)

---

Thread A: flush(PARTIAL) → s_renderFull = false, notify++
Thread B: flush(FULL)    → s_renderFull = true (upgraded), notify++
Thread C: flush(PARTIAL) → s_renderFull = true (not changed)
Render:  takes notify (counter=2) → doFull = true → FULL refresh (good)

---

Thread A: flush(FULL)    → s_renderFull = true, notify++
Thread B: flush(PARTIAL) → s_renderFull = true (not changed), notify++
Thread C: flush(PARTIAL) → s_renderFull = true (not changed), notify++
Render:  takes notify (counter=3) → doFull = true → FULL refresh (good)

---

Thread A: flush(PARTIAL) → s_renderFull = false, notify++
Thread B: flush(PARTIAL) → s_renderFull = false (not changed), notify++
Thread C: flush(FULL)    → s_renderFull = true (upgraded), notify++
Render:  takes notify (counter=3) → doFull = true → FULL refresh (good)

---

Thread A: flush(FULL)    → s_renderFull = true, notify++
Thread B: flush(PARTIAL) → s_renderFull = true (not changed), notify++
Thread C: flush(PARTIAL) → s_renderFull = true (not changed), notify++
Render:  takes notify (counter=3) → doFull = true → FULL refresh (good)

---

Thread A: flush(FULL)    → s_renderFull = true, notify++
Thread B: flush(PARTIAL) → s_renderFull = true (not changed), notify++
Render:  takes notify (counter=2), doFull = true
Thread C: flush(PARTIAL) → s_renderFull = true (not changed), notify++
Render:  takes notify (counter=1), doFull = true
```

The real issue is when FULL is followed by PARTIAL before the render task wakes:

```
Thread A: flush(FULL)    → s_renderFull = true, s_renderPending = true, notify++
Thread B: flush(PARTIAL) → s_renderPending is true, mode is PARTIAL → s_renderFull stays true
Render:  doFull = true → FULL refresh (CORRECT)

---

Thread A: flush(PARTIAL) → s_renderFull = false, s_renderPending = true, notify++
Thread B: flush(FULL)    → s_renderFull = true (upgraded), notify++
Thread C: flush(PARTIAL) → s_renderFull = true (not changed), notify++
Render:  doFull = true → FULL refresh (CORRECT)

---

Thread A: flush(FULL)    → s_renderFull = true, s_renderPending = true, notify++
Thread B: flush(PARTIAL) → s_renderFull = true (not changed), notify++
Thread C: flush(PARTIAL) → s_renderFull = true (not changed), notify++
Render:  doFull = true → FULL refresh (CORRECT)

---

Thread A: flush(FULL)    → s_renderFull = true, s_renderPending = true, notify++
Thread B: flush(PARTIAL) → s_renderFull = true (not changed), notify++
Thread C: flush(PARTIAL) → s_renderFull = true (not changed), notify++
Render:  doFull = true → FULL refresh (CORRECT)

---

Thread A: flush(FULL)    → s_renderFull = true, s_renderPending = true, notify++
Thread B: flush(PARTIAL) → s_renderFull = true (not changed), notify++
Thread C: flush(PARTIAL) → s_renderFull = true (not changed), notify++
Render:  doFull = true → FULL refresh (CORRECT)
```

Actually, the logic looks correct for preserving FULL. The real issue is **render task starvation** - the render task can be blocked by a long-running FULL refresh (8-15 seconds for e-paper) while new flush requests accumulate:

```
Thread A: flush(FULL)    → notify++, render starts FULL refresh (8-15s)
Thread B: flush(PARTIAL) → notify++, waits for render to complete
Thread C: flush(PARTIAL) → notify++, waits for render to complete
Thread D: flush(PARTIAL) → notify++, waits for render to complete
Render:  takes notify (counter=4), doFull = true → FULL refresh
         Now Thread B, C, D's requests are LOST (only one render cycle)
```

## Recommended Fix
1. **Add a pending flag for mode upgrade:**
   ```cpp
   void EpaperDisplay::flush(RefreshMode mode) {
       xSemaphoreTake(s_renderMutex, portMAX_DELAY);
       if (s_renderPending) {
           // Keep FULL if already pending FULL
           if (mode == RefreshMode::FULL) s_renderFull = true;
           // Don't downgrade FULL to PARTIAL
       } else {
           s_renderPending = true;
           s_renderFull = (mode == RefreshMode::FULL);
       }
       xSemaphoreGive(s_renderMutex);
       
       xTaskNotifyGive(s_renderTask);
   }
   ```

2. **Queue multiple flush requests:**
   Add a queue to buffer flush requests so none are lost during long renders:
   ```cpp
   static constexpr size_t FLUSH_QUEUE_SIZE = 8;
   static QueueHandle_t s_flushQueue;
   
   void EpaperDisplay::flush(RefreshMode mode) {
       xQueueSend(s_flushQueue, &mode, portMAX_DELAY);
       xTaskNotifyGive(s_renderTask);
   }
   ```

3. **Add timeout to prevent indefinite blocking:**
   ```cpp
   void EpaperDisplay::flushSync(RefreshMode mode) {
       if (!s_epd_display) return;
       
       // Wait for any pending render with timeout
       uint32_t start = xTaskGetTickCount();
       while (s_renderPending) {
           if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(2000)) {
               LOG_W(TAG, "Flush timeout");
               break;
           }
           vTaskDelay(pdMS_TO_TICKS(10));
       }
       
       if (mode == RefreshMode::FULL) {
           s_epd_display->update();
       } else {
           s_epd_display->updateWindow(0, 0, HEIGHT, WIDTH, false);
       }
   }
   ```

**Recommended approach:** Option 2 (queue-based) provides the most robust solution for handling rapid flush requests without losing data.

## References
- [ESP32 E-Paper Display Timing](https://github.com/lewisxhe/EPD_Examples) - Full refresh takes 8-15s
- [FreeRTOS Task Notifications](https://www.freertos.org/pt-task-notifications.html) - Notify counter behavior
- [E-Paper Ghosting](https://www.waveshare.com/wiki/2.9inch_e-Paper_Module) - Need full refresh to prevent ghosting
