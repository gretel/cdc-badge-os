---
title: "[LOW] Build-level circular dependency between cdc_core and cdc_hal"
severity: LOW
domain: architecture/circular-deps
lens: circular-dependency
labels:
  - "audit:architecture/circular-deps"
---

## Summary
A build-level circular dependency exists between `cdc_core` and `cdc_hal` components. However, unlike other circular dependencies, the header files form a valid Directed Acyclic Graph (DAG), making this less critical.

**Files involved:**
- `components/cdc_core/CMakeLists.txt` (line 8-10)
- `components/cdc_hal/CMakeLists.txt` (line 5-7)

**Dependency cycle:**
```
cdc_core --REQUIRES--> cdc_hal
cdc_hal  --REQUIRES--> cdc_core
```

## Impact
1. **Minimal runtime impact**: Since headers form a valid DAG, initialization order is well-defined.
2. **Build order ambiguity**: ESP-IDF build system may have non-deterministic build order.
3. **Conceptual coupling**: `cdc_core` should conceptually be the foundation, but it depends on `cdc_hal`.

**Why this is less critical:**
The header dependencies form a valid hierarchy:
```
cdc_core/IService.h (base, no deps)
    ↑
cdc_hal/*.h (depend on IService.h)
    ↑
cdc_core/AttestationKeyService.h, etc. (depend on cdc_hal/*.h)
```

This is a well-structured dependency graph where `IService.h` is the base interface.

## Evidence
**cdc_core/CMakeLists.txt:**
```cmake
idf_component_register(
    ...
    REQUIRES
        freertos
        esp_timer
        nvs_flash
        cdc_hal       # <-- Depends on cdc_hal
        cdc_log
        mbedtls
)
```

**cdc_hal/CMakeLists.txt:**
```cmake
idf_component_register(
    ...
    REQUIRES
        cdc_core      # <-- Depends on cdc_core
        cdc_log
        driver
        ...
)
```

**Header dependency analysis:**
- `cdc_core/include/cdc_core/IService.h` - No `cdc_hal` dependencies (base interface)
- `cdc_hal/include/cdc_hal/*.h` - All depend on `cdc_core/IService.h`
- `cdc_core/include/cdc_core/AttestationKeyService.h` - Depends on `cdc_hal/ISecureElement.h`
- `cdc_core/include/cdc_core/TropicStorage.h` - Depends on `cdc_hal/ISecureElement.h`

## Recommended Fix
While this is less critical, it can still be cleaned up for better architecture:

### Option 1: Keep as-is (Recommended for now)
Since the headers form a valid DAG and there's no runtime circular dependency, this can be left as-is. The build system will handle it correctly.

### Option 2: Move IService to a shared base component
Create a new `cdc_base` component:
1. Move `IService.h` and `ServiceState` enum to `cdc_base`
2. `cdc_core` REQUIRES `cdc_base`
3. `cdc_hal` REQUIRES `cdc_base`
4. No circular build dependency

### Option 3: Remove cdc_core from cdc_hal REQUIRES
If `cdc_hal` only needs `IService.h` (which could be considered a shared interface):
1. Check if `cdc_hal` uses anything else from `cdc_core`
2. If only `IService.h`, consider moving it to a shared location
3. Otherwise, remove `cdc_core` from `cdc_hal` REQUIRES if possible

**Current status:**
This circular dependency is acceptable given the well-structured header hierarchy. It should be monitored but doesn't require immediate action.

## References
- [Directed Acyclic Graph (DAG)](https://en.wikipedia.org/wiki/Directed_acyclic_graph)
- [ESP-IDF Component Dependencies](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#component-dependencies)
