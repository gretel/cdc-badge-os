---
title: "[MEDIUM] Mutable state in EventBus without thread-safe access"
severity: MEDIUM
domain: code-quality
lens: immutability
labels:
  - "audit:code-quality/immutability"
---

## Summary
The `EventBus` class in `components/cdc_core/src/EventBus.cpp` maintains mutable internal state (handler array, queue pointer, initialized flag) that can be accessed from multiple contexts (tasks and ISRs) without proper synchronization. While the class has some ISR-safe publishing via `publish(event, true)`, the subscription state itself is not protected.

**File affected:**
- `components/cdc_core/include/cdc_core/EventBus.h` (lines 128-142): Handler array and state
- `components/cdc_core/src/EventBus.cpp` (lines 48-70): `subscribe()`, `unsubscribe()` methods

## Impact
- **Race conditions**: `subscribe()` and `unsubscribe()` modify the `handlers_` array without locks. If called from different tasks or from an ISR context, this could lead to corrupted state
- **Memory visibility**: Without memory barriers, changes to `handlers_[i].active` might not be visible to other cores (ESP32-S3 is dual-core)
- **Event dispatch consistency**: During `process()`, handlers are iterated while another task could be modifying the array

## Evidence
From `components/cdc_core/src/EventBus.cpp:48-70`:
```cpp
uint8_t EventBus::subscribe(EventHandler handler, uint32_t mask) {
    if (!handler) return 0;

    for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
        if (!handlers_[i].active) {
            handlers_[i].handler = handler;  // Non-atomic write
            handlers_[i].mask = mask;        // Non-atomic write
            handlers_[i].active = true;      // Non-atomic write
            LOG_D(TAG, "Handler %u subscribed (mask: 0x%08lx)", i + 1, mask);
            return i + 1;
        }
    }
    LOG_E(TAG, "No free handler slots");
    return 0;
}
```

From `components/cdc_core/src/EventBus.cpp:121-139`:
```cpp
void EventBus::process() {
    if (!initialized_ || !queue_) return;

    Event event;
    while (xQueueReceive(static_cast<QueueHandle_t>(queue_), &event, 0) == pdTRUE) {
        uint32_t typeMask = 1u << static_cast<uint8_t>(event.type);

        for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
            if (handlers_[i].active && handlers_[i].handler) {  // Reading mutable state
                if (handlers_[i].mask == 0 || (handlers_[i].mask & typeMask)) {
                    handlers_[i].handler(event);  // Callback could re-enter subscribe/unsubscribe
                }
            }
        }
    }
}
```

Note: The `publish()` method has ISR-safe version but `subscribe()`/`unsubscribe()` do not.

## Recommended Fix
1. **Option A: Add mutex protection** (if FreeRTOS mutexes are available):
   ```cpp
   class EventBus {
   private:
       struct Subscription {
           EventHandler handler;
           uint32_t mask;
           bool active;
       };
       Subscription handlers_[MAX_HANDLERS] = {};
       void* queue_ = nullptr;
       bool initialized_ = false;
       SemaphoreHandle_t mutex_;  // Add mutex
       
   public:
       bool init(size_t queueSize = DEFAULT_QUEUE_SIZE) {
           mutex_ = xSemaphoreCreateMutex();
           // ... rest of init
       }
       
       uint8_t subscribe(EventHandler handler, uint32_t mask = 0) {
           xSemaphoreTake(mutex_, portMAX_DELAY);
           // ... subscribe logic
           xSemaphoreGive(mutex_);
       }
   };
   ```

2. **Option B: Use atomic operations** (for simple flags):
   ```cpp
   struct Subscription {
       EventHandler handler;
       uint32_t mask;
       std::atomic<bool> active;  // Use atomic for the flag
   };
   ```

3. **Option C: Constrain subscription to initialization phase**: Document that `subscribe()` should only be called during system initialization before multi-core operation begins.

4. **Option D: Copy handlers before dispatch**: In `process()`, copy the handler array to a local array before iterating, so modifications during callback don't affect the current iteration.

This is a Medium severity issue because the EventBus is central to system operation and race conditions could cause subtle, hard-to-debug issues.

## References
- ESP32-S3 is dual-core, so race conditions are a real concern
- FreeRTOS documentation: [Mutexes](https://www.freertos.org/a00107.html)
- C++ Core Guidelines [C.50](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c50-use-atomic-if-you-want-a-simple-shared-counter): Use `atomic` if you want a simple shared counter
- "Concurrency in Action" by Anthony Williams - chapter on shared data
