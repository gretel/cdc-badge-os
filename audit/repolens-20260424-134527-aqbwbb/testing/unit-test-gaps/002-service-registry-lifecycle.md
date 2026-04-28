---
title: "[HIGH] ServiceRegistry Lifecycle Methods Lack Unit Test Coverage"
severity: HIGH
domain: Testing
lens: unit-test-gaps
labels:
  - "audit:testing/unit-test-gaps"
---

## Summary
The `ServiceRegistry` class (`components/cdc_core/src/ServiceRegistry.cpp`, 189 lines) manages service discovery and lifecycle for the entire system but has zero unit tests. Key untested functions include:

- `registerService()` (line 38) - Service registration with duplicate detection
- `getService()` (line 70) - Service lookup by name
- `registerTypedService()` (line 92) - Typed service provision
- `getTypedService()` (line 113) - Typed service retrieval
- `isAvailable()` (line 126) - Service availability check
- `initAll()` (line 134) - Batch initialization
- `startAll()` (line 150) - Batch start with state checks
- `stopAll()` (line 176) - Batch stop in reverse order

## Impact
**System Stability Risk:** ServiceRegistry is the core dependency injection container. Bugs here would affect all services:
1. Registry overflow (MAX_SERVICES=24) is untested
2. Duplicate name detection may have edge cases
3. `initAll()` returns early on first failure - untested partial initialization
4. `startAll()` has complex state logic (INITIALIZED/STOPPED → STARTED)
5. `stopAll()` reverse order iteration could have off-by-one errors

## Evidence
File: `components/cdc_core/src/ServiceRegistry.cpp`

Line 38-62: `registerService()` - Has 3 exit paths (null params, registry full, duplicate)
```cpp
if (!name || !service) {
    LOG_E(TAG, "Invalid parameters");
    return false;  // Path 1
}
if (count_ >= MAX_SERVICES) {
    LOG_E(TAG, "Registry full...");
    return false;  // Path 2
}
for (size_t i = 0; i < count_; i++) {
    if (strcmp(services_[i].name, name) == 0) {
        LOG_E(TAG, "Service '%s' already registered", name);
        return false;  // Path 3
    }
}
```

Line 134-148: `initAll()` - Early return on failure
```cpp
for (size_t i = 0; i < count_; i++) {
    if (!services_[i].service->init()) {
        LOG_E(TAG, "Failed to initialize '%s'", services_[i].name);
        return false;  // Untested early return
    }
}
```

Line 150-169: `startAll()` - State-dependent logic
```cpp
if (services_[i].service->getState() == ServiceState::INITIALIZED ||
    services_[i].service->getState() == ServiceState::STOPPED) {
    if (!services_[i].service->start()) {
        return false;  // Untested
    }
}
```

Current test coverage:
```bash
$ find test/ -name "*.cpp" -exec grep -l "ServiceRegistry" {} \;
# Returns nothing - no ServiceRegistry tests exist
```

## Recommended Fix
Create `test/test_service_registry/test_service_registry.cpp` with test cases:

1. **Registration tests:**
   - Test successful registration
   - Test null name/service rejection
   - Test registry full condition (24 services)
   - Test duplicate name rejection

2. **Lookup tests:**
   - Test `getService()` returns correct pointer
   - Test `getService()` returns nullptr for missing name
   - Test typed `get<IDisplay>()` casting

3. **Lifecycle tests:**
   - Test `initAll()` with mix of success/failure
   - Test `startAll()` respects state machine
   - Test `stopAll()` stops in reverse order

4. **Typed service tests:**
   - Test `provide<>()` and `request<>()`
   - Test `isAvailable()` returns correct state
   - Test service type index bounds checking

Example test:
```cpp
void test_registerService_duplicate() {
    auto& reg = ServiceRegistry::instance();
    IService* svc1 = new MockService("test");
    IService* svc2 = new MockService("test");
    
    TEST_ASSERT_TRUE(reg.registerService("test", svc1));
    TEST_ASSERT_FALSE(reg.registerService("test", svc2));  // Duplicate
}

void test_initAll_partial_failure() {
    auto& reg = ServiceRegistry::instance();
    reg.registerService("svc1", new SuccessService());
    reg.registerService("svc2", new FailInitService());
    
    TEST_ASSERT_FALSE(reg.initAll());  // Should fail on svc2
    // Verify svc1 was initialized, svc2 was not
}
```

## References
- File: `components/cdc_core/include/cdc_core/ServiceRegistry.h` - Full API
- Pattern: Service Locator / Dependency Injection
