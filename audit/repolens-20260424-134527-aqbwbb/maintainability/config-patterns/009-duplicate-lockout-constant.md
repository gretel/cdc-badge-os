---
title: "[MEDIUM] Duplicate lockout duration constant defined in two locations"
severity: MEDIUM
domain: maintainability/config-patterns
lens: config-patterns
labels:
  - "audit:maintainability/config-patterns"
---

## Summary
The lockout duration constant `LOCKOUT_DURATION_MS` is defined in TWO separate header files with the same value (60000ms), creating a maintenance burden and risk of drift:

1. `components/cdc_core/include/cdc_core/PinManager.h:71` - Core PIN management
2. `components/cdc_views/include/cdc_views/PinEntryView.h:23` - UI view component

This is a classic configuration pattern issue where related constants are duplicated instead of being centralized.

## Impact
1. **Maintenance burden**: Changing the lockout duration requires editing two files
2. **Drift risk**: If one is updated and the other isn't, inconsistent behavior occurs
3. **Confusion**: Developers may not know which constant to use or if they should be the same
4. **No single source of truth**: Hard to find all lockout-related configuration in one place

## Evidence
**Duplicate definitions:**

```cpp
// components/cdc_core/include/cdc_core/PinManager.h:71
static constexpr uint32_t LOCKOUT_DURATION_MS = 60000;  // 60 seconds

// components/cdc_views/include/cdc_views/PinEntryView.h:23
static constexpr uint32_t LOCKOUT_DURATION_MS = 60000;  // 1 minute lockout
```

**Usage in code:**

```cpp
// components/cdc_core/src/PinManager.cpp
LOG_W(TAG, "Lockout started for %lu ms", LOCKOUT_DURATION_MS);
if (elapsed >= LOCKOUT_DURATION_MS) {
    // unlock logic
}
return LOCKOUT_DURATION_MS - elapsed;

// components/cdc_views/src/PinEntryView.cpp (uses same constant)
// The view references the constant from its own header
```

**Related duplicate constants found:**
- `MAX_SCAN_RESULTS` / `BLE_MAX_SCAN_RESULTS`: Defined in both `BluetoothController.cpp` and `BluetoothMenuUi.cpp` (both = 16)

## Recommended Fix

### Option 1: Centralize in Config.h (Recommended)
Create a centralized configuration for all lockout-related constants:

1. **Create/update `components/cdc_core/include/cdc_core/Config.h`**:
    ```cpp
    namespace cdc::config {
    namespace security {
    // Lockout configuration
    constexpr uint32_t PIN_LOCKOUT_MS = 60000;  // 60 seconds
    constexpr uint8_t PIN_MAX_RETRIES = 3;
    
    // BLE configuration
    constexpr uint8_t BLE_MAX_SCAN_RESULTS = 16;
    }
    }
    ```

2. **Update PinManager.h**:
    ```cpp
    #include "cdc_core/Config.h"
    
    class PinManager {
    public:
        static constexpr uint32_t LOCKOUT_DURATION_MS = cdc::config::security::PIN_LOCKOUT_MS;
        static constexpr uint8_t MAX_RETRIES = cdc::config::security::PIN_MAX_RETRIES;
    ```

3. **Update PinEntryView.h**:
    ```cpp
    #include "cdc_core/Config.h"
    
    class PinEntryView {
    public:
        static constexpr uint32_t LOCKOUT_DURATION_MS = cdc::config::security::PIN_LOCKOUT_MS;
    ```

4. **Update BluetoothController.cpp and BluetoothMenuUi.cpp**:
    ```cpp
    #include "cdc_core/Config.h"
    
    static constexpr uint8_t MAX_SCAN_RESULTS = cdc::config::security::BLE_MAX_SCAN_RESULTS;
    ```

### Option 2: Cross-reference (Quick Fix)
If centralized config is not feasible yet, at minimum cross-reference the constant:

```cpp
// components/cdc_views/include/cdc_views/PinEntryView.h
#include "cdc_core/PinManager.h"

namespace cdc::ui {
class PinEntryView : public ViewBase {
public:
    // Use PinManager's constant to avoid duplication
    static constexpr uint32_t LOCKOUT_DURATION_MS = core::PinManager::LOCKOUT_DURATION_MS;
```

### Option 3: Create Shared Constants Header
Create a dedicated constants header for shared values:

```cpp
// components/cdc_core/include/cdc_core/SecurityConstants.h
#pragma once
#include <cstdint>

namespace cdc::security {
constexpr uint32_t LOCKOUT_DURATION_MS = 60000;
constexpr uint8_t MAX_PIN_RETRIES = 3;
constexpr uint8_t BLE_MAX_SCAN_RESULTS = 16;
}
```

## References
- [DRY Principle](https://en.wikipedia.org/wiki/Don%27t_repeat_yourself)
- [Configuration Management Best Practices](https://12factor.net/config)
- Related issues: #001-missing-config-schema, #005-nvs-namespaces-scattered
