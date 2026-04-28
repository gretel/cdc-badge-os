---
title: "[LOW] Mixed singleton and service access patterns across components"
severity: LOW
domain: architecture
lens: pattern-consistency
labels:
  - "audit:code-quality/pattern-consistency"
---

## Summary

The codebase uses multiple patterns for singleton and service access, which creates minor inconsistency. While most modules use the **function-local static singleton** pattern, HAL components use **file-scope static variables with getter functions**, and some services use **ServiceRegistry** for typed access.

**Files affected:**
- `components/mod_hid/include/mod_hid/BleHidKeyboard.h` - Class-based singleton
- `components/cdc_hal/src/BQ25895Power.cpp` - File-scope static with getter
- `components/cdc_hal/include/cdc_hal/IDisplay.h` - Getter function pattern

### Pattern 1: Module singletons (most common)

```cpp
// In TotpModule.cpp
TotpModule& TotpModule::instance() {
    static TotpModule inst;  // Function-local static
    return inst;
}

// Usage:
auto& module = TotpModule::instance();
```

### Pattern 2: HAL components (file-scope static)

```cpp
// In BQ25895Power.cpp
static BQ25895Power g_powerManager;  // File-scope static

IPowerManager* getPowerManagerInstance() {
    return &g_powerManager;  // Returns pointer
}

// Usage:
auto* power = getPowerManagerInstance();
```

### Pattern 3: ServiceRegistry (typed services)

```cpp
// In IKeyboardProvider.cpp
IKeyboardProvider* getKeyboard() {
    return ServiceRegistry::instance().request<IKeyboardProvider>(ServiceType::KEYBOARD);
}

// Usage:
auto* kb = core::getKeyboard();
```

## Impact

1. **Cognitive overhead**: Developers need to remember which access pattern to use for different components
2. **Inconsistent return types**: Some return references (`Module::instance()`), others return pointers (`get*Instance()`)
3. **Different initialization timing**: Function-local statics are lazy (first use), file-scope statics are eager (at startup)

## Evidence

**Module pattern (TotpModule):**
- `components/mod_totp/src/TotpModule.cpp:921` - `TotpModule& TotpModule::instance() { static TotpModule inst; return inst; }`

**HAL pattern (BQ25895Power):**
- `components/cdc_hal/src/BQ25895Power.cpp:587` - `static BQ25895Power g_powerManager;`
- `components/cdc_hal/src/BQ25895Power.cpp:593` - `IPowerManager* getPowerManagerInstance() { return &g_powerManager; }`

**HAL pattern (IDisplay):**
- `components/cdc_hal/src/EpaperDisplay.cpp` - `static EpaperDisplay s_display;`
- `components/cdc_hal/include/cdc_hal/IDisplay.h:149` - `IDisplay* getDisplayInstance();`

**ServiceRegistry pattern:**
- `components/cdc_core/src/IKeyboardProvider.cpp:6` - `IKeyboardProvider* getKeyboard() { return ServiceRegistry::instance().request<...>(); }`

## Recommended Fix

Document the existing patterns as intentional variations, since each serves a specific purpose:

1. **Modules**: Use `Module::instance()` with function-local static
   - Lazy initialization (only when first accessed)
   - Returns reference for direct access
   - Example: `TotpModule::instance()`

2. **HAL**: Use `get*Instance()` with file-scope static
   - Eager initialization (at startup)
   - Returns pointer for optional access (can be nullptr)
   - Example: `getDisplayInstance()`

3. **Optional Services**: Use `ServiceRegistry::request<T>()`
   - Runtime registration/discovery
   - Can be provided by multiple implementations
   - Example: `getKeyboard()`

Alternatively, consider standardizing:
- Change HAL to use function-local statics for consistency
- Or change modules to use getter functions for optional access

## References

- C++17 "Meyers' singleton" pattern (function-local static)
- `components/cdc_core/include/cdc_core/ServiceRegistry.h` - Service registry documentation
- `components/cdc_hal/include/cdc_hal/IDisplay.h` - HAL interface documentation
