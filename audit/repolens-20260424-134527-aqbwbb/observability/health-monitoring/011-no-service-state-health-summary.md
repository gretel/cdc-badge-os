---
title: "[LOW] No service state health summary endpoint"
severity: LOW
domain: observability
lens: health-monitoring
labels:
  - "audit:observability/health-monitoring"
---

## Summary

While individual services (HAL components, modules) track their lifecycle state via `ServiceState` enum (UNINITIALIZED, INITIALIZED, STARTED, STOPPED, ERROR), there is **no command or API to query the aggregate health of all services**.

**Service state tracking exists:**
- `IService::getState()` returns current state (defined in `components/cdc_core/include/cdc_core/IService.h:10-16`)
- All HAL services implement state tracking (e.g., `BQ25895Power`, `Tropic01Element`, `I2cBus`)
- `ServiceRegistry` manages all services and knows their states

**But no health summary is exposed:**
- `STATUS` command only reports heap and uptime (`components/serial_cmd/src/SerialCmd.cpp:427-434`)
- No command like `SERVICES` or `STATES` to list all service states
- No aggregated health metric (e.g., "5/7 services ready")

**Current state tracking implementation** (`components/cdc_hal/src/BQ25895Power.cpp:112`):
```cpp
core::ServiceState state_ = core::ServiceState::UNINITIALIZED;

bool init() {
    if (state_ != core::ServiceState::UNINITIALIZED) {
        return state_ == core::core::ServiceState::INITIALIZED ||
               state_ == core::ServiceState::STARTED;
    }
    // ... initialization ...
    state_ = core::ServiceState::INITIALIZED;
    return true;
}

bool start() {
    if (state_ == core::ServiceState::INITIALIZED ||
        state_ == core::ServiceState::STOPPED) {
        state_ = core::ServiceState::STARTED;
        return true;
    }
    return false;
}

core::ServiceState getState() const override { return state_; }
```

## Impact

1. **Debugging difficulty**: Cannot quickly see which service failed to initialize or start
2. **No health dashboard**: External monitoring tools cannot query service health
3. **Manual troubleshooting**: Developers must run individual commands (TR01_STATUS, etc.) to check each component
4. **Lost initialization context**: If a module fails, there's no way to see if it's because a dependency service is not ready

## Evidence

**Service state definition:**
- `components/cdc_core/include/cdc_core/IService.h:10-16` - ServiceState enum

**Service registry** (`components/cdc_core/include/cdc_core/ServiceRegistry.h:28-118`):
- `registerService()` - Registers services
- `initAll()` - Initializes all services
- `startAll()` - Starts all services
- `count()` - Returns service count
- **Missing**: No `getServiceState(name)` or `getAllStates()` method

**Current STATUS command** (`components/serial_cmd/src/SerialCmd.cpp:427-434`):
```cpp
static void cmdStatus(const char* args) {
    (void)args;
    Console::printf("=== System Status ===\r\n");
    Console::printf("Free heap: %lu bytes\r\n", (unsigned long)esp_get_free_heap_size());
    Console::printf("Min free heap: %lu bytes\r\n", (unsigned long)esp_get_minimum_free_heap_size());
    Console::printf("Uptime: %llu ms\r\n", esp_timer_get_time() / 1000ULL);
    Console::flush();
}
```

**Search results:**
```bash
grep -r "getState()" components/cdc_core/src/ServiceRegistry.cpp  # Only used internally
grep -r "ServiceState" components/serial_cmd/  # 0 matches (STATUS command doesn't use it)
```

## Recommended Fix

Add a service state health summary to the STATUS command:

**Step 1: Add health query method to ServiceRegistry** (`components/cdc_core/include/cdc_core/ServiceRegistry.h`):
```cpp
struct ServiceHealth {
    const char* name;
    ServiceState state;
};

/**
 * \brief Get health state of all registered services
 * \param out Array to fill with health data
 * \param maxCount Maximum array size
 * \return Number of services written
 */
uint8_t getServiceHealth(ServiceHealth* out, uint8_t maxCount);
```

**Step 2: Implement the method** (`components/cdc_core/src/ServiceRegistry.cpp`):
```cpp
uint8_t ServiceRegistry::getServiceHealth(ServiceHealth* out, uint8_t maxCount) {
    if (!out || maxCount == 0) return 0;
    
    uint8_t count = 0;
    for (size_t i = 0; i < count_ && i < maxCount; i++) {
        out[count].name = services_[i].name;
        out[count].state = services_[i].service->getState();
        count++;
    }
    return count;
}
```

**Step 3: Update STATUS command** (`components/serial_cmd/src/SerialCmd.cpp`):
```cpp
static void cmdStatus(const char* args) {
    (void)args;
    Console::printf("=== System Status ===\r\n");
    Console::printf("Free heap: %lu bytes\r\n", (unsigned long)esp_get_free_heap_size());
    Console::printf("Min free heap: %lu bytes\r\n", (unsigned long)esp_get_minimum_free_heap_size());
    Console::printf("Uptime: %llu ms\r\n", esp_timer_get_time() / 1000ULL);
    
    // Service health summary
    ServiceRegistry::ServiceHealth health[16];
    uint8_t count = ServiceRegistry::instance().getServiceHealth(health, 16);
    uint8_t ready = 0;
    for (uint8_t i = 0; i < count; i++) {
        const char* stateStr = "UNKNOWN";
        if (health[i].state == ServiceState::STARTED) { stateStr = "READY"; ready++; }
        else if (health[i].state == ServiceState::INITIALIZED) stateStr = "INIT";
        else if (health[i].state == ServiceState::STOPPED) stateStr = "STOPPED";
        else if (health[i].state == ServiceState::ERROR) stateStr = "ERROR";
        
        Console::printf("  %s: %s\r\n", health[i].name, stateStr);
    }
    Console::printf("Services: %u/%u ready\r\n", ready, count);
    
    Console::flush();
}
```

**Expected output example:**
```
=== System Status ===
Free heap: 245632 bytes
Min free heap: 198450 bytes
Uptime: 12345 ms
  i2c: READY
  power: READY
  keypad: READY
  display: READY
  secure_element: READY
  rtc: READY
Services: 6/6 ready
```

## References

- Kubernetes health probes: https://kubernetes.io/docs/tasks/configure-pod-container/configure-liveness-readiness-startup-probes/
- Service discovery patterns: https://www.microservices.com/patterns/service-discovery/

</content>