---
title: "[MEDIUM] EventBus singleton has no event state tracking or subscription debugging"
severity: MEDIUM
domain: State Management Architecture
lens: state-architecture
labels:
  - "audit:architecture/state-architecture"
---

## Summary
The `EventBus` singleton (`components/cdc_core/include/cdc_core/EventBus.h`) uses static arrays for event handlers (`handlers_[MAX_HANDLERS]`) and a FreeRTOS queue for event buffering. There is no way to query current subscriptions, track event publishing statistics, or debug event flow.

### Evidence:
- `EventBus.h:131-135`: Static struct `Subscription { EventHandler handler; uint32_t mask; bool active; } handlers_[MAX_HANDLERS]`
- `EventBus.h:87-95`: `subscribe()` returns handler ID but no way to query subscriptions
- `EventBus.h:97-103`: `unsubscribe()` removes handler but no validation
- `EventBus.h:105-112`: `publish()` queues events but no statistics tracking

### State Access Pattern:
```cpp
// Subscribe to events
uint8_t id = EventBus::instance().subscribe(handler, mask);

// Publish events
EventBus::instance().publish(EventType::KEY_PRESSED);

// But no way to query:
// - How many handlers are subscribed?
// - Which handlers are subscribed to KEY_PRESSED?
// - How many events have been published?
```

## Impact
1. **Debugging difficulty**: No way to trace event flow during development
2. **Leaked subscriptions**: Handlers can be subscribed but never unsubscribed (no tracking)
3. **No event statistics**: Can't measure event frequency for performance tuning
4. **Race conditions**: Events can be published from ISR but no tracking of ISR vs main loop events

## Recommended Fix
1. Add subscription query API:
   ```cpp
   typedef struct {
       uint8_t id;
       EventHandler handler;
       uint32_t mask;
       bool active;
   } SubscriptionInfo;
   
   uint8_t getSubscriptionCount() const;
   SubscriptionInfo getSubscription(uint8_t id) const;
   uint8_t getHandlersForEvent(EventType type) const;  // Returns count of handlers for event
   ```

2. Add event statistics:
   ```cpp
   typedef struct {
       uint32_t publishedCount;
       uint32_t queuedCount;
       uint32_t processedCount;
       uint32_t droppedCount;  // Queue full
       uint32_t isrPublishCount;
   } EventStats;
   
   EventStats getStats() const;
   void resetStats();
   ```

3. Add subscription tracking:
   ```cpp
   uint8_t subscribe(EventHandler handler, uint32_t mask, const char* ownerModule);
   // Returns ID and tracks owner for debugging
   
   void unsubscribe(uint8_t id);
   // Logs warning if handler not found
   ```

4. Add event logging (debug build):
   ```cpp
   #ifdef DEBUG_MODE
   void logEvent(EventType type, bool fromISR);
   #endif
   ```

## References
- `components/cdc_core/include/cdc_core/EventBus.h` - Full header
- `components/cdc_core/src/EventBus.cpp` - Implementation
- `components/mod_fido2/src/Fido2Module.cpp` - Event subscription usage
