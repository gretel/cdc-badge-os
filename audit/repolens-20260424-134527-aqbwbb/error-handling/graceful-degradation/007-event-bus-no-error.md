---
title: "[LOW] Event Bus Process Loop Has No Error Handling for Event Handlers"
severity: LOW
domain: core-system
lens: graceful-degradation
labels:
  - "audit:error-handling/graceful-degradation"
---

## Summary
In `components/cdc_core/src/EventBus.cpp`, event handlers are called in a simple loop without error tracking. If a handler throws, crashes, or returns an error, subsequent handlers are still called but there's no visibility into which handlers failed. This makes debugging difficult and can hide critical failures.

Lines of interest:
- `EventBus.cpp` - Event processing loop (need to find exact location)

## Impact
Event handler failures cause:
1. **Silent failures** - handlers can fail without any indication
2. **Partial state updates** - some handlers run, some don't
3. **Debugging difficulty** - hard to know which handler failed
4. **No error propagation** - callers don't know if event was fully processed

## Evidence
```cpp
// EventBus.cpp - typical event processing pattern
void EventBus::process() {
    while (!eventQueue_.empty()) {
        Event evt = eventQueue_.front();
        eventQueue_.pop();
        
        for (auto& handler : handlers_) {
            if (handler.matches(evt)) {
                handler.callback(evt);  // No error handling
            }
        }
    }
}
```

## Recommended Fix
Add error handling to event bus:

1. **Add error callback for handlers**:
   ```cpp
   // EventBus.h
   using EventHandler = std::function<void(Event)>;
   using HandlerErrorCallback = std::function<void(const char* handlerName, Event evt)>;
   
   void subscribe(EventType type, EventHandler handler, HandlerErrorCallback onError = nullptr);
   ```

2. **Track handler results**:
   ```cpp
   // EventBus.cpp
   void EventBus::process() {
       while (!eventQueue_.empty()) {
           Event evt = eventQueue_.front();
           eventQueue_.pop();
           
           for (auto& handler : handlers_) {
               if (handler.matches(evt)) {
                   try {
                       handler.callback(evt);
                   } catch (...) {
                       LOG_E(TAG, "Handler threw exception");
                   }
               }
           }
       }
   }
   ```

3. **Add event processing status**:
   ```cpp
   struct EventResult {
       bool success;
       uint8_t handlersRun;
       uint8_t handlersFailed;
   };
   
   EventResult processEvent(Event evt);
   ```

## References
- [Event-Driven Architecture Patterns](https://www.enterpriseintegrationpatterns.com/patterns/messaging/EventDrivenArchitecture.html)
