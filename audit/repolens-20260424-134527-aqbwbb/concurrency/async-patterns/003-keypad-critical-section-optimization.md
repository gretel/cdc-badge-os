---
title: "[LOW] Keypad circular buffer uses long critical sections in ISR context"
severity: LOW
domain: concurrency/async-patterns
lens: async-patterns
labels:
  - "concurrency"
  - "critical-section"
  - "keypad"
  - "isr-safety"
---

## Summary
In `components/cdc_hal/src/TCA9535Keypad.cpp`, the `isrHandler()` calls `xSemaphoreGiveFromISR()` which can trigger a context switch via `portYIELD_FROM_ISR()`. While the ISR itself is short, the worker task uses `portENTER_CRITICAL()`/`portEXIT_CRITICAL()` for buffer operations that could be optimized with interrupt-safe queue operations or shorter critical sections.

**Location:** `components/cdc_hal/src/TCA9535Keypad.cpp:370-378` (ISR) and `336-363` (buffer operations)

```cpp
// ISR (line 370-378)
void IRAM_ATTR TCA9535Keypad::isrHandler(void* arg) {
    auto* self = static_cast<TCA9535Keypad*>(arg);
    if (self->inSleepMode_) return;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(self->semaphore_, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);  // May trigger context switch
}

// Buffer operations (line 336-363)
void TCA9535Keypad::bufferAddKey(Key key) {
    if (key == Key::KEY_NONE) return;

    portENTER_CRITICAL(&bufferMux_);  // Critical section
    uint8_t nextHead = (bufferHead_ + 1) % KEY_BUFFER_SIZE;
    if (nextHead != bufferTail_) {
        keyBuffer_[bufferHead_] = key;
        bufferHead_ = nextHead;
    }
    portEXIT_CRITICAL(&bufferMux_);
}
```

## Impact
- **Minor latency:** Critical sections disable interrupts globally, potentially delaying other ISRs
- **Context switch overhead:** `portYIELD_FROM_ISR()` may cause unnecessary task switches if the worker task is already running
- **Not a major issue:** The critical sections are short and the pattern is generally correct

## Evidence
File: `components/cdc_hal/src/TCA9535Keypad.cpp`
- Line 370-378: ISR uses `portYIELD_FROM_ISR()` which may trigger context switch
- Lines 336-350: `bufferAddKey()` uses `portENTER_CRITICAL()`/`portEXIT_CRITICAL()`
- Lines 353-363: `bufferGetKey()` uses same critical section pattern
- Line 490-494: `clearBuffer()` also uses critical sections
- The semaphore pattern is correct, but the critical sections could be replaced with FreeRTOS queue for better abstraction

## Recommended Fix
**Option 1: Use FreeRTOS queue for key events (recommended)**
```cpp
// Replace circular buffer with queue
static constexpr size_t KEY_EVENT_QUEUE_SIZE = 16;
QueueHandle_t keyEventQueue_ = nullptr;

// In init():
keyEventQueue_ = xQueueCreate(KEY_EVENT_QUEUE_SIZE, sizeof(Key));

// In ISR:
void IRAM_ATTR TCA9535Keypad::isrHandler(void* arg) {
    auto* self = static_cast<TCA9535Keypad*>(arg);
    if (self->inSleepMode_) return;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(self->keyEventQueue_, &self->currentKey_, &xHigherPriorityTaskWoken);
    xSemaphoreGiveFromISR(self->semaphore_, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// In worker task:
void TCA9535Keypad::taskFunc(void* arg) {
    auto* self = static_cast<TCA9535Keypad*>(arg);
    Key key;
    
    while (true) {
        xSemaphoreTake(self->semaphore_, pdMS_TO_TICKS(POLL_TIMEOUT_MS));
        
        // Read all queued key events
        while (xQueueReceive(self->keyEventQueue_, &key, 0) == pdTRUE) {
            if (self->callback_) {
                self->callback_(key, true);
            }
        }
        
        // ... rest of processing
    }
}
```

**Option 2: Optimize current critical sections (minimal change)**
```cpp
// Already optimal for simple operations - no change needed
// The critical sections are already short and efficient
// Only consider Option 1 if you need queue-like semantics
```

**Option 3: Use task notifications for single-key events**
```cpp
// For single-key scenarios, use task notifications instead of semaphore
void IRAM_ATTR TCA9535Keypad::isrHandler(void* arg) {
    auto* self = static_cast<TCA9535Keypad*>(arg);
    if (self->inSleepMode_) return;

    // Send key data via task notification
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(&self->taskHandle_, self->currentKey_, 
                       eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// In worker task:
uint32_t key = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
Key currentKey = static_cast<Key>(key);
```

## References
- [FreeRTOS ISR Best Practices](https://www.freertos.org/a00110.html)
- [FreeRTOS Queues in ISR](https://www.freertos.org/FreeRTOS-queue-functions.html)
- [Task Notifications](https://www.freertos.org/worker-tasks.html)
