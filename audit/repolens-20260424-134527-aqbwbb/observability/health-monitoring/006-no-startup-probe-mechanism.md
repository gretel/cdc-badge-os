---
title: "[MEDIUM] No startup probe mechanism for slow initialization"
severity: MEDIUM
domain: observability
lens: health-monitoring
labels:
  - "audit:observability/health-monitoring"
---

## Summary

The system lacks a startup probe mechanism to track completion of slow initialization processes, particularly the TROPIC storage cache rebuild which can take significant time on first boot or after cache invalidation.

**Current behavior:**
- `main.cpp` calls `tropicStorage.init()` and `tropicStorage.start()` sequentially (lines 183-187)
- The storage cache may need to rebuild from the secure element if `cacheValid_` is false
- `rebuildVerbose()` iterates through all R-Memory slots (potentially 512 slots) - a slow operation
- No mechanism to signal when this initialization is complete

**Evidence from code:**
```cpp
// main.cpp:183-187
auto& tropicStorage = cdc::core::TropicStorage::instance();
tropicStorage.setSecureElement(s_secureElement);
tropicStorage.init();
tropicStorage.start();
ServiceRegistry::instance().registerService("tropic_storage", &tropicStorage);
```

```cpp
// TropicStorage.cpp:38-42
cacheValid_ = loadHeader();
if (!cacheValid_) {
    LOG_W(TAG, "Cache header missing or stale - rebuild required");
    // Rebuild happens here but caller doesn't wait
}
```

## Impact

- **Premature readiness**: The system reports "ready" before slow initialization completes
- **Silent race conditions**: Modules that depend on the storage cache may start before it's populated
- **No visibility**: Operators cannot tell if initialization is still in progress
- **No timeout handling**: Slow initialization could hang indefinitely without detection

## Evidence

**File:** `components/cdc_core/include/cdc_core/TropicStorage.h:34` - Only provides `isCacheValid()` getter, no initialization state tracking

**File:** `components/cdc_core/src/TropicStorage.cpp:207-274` - `rebuildVerbose()` can process 512+ slots but has no progress reporting or completion callback

**File:** `main/main.cpp:183-187` - Storage cache initialized synchronously but rebuild may happen asynchronously in `start()`

## Recommended Fix

Add a startup probe mechanism to track slow initialization:

1. **Add initialization state tracking to TropicStorage:**
   ```cpp
   enum class InitState {
       UNINITIALIZED,
       INITIALIZING,     // Slow init in progress
       READY,            // Fully initialized
       NEEDS_REBUILD     // Cache valid but needs rebuild
   };
   
   class TropicStorage : public IService {
       InitState initState_ = InitState::UNINITIALIZED;
       bool isInitializing() const { return initState_ == InitState::INITIALIZING; }
       bool isReady() const { return initState_ == InitState::READY; }
   };
   ```

2. **Add a completion callback for slow initialization:**
   ```cpp
   using InitCallback = void(*)(bool success, void* ctx);
   void setInitCallback(InitCallback cb, void* ctx);
   ```

3. **Update STATUS command to report startup probe state:**
   ```
   STATUS - Include "Startup: COMPLETE" or "Startup: INITIALIZING (storage cache)"
   ```

4. **Add a STARTUP command for external health checks:**
   ```
   STARTUP - Show initialization state of all components
   STARTUP WAIT - Block until all startup probes pass (for boot scripts)
   ```

## References

- Kubernetes startup probes: https://kubernetes.io/docs/concepts/workloads/pods/pod-lifecycle/#startup-probe
- ESP32 initialization best practices: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/init.html
