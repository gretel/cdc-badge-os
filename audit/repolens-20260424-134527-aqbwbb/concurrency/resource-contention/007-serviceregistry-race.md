---
title: "[LOW] ServiceRegistry initAll/startAll not thread-safe for concurrent module registration"
severity: LOW
domain: resource-contention
lens: concurrency
labels:
  - "audit:concurrency/resource-contention"
---

## Summary
The `ServiceRegistry` class in `components/cdc_core/src/ServiceRegistry.cpp` stores services in a static array without synchronization. If modules register services concurrently with `initAll()` or `startAll()` being called, there's a data race on the `services_` array.

**Locations:**
- `components/cdc_core/src/ServiceRegistry.cpp:38-63` - registerService()
- `components/cdc_core/src/ServiceRegistry.cpp:131-148` - initAll()
- `components/cdc_core/src/ServiceRegistry.cpp:151-168` - startAll()
- `components/cdc_core/include/cdc_core/ServiceRegistry.h:126` - services_ array

## Impact
1. **Data Race**: Concurrent `registerService()` calls with `initAll()` can cause torn reads/writes.
2. **Initialization Order Issues**: Services registered after `initAll()` starts may not be initialized.
3. **Crash Risk**: If `initAll()` calls `init()` on a service that's mid-registration, it may use uninitialized data.

**Evidence:**
```cpp
// ServiceRegistry.cpp:registerService (lines 38-63)
bool ServiceRegistry::registerService(const char* name, IService* service) {
    if (!name || !service) {
        LOG_E(TAG, "Invalid parameters");
        return false;
    }
    
    if (count_ >= MAX_SERVICES) {
        LOG_E(TAG, "Registry full, cannot register '%s'", name);
        return false;
    }
    
    // Check for duplicate name
    for (size_t i = 0; i < count_; i++) {  // <-- Read count_ without lock
        if (strcmp(services_[i].name, name) == 0) {  // <-- Read services_ without lock
            LOG_E(TAG, "Service '%s' already registered", name);
            return false;
        }
    }
    
    services_[count_].name = name;  // <-- Write without lock
    services_[count_].service = service;  // <-- Write without lock
    count_++;  // <-- Increment without lock (not atomic!)
    
    LOG_I(TAG, "Registered service '%s'", name);
    return true;
}

// ServiceRegistry.cpp:initAll (lines 131-148)
bool ServiceRegistry::initAll() {
    LOG_I(TAG, "Initializing %u services...", count_);  // <-- Read count_ without lock
    
    for (size_t i = 0; i < count_; i++) {  // <-- Iterate without lock
        LOG_I(TAG, "  [%u/%u] %s", i + 1, count_, services_[i].name);  // <-- Read without lock
        
        if (!services_[i].service->init()) {  // <-- Read service without lock
            LOG_E(TAG, "Failed to initialize '%s'", services_[i].name);
            return false;
        }
    }
    
    LOG_I(TAG, "All services initialized");
    return true;
}

// ServiceRegistry.cpp:startAll (lines 151-168)
bool ServiceRegistry::startAll() {
    LOG_I(TAG, "Starting %u services...", count_);  // <-- Read count_ without lock
    
    for (size_t i = 0; i < count_; i++) {  // <-- Iterate without lock
        if (services_[i].service->getState() == ServiceState::INITIALIZED ||
            services_[i].service->getState() == ServiceState::STOPPED) {
            
            if (!services_[i].service->start()) {  // <-- Read service without lock
                LOG_E(TAG, "Failed to start '%s'", services_[i].name);
                return false;
            }
        }
    }
    
    LOG_I(TAG, "All services started");
    return true;
}

// ServiceRegistry.h:services_ (line 126)
Entry services_[MAX_SERVICES] = {};  // No mutex protection
size_t count_ = 0;  // Not atomic
```

**Race condition sequence:**
```
Thread A (Module Init):         Thread B (Main Loop):
    registerService("module1", &m1)   initAll()
        if (count_ >= MAX_SERVICES)       for (i=0; i<count_; i++)
            count_ = 0, OK                    i=0, count_=0
        for (i=0; i<count_; i++)          // Loop doesn't start
            count_ = 0, no duplicates
        services_[0].name = "module1"
        services_[0].service = &m1
        count_++  // count_ = 1
        
    registerService("module2", &m2)
        if (count_ >= MAX_SERVICES)
            count_ = 1, OK
        for (i=0; i<count_; i++)
            i=0, services_[0].name = "module1"
            strcmp("module1", "module2") != 0
        services_[1].name = "module2"
        services_[1].service = &m2
        count_++  // count_ = 2
        
                                for (i=0; i<count_; i++)  // Re-eval count_
                                    count_ = 2
                                    i=0, services_[0].name
                                    services_[0].service->init()
                                    i=1, services_[1].name
                                    services_[1].service->init()
```

