---
title: "[MEDIUM] EventBus handler array lacks thread-safety for concurrent subscribe/unsubscribe"
severity: MEDIUM
domain: resource-contention
lens: concurrency
labels:
  - "audit:concurrency/resource-contention"
---

## Summary
The `EventBus` class in `components/cdc_core/src/EventBus.cpp` stores event handlers in a static array without any synchronization. When `subscribe()` or `unsubscribe()` is called concurrently with `process()`, there's a data race on the `handlers_` array. This can cause handlers to be missed, called twice, or crash if a handler pointer is read mid-update.

**Locations:**
- `components/cdc_core/src/EventBus.cpp:48-60` - subscribe()
- `components/cdc_core/src/EventBus.cpp:69-75` - unsubscribe()
- `components/cdc_core/src/EventBus.cpp:121-139` - process()
- `components/cdc_core/include/cdc_core/EventBus.h:138` - handlers_ array

## Impact
1. **Data Race**: Multiple threads can call `subscribe()`/`unsubscribe()` while `process()` iterates the array, causing torn reads/writes.
2. **Missed Events**: If a handler is added/removed mid-iteration, events may not be delivered to all registered handlers.
3. **Crash Risk**: If `process()` calls a handler that was just unsubscribed (and freed), it can crash.
4. **Handler Duplication**: Concurrent `subscribe()` calls may result in the same handler being added multiple times.

**Evidence:**
```cpp
// EventBus.cpp:subscribe (lines 48-60)
uint8_t EventBus::subscribe(EventHandler handler, uint32_t mask) {
    if (!handler) return 0;
    
    for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
        if (!handlers_[i].active) {  // <-- Read without lock
            handlers_[i].handler = handler;  // <-- Write without lock
            handlers_[i].mask = mask;        // <-- Write without lock
            handlers_[i].active = true;      // <-- Write without lock
            LOG_D(TAG, "Handler %u subscribed (mask: 0x%08lx)", i + 1, mask);
            return i + 1;
        }
    }
    
    LOG_E(TAG, "No free handler slots");
    return 0;
}

// EventBus.cpp:unsubscribe (lines 69-75)
void EventBus::unsubscribe(uint8_t id) {
    if (id == 0 || id > MAX_HANDLERS) return;
    
    handlers_[id - 1].active = false;  // <-- Write without lock
    handlers_[id - 1].handler = nullptr;  // <-- Write without lock
    LOG_D(TAG, "Handler %u unsubscribed", id);
}

// EventBus.cpp:process (lines 121-139)
void EventBus::process() {
    if (!initialized_ || !queue_) return;
    
    Event event;
    while (xQueueReceive(static_cast<QueueHandle_t>(queue_),
                         &event, 0) == pdTRUE) {
        // Dispatch to all matching handlers
        uint32_t typeMask = 1u << static_cast<uint8_t>(event.type);
        
        for (uint8_t i = 0; i < MAX_HANDLERS; i++) {  // <-- Iterate without lock
            if (handlers_[i].active && handlers_[i].handler) {  // <-- Read without lock
                // Check mask (0 = receive all)
                if (handlers_[i].mask == 0 ||
                    (handlers_[i].mask & typeMask)) {
                    handlers_[i].handler(event);  // <-- Call without lock
                }
            }
        }
    }
}

// EventBus.h:handlers_ (line 138)
Subscription handlers_[MAX_HANDLERS] = {};  // No mutex protection
```

**Race condition sequence:**
```
Thread A (subscribe):           Thread B (process):
                                for (i=0; i<16; i++) {
                                    if (handlers_[i].active) {  // Read i=5
                                        handlers_[i].handler(event);
for (i=0; i<16; i++) {
    if (!handlers_[i].active) {  // Read i=5, not active
        handlers_[i].handler = fn1;
        handlers_[i].mask = mask1;
        handlers_[i].active = true;
        return 6;
                                    }
                                }
                                // Continue loop
```

If Thread A adds a handler while Thread B is iterating, the new handler may or may not be called depending on loop state.

```
Thread A (unsubscribe):         Thread B (process):
handlers_[5].active = false;    for (i=0; i<16; i++) {
handlers_[5].handler = nullptr;     if (handlers_[i].active && handlers_[i].handler) {
                                        // i=5, reads active=false (just set)
                                        // Handler skipped!
                                    }
                                }
```

Handler is unsubscribed but process() may have already read the pointer, leading to a use-after-free if the handler was dynamically allocated.

## Recommended Fix
Add a lightweight mutex to protect handler array access:

```cpp
// EventBus.h
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
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;  // Add spinlock
};

// EventBus.cpp
uint8_t EventBus::subscribe(EventHandler handler, uint32_t mask) {
    if (!handler) return 0;
    
    portENTER_CRITICAL(&mux_);
    for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
        if (!handlers_[i].active) {
            handlers_[i].handler = handler;
            handlers_[i].mask = mask;
            handlers_[i].active = true;
            portEXIT_CRITICAL(&mux_);
            LOG_D(TAG, "Handler %u subscribed (mask: 0x%08lx)", i + 1, mask);
            return i + 1;
        }
    }
    portEXIT_CRITICAL(&mux_);
    
    LOG_E(TAG, "No free handler slots");
    return 0;
}

void EventBus::unsubscribe(uint8_t id) {
    if (id == 0 || id > MAX_HANDLERS) return;
    
    portENTER_CRITICAL(&mux_);
    handlers_[id - 1].active = false;
    handlers_[id - 1].handler = nullptr;
    portEXIT_CRITICAL(&mux_);
    LOG_D(TAG, "Handler %u unsubscribed", id);
}

void EventBus::process() {
    if (!initialized_ || !queue_) return;
    
    Event event;
    while (xQueueReceive(static_cast<QueueHandle_t>(queue_),
                         &event, 0) == pdTRUE) {
        uint32_t typeMask = 1u << static_cast<uint8_t>(event.type);
        
        // Copy handlers to local array to minimize critical section
        Subscription snapshot[MAX_HANDLERS];
        portENTER_CRITICAL(&mux_);
        for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
            snapshot[i] = handlers_[i];
        }
        portEXIT_CRITICAL(&mux_);
        
        // Dispatch from snapshot
        for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
            if (snapshot[i].active && snapshot[i].handler) {
                if (snapshot[i].mask == 0 ||
                    (snapshot[i].mask & typeMask)) {
                    snapshot[i].handler(event);
                }
            }
        }
    }
}
```

**Key changes:**
1. Add `portMUX_TYPE` spinlock for lightweight protection
2. Use `portENTER_CRITICAL`/`portEXIT_CRITICAL` for short critical sections
3. Copy handlers to local snapshot before dispatch to minimize lock hold time
4. Handlers called outside critical section to avoid blocking other operations

## References
- [ESP32 FreeRTOS Spinlocks](https://docs.freertos.org/Using-a-short-fast-spinlock-instead-of-a-mutex.html)
- [Read-Copy-Update Pattern](https://lwn.net/Articles/373105/) - Snapshot pattern for iteration
- [Event Bus Concurrency](https://www.embedded.com/design/prototyping-and-development/4045011/Event-bus-design-patterns-for-embedded-systems) - Thread-safe event dispatch
