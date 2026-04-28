---
title: "[MEDIUM] Transitive circular dependency: cdc_ui → cdc_hal → cdc_core → cdc_log → usb_badge → cdc_core"
severity: MEDIUM
domain: architecture/circular-deps
lens: circular-dependency
labels:
  - "audit:architecture/circular-deps"
---

## Summary
A 5-hop transitive circular dependency exists involving five components: `cdc_ui`, `cdc_hal`, `cdc_core`, `cdc_log`, and `usb_badge`. This cycle is longer than the documented 3-hop and 4-hop variants and adds `cdc_ui` to the dependency chain.

**Dependency cycle:**
```
cdc_ui → cdc_hal → cdc_core → cdc_log → usb_badge → cdc_core
```

**Files involved:**
- `components/cdc_ui/CMakeLists.txt` (line 10-12)
- `components/cdc_hal/CMakeLists.txt` (line 21-22)
- `components/cdc_core/CMakeLists.txt` (line 22-23)
- `components/cdc_log/CMakeLists.txt` (line 8-9)
- `components/usb_badge/CMakeLists.txt` (line 5)

## Impact
1. **Build order complexity**: The 5-component cycle creates additional ambiguity in the ESP-IDF build system.
2. **UI framework coupling**: `cdc_ui` (the UI framework component) is now part of the core circular dependency chain, making the UI more tightly coupled to foundational components.
3. **Testing difficulty**: Testing `cdc_ui` in isolation requires the full chain (`cdc_hal`, `cdc_core`, `cdc_log`, `usb_badge`) to be built first.
4. **Refactoring coupling**: Changes to any component in this chain ripple through all others, including the UI framework.
5. **Initialization order fragility**: The UI framework depends on hardware abstraction which depends on core services which depend on logging which depends on USB implementation.

**Note:** This is a longer variant of the cycles documented in issues 004 and 005. Fixing the core cycles will also resolve this.

## Evidence
**Dependency chain from CMakeLists.txt files:**

1. **cdc_ui/CMakeLists.txt** (line 10-12):
```cmake
    REQUIRES
        cdc_core      # <-- Depends on cdc_core
        cdc_hal       # <-- Also depends on cdc_hal
        cdc_log       # <-- Also depends on cdc_log
```

2. **cdc_hal/CMakeLists.txt** (line 21):
```cmake
    REQUIRES
        cdc_core      # <-- Depends on cdc_core
```

3. **cdc_core/CMakeLists.txt** (line 22-23):
```cmake
    REQUIRES
        cdc_hal       # <-- Depends on cdc_hal (creates cycle)
        cdc_log       # <-- Also depends on cdc_log
```

4. **cdc_log/CMakeLists.txt** (line 8-9):
```cmake
    REQUIRES
        usb_badge     # <-- Depends on usb_badge
```

5. **usb_badge/CMakeLists.txt** (line 5):
```cmake
    REQUIRES cdc_core cdc_log freertos driver espressif__tinyusb
             #     ^^^^^^^^
             #     |
             #     +-- Cycle back to cdc_core
```

**Actual usage pattern:**
- `cdc_ui` provides the core UI framework (I18n, ViewStack, IView)
- `cdc_hal` provides hardware abstraction interfaces
- `cdc_core` provides core services (IService, ServiceRegistry)
- `cdc_log` provides logging over USB CDC
- `usb_badge` provides USB CDC/HID implementation

The cycle exists because:
1. `cdc_ui` needs `cdc_hal` for hardware-aware UI (display, keypad)
2. `cdc_ui` needs `cdc_core` for service registry
3. `cdc_hal` needs `cdc_core` for base `IService` interface
4. `cdc_core` needs `cdc_log` for logging
5. `cdc_log` needs `usb_badge` for USB CDC functionality
6. `usb_badge` needs `cdc_core` for feature flags

## Recommended Fix
This cycle is part of the larger architectural issue involving the core components. The following fixes (from issue 004) will also resolve this:

### Option 1: Move IService to a shared base component (Most robust)
Create a new `cdc_base` component:
1. Move `IService.h`, `ServiceState` enum to `components/cdc_base/`
2. `cdc_core` REQUIRES `cdc_base`
3. `cdc_hal` REQUIRES `cdc_base`
4. `cdc_ui` REQUIRES `cdc_base`
5. Remove `cdc_core` from `cdc_hal` and `cdc_ui` REQUIRES

**CMakeLists.txt changes:**
```cmake
// components/cdc_hal/CMakeLists.txt
REQUIRES
    cdc_base      // Instead of cdc_core

// components/cdc_ui/CMakeLists.txt
REQUIRES
    cdc_base      // Instead of cdc_core
    cdc_hal
    cdc_log
```

### Option 2: Remove cdc_core dependency from usb_badge (Quick fix)
If `usb_badge` only uses `cdc_core/feature_flags.h`:
1. Move `feature_flags.h` to a shared location or `cdc_log`
2. Update `usb_badge` to REQUIRES only `cdc_log`

**CMakeLists.txt changes:**
```cmake
// components/usb_badge/CMakeLists.txt
REQUIRES cdc_log freertos driver espressif__tinyusb
         # ^^^^^^^^ Only cdc_log, not cdc_core
```

### Option 3: Make cdc_log dependency on usb_badge PRIVATE
If `cdc_log` only needs `tusb.h` (from espressif__tinyusb):
```cmake
// components/cdc_log/CMakeLists.txt
REQUIRES
    esp_timer
    espressif__tinyusb  // For tusb.h
PRIV_REQUIRES
    usb_badge           // Private linking only
```

**Recommended approach:**
Start with Option 2 (move feature_flags) as it's the quickest fix, then consider Option 1 for long-term architectural cleanliness.

## References
- Related issue: #004 (4-hop cycle: cdc_core → cdc_hal → cdc_log → usb_badge → cdc_core)
- Related issue: #005 (3-hop cycle: cdc_core → cdc_log → usb_badge → cdc_core)
- [ESP-IDF Component Dependencies](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#component-dependencies)
- [Transitive Dependency](https://en.wikipedia.org/wiki/Transitive_dependency)
- [Dependency Inversion Principle](https://en.wikipedia.org/wiki/Dependency_inversion_principle)
