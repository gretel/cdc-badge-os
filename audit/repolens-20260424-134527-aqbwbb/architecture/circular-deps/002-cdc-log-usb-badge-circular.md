---
title: "[MEDIUM] Circular dependency between cdc_log and usb_badge components"
severity: MEDIUM
domain: architecture/circular-deps
lens: circular-dependency
labels:
  - "audit:architecture/circular-deps"
---

## Summary
A circular dependency exists between the `cdc_log` and `usb_badge` components at the CMake build level. Both components list each other in their `REQUIRES` clause.

**Files involved:**
- `components/cdc_log/CMakeLists.txt` (line 8-10)
- `components/usb_badge/CMakeLists.txt` (line 5-6)

**Dependency cycle:**
```
cdc_log    --REQUIRES--> usb_badge
usb_badge  --REQUIRES--> cdc_log
```

## Impact
1. **Build order ambiguity**: The ESP-IDF build system may struggle to determine the correct compilation order.
2. **Tight coupling**: `cdc_log` should be a foundational logging library that other components depend on, not one that depends on a higher-level component like `usb_badge`.
3. **Initialization order**: Logging should be available early in the boot process, before USB initialization.
4. **Testing difficulty**: Testing `cdc_log` in isolation requires `usb_badge` to be built first.

## Evidence
**cdc_log/CMakeLists.txt:**
```cmake
idf_component_register(
    SRCS
        "src/cdc_log.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        usb_badge       # <-- Depends on usb_badge
        esp_timer
)
```

**usb_badge/CMakeLists.txt:**
```cmake
idf_component_register(
    SRCS "usb_cdc.cpp" "usb_hid.cpp"
    INCLUDE_DIRS "include"
    REQUIRES cdc_core cdc_log freertos driver espressif__tinyusb  # <-- Depends on cdc_log
    PRIV_REQUIRES usb
)
```

**Actual usage:**
- `usb_badge/usb_cdc.cpp` includes `cdc_log.h` and uses `LOG_I()`, `LOG_D()` macros.
- `usb_badge/usb_hid.cpp` includes `cdc_log.h` and uses `LOG_I()`, `LOG_D()` macros.
- `cdc_log/src/cdc_log.cpp` includes `tusb.h` (from usb_badge's dependencies) but does NOT include any `usb_badge` headers directly.

The `cdc_log` component uses `tusb.h` for USB CDC functionality, which creates the need for `usb_badge` in the build. However, this is a runtime dependency, not necessarily a build dependency.

## Recommended Fix
Break the circular dependency by refactoring:

### Option 1: Remove usb_badge from cdc_log REQUIRES (Recommended)
If `cdc_log` only needs `tusb.h` (which comes from `espressif__tinyusb`), update `cdc_log/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS
        "src/cdc_log.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        # usb_badge      <-- Remove this
        esp_timer
        espressif__tinyusb  # Add this for tusb.h
)
```

This works because `cdc_log` uses `tusb.h` directly, not any `usb_badge` specific headers.

### Option 2: Extract USB CDC basics to cdc_log
If `cdc_log` needs more than just `tusb.h` from `usb_badge`:
1. Move the minimal USB CDC initialization code from `usb_badge` to `cdc_log`
2. Make `usb_badge` REQUIRES `cdc_log` only
3. This makes `cdc_log` a true foundational component

### Option 3: Make usb_badge dependency PRIVATE
If `cdc_log` only needs `usb_badge` for linking (not for headers):
```cmake
idf_component_register(
    SRCS "src/cdc_log.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer
    PRIV_REQUIRES usb_badge  # Private dependency only for this component
)
```

**Verification:**
After making changes, verify the build works:
```bash
~/.platformio/penv/bin/pio run
```

Also verify that logging works correctly at boot by checking serial output.

## References
- [ESP-IDF Component Dependencies](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#component-dependencies)
- [ESP-IDF PRIV_REQUIRES](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#id13)
- [Dependency Inversion Principle](https://en.wikipedia.org/wiki/Dependency_inversion_principle)
