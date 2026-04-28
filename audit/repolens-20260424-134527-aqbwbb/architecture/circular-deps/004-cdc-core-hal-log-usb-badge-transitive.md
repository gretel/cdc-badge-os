---
title: "[MEDIUM] Transitive circular dependency: cdc_core → cdc_hal → cdc_log → usb_badge → cdc_core"
severity: MEDIUM
domain: architecture/circular-deps
lens: circular-dependency
labels:
  - "audit:architecture/circular-deps"
---

## Summary
A transitive circular dependency exists involving four components: `cdc_core`, `cdc_hal`, `cdc_log`, and `usb_badge`. This is a longer dependency cycle that creates build order ambiguity and tight coupling between foundational components.

**Dependency cycle:**
```
cdc_core → cdc_hal → cdc_log → usb_badge → cdc_core
```

**Files involved:**
- `components/cdc_core/CMakeLists.txt` (line 22-23)
- `components/cdc_hal/CMakeLists.txt` (line 21-22)
- `components/cdc_log/CMakeLists.txt` (line 8-9)
- `components/usb_badge/CMakeLists.txt` (line 5)

## Impact
1. **Build system complexity**: The ESP-IDF build system must resolve a 4-component cycle, which can lead to non-deterministic build order.
2. **Initialization order fragility**: Components at different levels of the hierarchy depend on each other, making boot order critical.
3. **Testing difficulty**: Unit testing any of these foundational components requires the full chain to be built.
4. **Refactoring coupling**: Changes to `cdc_log` (a foundational logging component) ripple through `usb_badge`, `cdc_core`, and `cdc_hal`.
5. **Conceptual architecture violation**: `cdc_core` should be the foundational component, but it depends on `cdc_hal`, which depends on `cdc_log`, which depends on `usb_badge`, which depends back on `cdc_core`.

## Evidence
**Dependency chain from CMakeLists.txt files:**

1. **cdc_core/CMakeLists.txt** (line 22-23):
```cmake
    REQUIRES
        ...
        cdc_hal       # <-- Depends on cdc_hal
        cdc_log       # <-- Depends on cdc_log
```

2. **cdc_hal/CMakeLists.txt** (line 21-22):
```cmake
    REQUIRES
        cdc_core      # <-- Depends on cdc_core (direct cycle!)
        cdc_log       # <-- Also depends on cdc_log
```

3. **cdc_log/CMakeLists.txt** (line 8-9):
```cmake
    REQUIRES
        usb_badge     # <-- Depends on usb_badge
        esp_timer
```

4. **usb_badge/CMakeLists.txt** (line 5):
```cmake
    REQUIRES cdc_core cdc_log freertos driver espressif__tinyusb
             #     ^^^^^^^^  ^^^^^^^^
             #     |         |
             #     +---------+-- Both create cycles!
```

**Actual usage pattern:**
- `cdc_core` provides core services (IService, ServiceRegistry, ModuleRegistry)
- `cdc_hal` provides hardware abstraction interfaces (IDisplay, IKeypad, ISecureElement)
- `cdc_log` provides logging over USB CDC
- `usb_badge` provides USB CDC/HID implementation

The cycle exists because:
1. `cdc_core` needs `cdc_hal` for hardware-aware services (e.g., `AttestationKeyService` uses `ISecureElement`)
2. `cdc_hal` needs `cdc_core` for base `IService` interface
3. `cdc_log` needs `usb_badge` for USB CDC functionality (uses `tusb.h`)
4. `usb_badge` needs `cdc_core` and `cdc_log` for feature flags and logging

## Recommended Fix
Break the cycle by refactoring one of the following ways:

### Option 1: Move IService to a shared base component (Most robust)
Create a new `cdc_base` component:
1. Move `IService.h`, `ServiceState` enum to `components/cdc_base/`
2. `cdc_core` REQUIRES `cdc_base`
3. `cdc_hal` REQUIRES `cdc_base`
4. Remove `cdc_core` from `cdc_hal` REQUIRES

**CMakeLists.txt changes:**
```cmake
// components/cdc_hal/CMakeLists.txt
REQUIRES
    cdc_base      // Instead of cdc_core
    cdc_log
    ...
```

### Option 2: Remove cdc_core dependency from usb_badge (Quick fix)
If `usb_badge` only uses `cdc_core/feature_flags.h`:
1. Move `feature_flags.h` to a shared location or `cdc_log`
2. Update `usb_badge` to REQUIRES only `cdc_log`

**CMakeLists.txt changes:**
```cmake
// components/usb_badge/CMakeLists.txt
REQUIRES cdc_log freertos driver espressif__tinyusb
         // ^^^^^^^^ Only cdc_log, not cdc_core
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

### Option 4: Extract USB CDC basics to cdc_log
1. Move minimal USB CDC initialization from `usb_badge` to `cdc_log`
2. Make `usb_badge` REQUIRES `cdc_log` only
3. `cdc_core` and `cdc_hal` can still REQUIRES `cdc_log`

**Recommended approach:**
Start with Option 2 (move feature_flags) as it's the quickest fix, then consider Option 1 for long-term architectural cleanliness.

## References
- [ESP-IDF Component Dependencies](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#component-dependencies)
- [Transitive Dependency](https://en.wikipedia.org/wiki/Transitive_dependency)
- [Dependency Inversion Principle](https://en.wikipedia.org/wiki/Dependency_inversion_principle)
- Related issues: #001 (serial_cmd ↔ cdc_os_ui), #002 (cdc_log ↔ usb_badge), #003 (cdc_core ↔ cdc_hal)
