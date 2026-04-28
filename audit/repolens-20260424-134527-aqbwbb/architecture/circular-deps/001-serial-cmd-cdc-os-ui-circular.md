---
title: "[HIGH] Circular dependency between serial_cmd and cdc_os_ui components"
severity: HIGH
domain: architecture/circular-deps
lens: circular-dependency
labels:
  - "audit:architecture/circular-deps"
---

## Summary
A circular dependency exists between the `serial_cmd` and `cdc_os_ui` components at the CMake build level. Both components list each other in their `REQUIRES` clause, creating a bidirectional build dependency.

**Files involved:**
- `components/serial_cmd/CMakeLists.txt` (line 11-13)
- `components/cdc_os_ui/CMakeLists.txt` (line 18-20)

**Dependency cycle:**
```
serial_cmd --REQUIRES--> cdc_os_ui
cdc_os_ui  --REQUIRES--> serial_cmd
```

## Impact
1. **Build order ambiguity**: The build system may struggle to determine the correct compilation order, potentially causing non-deterministic builds.
2. **Tight coupling**: Components should be modular and independent; circular dependencies make them interdependent and harder to maintain.
3. **Initialization order issues**: Runtime initialization may fail if one component expects the other to be fully initialized first.
4. **Testing difficulty**: Unit testing either component in isolation becomes harder due to the circular build dependency.
5. **Refactoring friction**: Changes to one component may unexpectedly break the other.

## Evidence
**serial_cmd/CMakeLists.txt:**
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
        cdc_os_ui       # <-- Depends on cdc_os_ui
)
```

**cdc_os_ui/CMakeLists.txt:**
```cmake
idf_component_register(
    SRCS
        "src/HardwareInfo.cpp"
        "src/AppUi.cpp"
        ...
    INCLUDE_DIRS
        "include"
    REQUIRES
        cdc_core
        cdc_hal
        cdc_log
        cdc_ui
        cdc_views
        serial_cmd      # <-- Depends on serial_cmd
        Adafruit-GFX
        ...
)
```

**Actual usage:**
- `cdc_os_ui/src/AppUi.cpp` includes `serial_cmd/SerialCmd.h` and uses `serial::SerialCmd::setTextCallback()` and `serial::SerialCmd::setTimeCallback()`.
- `serial_cmd/src/*.cpp` does NOT include any `cdc_os_ui` headers directly, suggesting the dependency may be unnecessary or could be reduced.

## Recommended Fix
Break the circular dependency by refactoring one of the following ways:

### Option 1: Move shared functionality to cdc_core (Recommended)
If `serial_cmd` needs functionality from `cdc_os_ui`, consider moving that functionality to `cdc_core` which both already depend on:
1. Identify what `serial_cmd` actually uses from `cdc_os_ui`
2. Move that functionality to `components/cdc_core/`
3. Update `serial_cmd/CMakeLists.txt` to REQUIRES `cdc_core` instead of `cdc_os_ui`

### Option 2: Remove unnecessary dependency from serial_cmd
If `serial_cmd` doesn't actually need `cdc_os_ui` at build time:
1. Verify what `serial_cmd` needs from `cdc_os_ui`
2. Check if it's just for linking (could use forward declarations)
3. Remove `cdc_os_ui` from `serial_cmd/CMakeLists.txt` REQUIRES list

### Option 3: Create a shared interface component
Create a new component (e.g., `cdc_serial_ui`) that both depend on:
1. Extract shared interfaces/types to new component
2. `cdc_os_ui` REQUIRES `cdc_serial_ui`
3. `serial_cmd` REQUIRES `cdc_serial_ui`
4. Both depend on the shared interface, not each other

**Quick fix (if Option 2 is viable):**
Edit `components/serial_cmd/CMakeLists.txt` and remove `cdc_os_ui` from the REQUIRES list:
```cmake
    REQUIRES
        cdc_core
        driver
        freertos
        esp_timer
        usb_badge
        cdc_log
        # cdc_os_ui  <-- Remove this line
```

Then verify the build still works:
```bash
~/.platformio/penv/bin/pio run
```

## References
- [ESP-IDF Component Dependencies](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#component-dependencies)
- [Dependency Inversion Principle](https://en.wikipedia.org/wiki/Dependency_inversion_principle)
- [Circular Dependency - Wikipedia](https://en.wikipedia.org/wiki/Circular_dependency)
