---
title: "[MEDIUM] Transitive circular dependency: mod_fido2 → usb_badge → cdc_log → usb_badge"
severity: MEDIUM
domain: architecture/circular-deps
lens: circular-dependency
labels:
  - "audit:architecture/circular-deps"
---

## Summary
A transitive circular dependency exists involving the FIDO2 module and core USB/logging components. The `mod_fido2` module depends on `usb_badge`, which in turn depends on `cdc_log`, which depends back on `usb_badge`.

**Dependency cycle:**
```
mod_fido2 → usb_badge → cdc_log → usb_badge
```

**Files involved:**
- `components/mod_fido2/CMakeLists.txt` (line 15-17)
- `components/usb_badge/CMakeLists.txt` (line 5)
- `components/cdc_log/CMakeLists.txt` (line 8-9)

## Impact
1. **Module coupling**: The FIDO2 module (a high-level feature) is tightly coupled to the USB implementation details.
2. **Build order complexity**: The 3-component cycle creates ambiguity in the build system.
3. **Testing difficulty**: Testing `mod_fido2` in isolation requires `usb_badge` and `cdc_log` to be built first due to the circular dependency.
4. **Refactoring friction**: Changes to `usb_badge` or `cdc_log` may unexpectedly affect the FIDO2 module.
5. **Initialization order**: The FIDO2 module's USB HID transport depends on USB initialization which depends on logging which depends on USB.

## Evidence
**Dependency chain from CMakeLists.txt files:**

1. **mod_fido2/CMakeLists.txt** (line 15-17):
```cmake
    REQUIRES
        usb_badge     # <-- Depends on usb_badge
        cdc_core
        cdc_ui
        cdc_views
        cdc_hal
        cdc_log
        freertos
```

2. **usb_badge/CMakeLists.txt** (line 5):
```cmake
    REQUIRES cdc_core cdc_log freertos driver espressif__tinyusb
                   #     ^^^^^^^^
                   #     |
                   #     +-- Creates cycle
```

3. **cdc_log/CMakeLists.txt** (line 8-9):
```cmake
    REQUIRES
        usb_badge     # <-- Depends back on usb_badge
        esp_timer
```

**Actual usage pattern:**
- `mod_fido2` uses `usb_badge` for FIDO2 HID transport (CTAPHID)
- `usb_badge` uses `cdc_log` for logging
- `cdc_log` uses `usb_badge` for USB CDC functionality (uses `tusb.h`)

The cycle exists because:
1. `mod_fido2` needs `usb_badge` for USB HID interface
2. `usb_badge` needs `cdc_log` for logging
3. `cdc_log` needs `usb_badge` for USB CDC (uses `tusb.h` from usb_badge)

## Recommended Fix
Break the cycle by refactoring one of the following ways:

### Option 1: Remove usb_badge from cdc_log REQUIRES (Recommended)
If `cdc_log` only needs `tusb.h` (which comes from `espressif__tinyusb` directly):

**CMakeLists.txt changes:**
```cmake
// components/cdc_log/CMakeLists.txt
REQUIRES
    esp_timer
    espressif__tinyusb  // For tusb.h directly
    # usb_badge         // Remove this
```

**Verification:**
Check `cdc_log/src/cdc_log.cpp` to confirm it only uses `tusb.h` and not any `usb_badge` specific headers:
```cpp
#include "tusb.h"  // From espressif__tinyusb, not usb_badge
```

### Option 2: Move USB CDC basics to cdc_log
If `cdc_log` needs more than just `tusb.h` from `usb_badge`:
1. Move the minimal USB CDC initialization code from `usb_badge` to `cdc_log`
2. Make `usb_badge` REQUIRES `cdc_log` only
3. Make `mod_fido2` REQUIRES `cdc_log` instead of `usb_badge`

### Option 3: Make usb_badge dependency PRIVATE in cdc_log
```cmake
// components/cdc_log/CMakeLists.txt
REQUIRES
    esp_timer
    espressif__tinyusb  // For tusb.h
PRIV_REQUIRES
    usb_badge           // Private for linking only
```

**Quick fix (Option 1):**
Edit `components/cdc_log/CMakeLists.txt` and replace `usb_badge` with `espressif__tinyusb`:
```cmake
idf_component_register(
    SRCS "src/cdc_log.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_timer
        espressif__tinyusb  // Instead of usb_badge
)
```

Then verify the build still works:
```bash
~/.platformio/penv/bin/pio run
```

## References
- [ESP-IDF Component Dependencies](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#component-dependencies)
- [ESP-IDF PRIV_REQUIRES](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#id13)
- Related findings: [002](002-cdc-log-usb-badge-circular.md) (cdc_log ↔ usb_badge direct cycle)