This looks fine. The issue is with **non-atomic increment**:

```
Thread A:                       Thread B:
    count_ >= MAX_SERVICES          count_ >= MAX_SERVICES
        count_ = 2, OK                  count_ = 2, OK
    services_[2].name = ...         services_[2].name = ...
    services_[2].service = ...      services_[2].service = ...
    count_++  // count_ = 3         count_++  // count_ = 3 (lost increment!)
```

Both threads read `count_ = 2`, write to `services_[2]`, and increment to 3. One service is lost.

**Another race - initAll during registration:**
```
Thread A (Module):              Thread B (Main):
    registerService("mod1", &m1)  initAll()
        if (count_ >= MAX)            for (i=0; i<count_; i++)
            count_ = 0, OK                count_ = 0
        for (i=0; i<count_; i++)        // Loop doesn't start
            count_ = 0, no dup
        services_[0].name = "mod1"  // Partial write
        services_[0].service = &m1  // Partial write
        count_++  // count_ = 1
        
                                    for (i=0; i<count_; i++)
                                        count_ = 1
                                        i=0, services_[0].name
                                        // Reads "mod1"
                                        services_[0].service
                                        // Reads &m1
                                        services_[0].service->init()
```

This also looks fine since the writes are sequential. The real issue is when **count_ increment is not atomic**:

```
Thread A:                       Thread B:
    uint8_t temp = count_;        uint8_t temp = count_;  // temp = 0
    temp++;                       temp++;                   // temp = 1
    count_ = temp;  // count_ = 1  count_ = temp;  // count_ = 1 (lost!)
```

## Recommended Fix
Add synchronization for service registration:

```cpp
// ServiceRegistry.h
class ServiceRegistry {
private:
    Entry services_[MAX_SERVICES] = {};
    size_t count_ = 0;
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;  // Add spinlock
};

// ServiceRegistry.cpp
bool ServiceRegistry::registerService(const char* name, IService* service) {
    if (!name || !service) {
        LOG_E(TAG, "Invalid parameters");
        return false;
    }
    
    portENTER_CRITICAL(&mux_);
    
    if (count_ >= MAX_SERVICES) {
        portEXIT_CRITICAL(&mux_);
        LOG_E(TAG, "Registry full, cannot register '%s'", name);
        return false;
    }
    
    // Check for duplicate name
    for (size_t i = 0; i < count_; i++) {
        if (strcmp(services_[i].name, name) == 0) {
            portEXIT_CRITICAL(&mux_);
            LOG_E(TAG, "Service '%s' already registered", name);
            return false;
        }
    }
    
    services_[count_].name = name;
    services_[count_].service = service;
    count_++;
    
    portEXIT_CRITICAL(&mux_);
    LOG_I(TAG, "Registered service '%s'", name);
    return true;
}

bool ServiceRegistry::initAll() {
    portENTER_CRITICAL(&mux_);
    
    // Copy service pointers to local array
    Entry snapshot[MAX_SERVICES];
    size_t localCount = count_;
    for (size_t i = 0; i < count_; i++) {
        snapshot[i] = services_[i];
    }
    
    portEXIT_CRITICAL(&mux_);
    
    LOG_I(TAG, "Initializing %u services...", localCount);
    
    for (size_t i = 0; i < localCount; i++) {
        LOG_I(TAG, "  [%u/%u] %s", i + 1, localCount, snapshot[i].name);
        
        if (!snapshot[i].service->init()) {
            LOG_E(TAG, "Failed to initialize '%s'", snapshot[i].name);
            return false;
        }
    }
    
    LOG_I(TAG, "All services initialized");
    return true;
}
```

**Alternative:** Use a two-phase registration pattern where modules register during a known initialization window before `initAll()` is called.

## References
- [ESP32 FreeRTOS Spinlocks](https://docs.freertos.org/Using-a-short-fast-spinlock-instead-of-a-mutex.html)
- [Service Locator Pattern](https://www.codeproject.com/Articles/532525/Service-Locator-Pattern) - Thread-safe variants
- [Initialization Order Fiasco](https://isocpp.org/wiki/faq/ctors#static-init-order) - C++ static initialization
