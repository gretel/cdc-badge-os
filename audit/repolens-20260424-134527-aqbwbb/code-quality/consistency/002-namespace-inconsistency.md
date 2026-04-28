---
title: "[MEDIUM] Namespace Usage Inconsistency"
severity: MEDIUM
domain: code-structure
lens: code-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The codebase shows inconsistent namespace usage patterns:
1. **using namespace directives**: Mixed usage across files
2. **namespace declarations**: Some headers use namespaces, others don't
3. **std namespace**: CalEPD uses `using namespace std;` in headers

### Evidence

**using namespace std in headers (CalEPD):**
```cpp
// components/CalEPD/include/epdspi.h
using namespace std;

// components/CalEPD/include/color/wave5i7ColorVector.h
using namespace std;
```

**using namespace cdc::ui (mixed usage):**
```cpp
// components/mod_nvsedit/src/NvsEditModule.cpp
using namespace cdc::ui;
using namespace cdc::core;

// components/mod_os_ui/src/WifiMenuUi.cpp
using namespace cdc::ui;

// components/main/main.cpp
using namespace cdc::core;
```

**Local scope using:**
```cpp
// components/mod_hid/src/BleHidKeyboard.cpp
    using namespace hal;

// components/mod_ble_serial/src/BleUartService.cpp
    using namespace cdc::hal;
```

**Headers with namespaces (inconsistent):**
- `components/cdc_core/include/cdc_core/ServiceRegistry.h` - uses `namespace cdc::core`
- `components/cdc_ui/include/cdc_ui/I18n.h` - uses `namespace cdc::ui`
- `components/mod_totp/include/mod_totp/TotpModule.h` - uses `namespace cdc::mod_totp`

## Impact
- **Cognitive overhead**: Developers must check each file for namespace conventions
- **Potential name collisions**: `using namespace std;` in headers affects all includers
- **Inconsistent code style**: Some files use fully qualified names, others use using declarations

## Recommended Fix
1. **Remove `using namespace std;` from CalEPD headers** - Use `std::` prefix instead
2. **Establish consistent using directive policy**:
   - Prefer `using namespace X;` in .cpp files only
   - Use fully qualified names in .h files
3. **Document namespace convention** in a style guide

### Files to fix:
- `components/CalEPD/include/epdspi.h`
- `components/CalEPD/include/color/wave5i7ColorVector.h`

## References
- C++ Core Guidelines: Use `using namespace` sparingly, prefer in .cpp files
- Project architecture: Modular design with clear namespace boundaries
