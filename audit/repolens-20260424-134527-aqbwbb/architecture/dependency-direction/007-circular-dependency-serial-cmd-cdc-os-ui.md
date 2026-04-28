---
title: "[HIGH] Circular dependency between serial_cmd and cdc_os_ui"
severity: HIGH
domain: architecture/dependency-direction
lens: dependency-direction
labels:
  - "audit:architecture/dependency-direction"
slug: circular-dependency-serial-cmd-cdc-os-ui
---

## Summary
There is a circular dependency between `serial_cmd` and `cdc_os_ui` components. Both components list each other in their `REQUIRES` section in CMakeLists.txt, but only `cdc_os_ui` actually uses code from `serial_cmd`.

**Evidence:**
- `components/serial_cmd/CMakeLists.txt` line 18: `cdc_os_ui` is listed in REQUIRES
- `components/cdc_os_ui/CMakeLists.txt` line 24: `serial_cmd` is listed in REQUIRES
- `components/cdc_os_ui/src/AppUi.cpp` line 13: `#include "serial_cmd/SerialCmd.h"` and uses `serial::SerialCmd`
- `components/serial_cmd/src/*.cpp`: No includes from `cdc_os_ui` found
- `components/serial_cmd/include/serial_cmd/*.h`: No includes from `cdc_os_ui` found

## Impact
**Maintenance Burden:** Circular dependencies make the codebase harder to understand, test, and modify. Changes to one component may require changes to the other, even if only one direction is actually used.

**Build Complexity:** While ESP-IDF may resolve this through transitive dependencies, it creates unnecessary coupling and can lead to build order issues.

**Architecture Violation:** The intended layered architecture is compromised. `serial_cmd` (infrastructure/command layer) should not depend on `cdc_os_ui` (OS-level UI layer).

## Evidence
**File: components/serial_cmd/CMakeLists.txt**
```cmake
idf_component_register(
    SRCS
        "src/CommandRegistry.cpp"
        "src/Console.cpp"
        "src/SerialCmd.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        cdc_core
        driver
        freertos
        esp_timer
        usb_badge
        cdc_log
        cdc_os_ui    # <-- Listed but not actually used
)
```

**File: components/cdc_os_ui/src/AppUi.cpp**
```cpp
#include "serial_cmd/SerialCmd.h"
// ...
serial::SerialCmd::setTextCallback([](const char* field, const char* value) {
```

**Verification:** No code in `serial_cmd` uses anything from `cdc_os_ui`:
```bash
$ grep -r "cdc_os_ui" components/serial_cmd/
components/serial_cmd/CMakeLists.txt:        cdc_os_ui  # Only in CMakeLists.txt, not in source
```

## Recommended Fix
Remove `cdc_os_ui` from the `REQUIRES` list in `components/serial_cmd/CMakeLists.txt`.

**Step 1:** Edit `components/serial_cmd/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS
        "src/CommandRegistry.cpp"
        "src/Console.cpp"
        "src/SerialCmd.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        cdc_core
        driver
        freertos
        esp_timer
        usb_badge
        cdc_log
        # cdc_os_ui removed - not actually used
)
```

**Step 2:** Verify the build still works:
```bash
~/.platformio/penv/bin/pio run
```

**Step 3:** Verify no code in `serial_cmd` actually needs `cdc_os_ui`:
- Check that no `#include "cdc_os_ui/..."` exists in source files
- Check that no symbols from `cdc_os_ui` are used

## References
- [Dependency Inversion Principle](https://en.wikipedia.org/wiki/Dependency_inversion_principle)
- [Clean Architecture: A Craftsman's Guide to Software Structure and Design](https://www.oreilly.com/library/view/clean-architectures-in/9781617293494/)
- CDC Badge OS Architecture: `CLAUDE.md` - "Module Architecture - CRITICAL" section
