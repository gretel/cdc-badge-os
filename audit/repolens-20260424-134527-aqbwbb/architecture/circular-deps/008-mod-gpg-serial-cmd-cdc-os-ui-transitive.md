---
title: "[MEDIUM] Transitive circular dependency: mod_gpg → serial_cmd → cdc_os_ui → mod_gpg"
severity: MEDIUM
domain: architecture/circular-deps
lens: circular-dependency
labels:
  - "audit:architecture/circular-deps"
---

## Summary
A transitive circular dependency exists involving the `mod_gpg` module and the `serial_cmd`/`cdc_os_ui` components. The `mod_gpg` module depends on both `serial_cmd` and `cdc_os_ui`, while `serial_cmd` also depends on `cdc_os_ui`, creating a diamond-shaped dependency pattern that can lead to circular initialization issues.

**Dependency pattern:**
```
mod_gpg → serial_cmd → cdc_os_ui
mod_gpg → cdc_os_ui
```

**Files involved:**
- `components/mod_gpg/CMakeLists.txt` (line 18-26)
- `components/mod_gpg/src/GpgModule.cpp` (includes `serial_cmd/ICommandRegistry.h`, `cdc_os_ui/views/PinChangeView.h`)
- `components/serial_cmd/CMakeLists.txt` (line 11-13)
- `components/cdc_os_ui/CMakeLists.txt` (line 18-29)

## Impact
1. **Module coupling**: The GPG module (a high-level feature) is tightly coupled to both the serial command interface and the OS UI, creating a diamond dependency pattern.
2. **Initialization order**: The GPG module may experience initialization order issues if `serial_cmd` and `cdc_os_ui` have conflicting initialization requirements.
3. **Testing difficulty**: Testing `mod_gpg` in isolation requires both `serial_cmd` and `cdc_os_ui` to be built first, increasing test complexity.
4. **Refactoring friction**: Changes to `serial_cmd` or `cdc_os_ui` may unexpectedly affect the GPG module.
5. **Build order complexity**: The ESP-IDF build system must resolve the diamond dependency, which can lead to non-deterministic build order.

## Evidence
**Dependency chain from CMakeLists.txt files:**

1. **mod_gpg/CMakeLists.txt** (line 18-26):
```cmake
    REQUIRES
        cdc_core
        cdc_ui
        cdc_views
        cdc_os_ui       # <-- Depends on cdc_os_ui
        cdc_hal
        usb_badge
        serial_cmd      # <-- Also depends on serial_cmd
        cdc_log
        ...
```

2. **serial_cmd/CMakeLists.txt** (line 11-13):
```cmake
    REQUIRES
        cdc_core
        driver
        freertos
        esp_timer
        usb_badge
        cdc_log
        cdc_os_ui       # <-- Depends on cdc_os_ui
```

3. **Actual usage in mod_gpg/src/GpgModule.cpp**:
```cpp
#include "serial_cmd/ICommandRegistry.h"  // Uses serial_cmd
#include "serial_cmd/Console.h"
#include "cdc_os_ui/views/PinChangeView.h"  // Uses cdc_os_ui
...
static ui::PinChangeView s_pinChangeView;   // Instantiates cdc_os_ui view
```

The diamond pattern exists because:
1. `mod_gpg` needs `serial_cmd` for CCID command registration
2. `mod_gpg` needs `cdc_os_ui` for PIN change UI (`PinChangeView`)
3. `serial_cmd` also needs `cdc_os_ui` for serial command UI integration
4. This creates a diamond: `mod_gpg` → both `serial_cmd` and `cdc_os_ui` → `cdc_os_ui`

## Recommended Fix
Break the diamond dependency by refactoring one of the following ways:

### Option 1: Extract shared PIN UI to a separate component (Recommended)
Create a new `cdc_pin_ui` component:
1. Move `PinChangeView` (and related PIN UI) to a new `components/cdc_pin_ui/`
2. `cdc_os_ui` REQUIRES `cdc_pin_ui`
3. `mod_gpg` REQUIRES `cdc_pin_ui` (instead of `cdc_os_ui` for PIN UI)
4. `serial_cmd` REQUIRES `cdc_pin_ui` if it needs PIN UI

**CMakeLists.txt changes:**
```cmake
// components/mod_gpg/CMakeLists.txt
REQUIRES
    ...
    cdc_pin_ui        // Instead of cdc_os_ui for PIN UI
    serial_cmd
    ...

// components/cdc_os_ui/CMakeLists.txt
REQUIRES
    ...
    cdc_pin_ui        // Reuse PIN UI component
```

### Option 2: Remove serial_cmd dependency from mod_gpg
If `mod_gpg` only uses `serial_cmd` for CCID registration:
1. Register CCID commands directly in `mod_gpg` module initialization
2. Use `CommandRegistry` directly without depending on `serial_cmd` component
3. Update `mod_gpg` to REQUIRES `cdc_core` only (for `CommandRegistry`)

**CMakeLists.txt changes:**
```cmake
// components/mod_gpg/CMakeLists.txt
REQUIRES
    ...
    // serial_cmd      // Remove this
    cdc_core          // Already has this for CommandRegistry
```

### Option 3: Move PIN UI to cdc_views
If `PinChangeView` is a generic UI component:
1. Move `PinChangeView` to `components/cdc_views/`
2. `cdc_os_ui` REQUIRES `cdc_views` (already does)
3. `mod_gpg` REQUIRES `cdc_views` (already does) instead of `cdc_os_ui`

**CMakeLists.txt changes:**
```cmake
// components/mod_gpg/CMakeLists.txt
REQUIRES
    ...
    cdc_views         // Use cdc_views for PinChangeView
    // cdc_os_ui       // Remove if only needed for PinChangeView
```

**Quick fix (Option 2):**
If `mod_gpg` can register its CCID commands without depending on `serial_cmd`:
1. Edit `components/mod_gpg/src/GpgModule.cpp` to use `CommandRegistry` directly
2. Remove `serial_cmd` from `mod_gpg/CMakeLists.txt` REQUIRES list
3. Verify build still works

```bash
~/.platformio/penv/bin/pio run
```

## References
- [Diamond Dependency Problem](https://en.wikipedia.org/wiki/Diamond_(disambiguation)#Diamond_inheritance)
- [ESP-IDF Component Dependencies](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#component-dependencies)
- Related findings: [001](001-serial-cmd-cdc-os-ui-circular.md) (serial_cmd ↔ cdc_os_ui cycle)
