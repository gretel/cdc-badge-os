---
title: "[MEDIUM] Missing dependency declaration: cdc_core uses usb_badge but doesn't declare it"
severity: MEDIUM
domain: architecture/dependency-direction
lens: dependency-direction
labels:
  - "audit:architecture/dependency-direction"
slug: missing-dependency-cdc-core-usb-badge
---

## Summary
The `cdc_core` component includes `usb_badge/usb_hid.h` in `src/UsbManager.cpp` but does not declare `usb_badge` as a dependency in its `CMakeLists.txt`. This creates a fragile build that may break depending on transitive dependency resolution.

**Evidence:**
- `components/cdc_core/src/UsbManager.cpp` line 3: `#include "usb_badge/usb_hid.h"`
- `components/cdc_core/CMakeLists.txt` lines 18-23: `usb_badge` NOT listed in REQUIRES

## Impact
**Build Fragility:** The component may compile only because `usb_badge` is pulled in as a transitive dependency from `main/CMakeLists.txt`. If the dependency graph changes, the build could break silently.

**Architecture Clarity:** `cdc_core` (core/domain layer) depending on `usb_badge` (hardware/infrastructure layer) represents a dependency direction issue. Core components should depend on abstractions, not concrete hardware implementations.

**Maintainability:** Future developers may not understand the coupling between `cdc_core` and `usb_badge` since it's not explicitly declared.

## Evidence
**File: components/cdc_core/src/UsbManager.cpp**
```cpp
#include "cdc_core/UsbManager.h"
#include "cdc_log.h"
#include "usb_badge/usb_hid.h"  // <-- Uses usb_badge
#include <string.h>
```

**File: components/cdc_core/CMakeLists.txt**
```cmake
idf_component_register(
    SRCS
        "src/AttestationKeyService.cpp"
        "src/ServiceRegistry.cpp"
        "src/EventBus.cpp"
        "src/PinManager.cpp"
        "src/ModuleRegistry.cpp"
        "src/TropicStorage.cpp"
        "src/TropicSlotMap.cpp"
        "src/KeyFingerprint.cpp"
        "src/UsbManager.cpp"
        "src/IKeyboardProvider.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        freertos
        esp_timer
        nvs_flash
        cdc_hal
        cdc_log
        mbedtls
        # usb_badge missing!
)
```

**Current Dependency Graph:**
```
main
├── cdc_core (uses usb_badge but doesn't declare it)
└── usb_badge (only declared here)
```

**What actually works:**
```bash
$ grep "usb_badge" main/CMakeLists.txt
REQUIRES nvs_flash freertos esp_timer cdc_core usb_badge serial_cmd cdc_hal cdc_log cdc_os_ui
```
The build works because `main` declares both `cdc_core` and `usb_badge`, and ESP-IDF's transitive dependency resolution makes `usb_badge` headers available.

## Recommended Fix
Add `usb_badge` to the `REQUIRES` list in `components/cdc_core/CMakeLists.txt`.

**Step 1:** Edit `components/cdc_core/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS
        "src/AttestationKeyService.cpp"
        "src/ServiceRegistry.cpp"
        "src/EventBus.cpp"
        "src/PinManager.cpp"
        "src/ModuleRegistry.cpp"
        "src/TropicStorage.cpp"
        "src/TropicSlotMap.cpp"
        "src/KeyFingerprint.cpp"
        "src/UsbManager.cpp"
        "src/IKeyboardProvider.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        freertos
        esp_timer
        nvs_flash
        cdc_hal
        cdc_log
        mbedtls
        usb_badge  # <-- Add this
)
```

**Step 2:** Verify the build still works:
```bash
~/.platformio/penv/bin/pio run
```

**Step 3 (Optional - Architecture Improvement):** Consider whether `cdc_core` should depend on `usb_badge` directly. A better architecture might be:
- Define a `IUsbManager` interface in `cdc_core`
- Implement it in `usb_badge`
- Have `cdc_core` depend on the interface, not the implementation

However, this is a larger refactor and the immediate fix is to declare the existing dependency.

## References
- [ESP-IDF Component Dependencies](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#id11)
- CDC Badge OS Architecture: `CLAUDE.md` - "Module Architecture - CRITICAL" section
- [Dependency Inversion Principle](https://en.wikipedia.org/wiki/Dependency_inversion_principle)
