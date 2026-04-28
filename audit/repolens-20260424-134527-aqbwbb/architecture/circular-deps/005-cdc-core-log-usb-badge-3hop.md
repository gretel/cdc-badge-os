---
title: "[LOW] Short transitive cycle: cdc_core → cdc_log → usb_badge → cdc_core"
severity: LOW
domain: architecture/circular-deps
lens: circular-dependency
labels:
  - "audit:architecture/circular-deps"
---

## Summary
A 3-hop transitive circular dependency exists between `cdc_core`, `cdc_log`, and `usb_badge`. This cycle is shorter than the 4-hop variant (see related issue 004) but represents the same architectural tight coupling.

**Dependency cycle:**
```
cdc_core → cdc_log → usb_badge → cdc_core
```

**Files involved:**
- `components/cdc_core/CMakeLists.txt` (line 23)
- `components/cdc_log/CMakeLists.txt` (line 8-9)
- `components/usb_badge/CMakeLists.txt` (line 5)

## Impact
1. **Build order complexity**: Even a 3-component cycle creates ambiguity in the build system.
2. **Logging initialization**: `cdc_log` (foundational logging) depends on `usb_badge` (USB implementation), which depends back on `cdc_core`.
3. **Coupling**: Core components should have minimal dependencies on implementation-specific components like `usb_badge`.
4. **Testing**: Testing `cdc_core` requires `usb_badge` to be built first, even if only for linking.

**Note:** This is a subset of the longer cycle documented in issue 004. Fixing issue 004 will also resolve this cycle.

## Evidence
**Dependency chain:**

1. **cdc_core/CMakeLists.txt** (line 23):
```cmake
    REQUIRES
        ...
        cdc_log       # <-- Depends on cdc_log
```

2. **cdc_log/CMakeLists.txt** (line 8-9):
```cmake
    REQUIRES
        usb_badge     # <-- Depends on usb_badge
        esp_timer
```

3. **usb_badge/CMakeLists.txt** (line 5):
```cmake
    REQUIRES cdc_core cdc_log freertos driver espressif__tinyusb
             #     ^^^^^^^^
             #     |
             #     +-- Creates cycle back to cdc_core
```

**Actual usage:**
- `cdc_core` REQUIRES `cdc_log` for logging functionality
- `cdc_log` REQUIRES `usb_badge` for USB CDC (`tusb.h`)
- `usb_badge` REQUIRES `cdc_core` for feature flags (`cdc_core/feature_flags.h`)

## Recommended Fix
This cycle is resolved as a side effect of fixing issue 004. However, if addressing independently:

### Option 1: Move feature_flags to cdc_log
If `usb_badge` only needs `cdc_core/feature_flags.h`:
1. Move `feature_flags.h` into `cdc_log` component
2. Update `usb_badge` to REQUIRES `cdc_log` only
3. `cdc_core` can still REQUIRES `cdc_log`

**CMakeLists.txt changes:**
```cmake
// components/usb_badge/CMakeLists.txt
REQUIRES cdc_log freertos driver espressif__tinyusb
         // ^^^^^^^^ Removed cdc_core
```

### Option 2: Make usb_badge dependency PRIVATE in cdc_log
```cmake
// components/cdc_log/CMakeLists.txt
REQUIRES
    esp_timer
    espressif__tinyusb  // For tusb.h
PRIV_REQUIRES
    usb_badge           // Private for linking only
```

## References
- Related issue: #004 (longer 4-hop cycle involving cdc_hal)
- [ESP-IDF Component Dependencies](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#component-dependencies)
- [ESP-IDF PRIV_REQUIRES](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#id13)
