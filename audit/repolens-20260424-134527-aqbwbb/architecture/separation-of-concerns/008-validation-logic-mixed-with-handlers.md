---
title: "[LOW] Validation logic mixed with UI data handlers"
severity: LOW
domain: architecture
lens: separation-of-concerns
labels:
  - "audit:architecture/separation-of-concerns"
---

## Summary
The `WifiHandlers.cpp` file contains validation logic (`isValidIpOctet`, `isValidIpAddress`, `parseIpAddress`) mixed with data handling and persistence logic. While this is in a "handlers" file, the validation methods are pure utility functions that should be separated into a dedicated validation module for reuse across the codebase.

**Location**: `components/cdc_os_ui/src/WifiHandlers.cpp:38-65`

## Impact
- **Code duplication risk**: Similar IP validation logic may be duplicated elsewhere (e.g., in settings, other network modules)
- **Limited reusability**: Validation logic is tied to `WifiHandlers` class, cannot be easily reused for other IP-based configuration
- **Testing complexity**: Validation logic requires instantiating `WifiHandlers` or extracting logic manually
- **Maintenance burden**: If IP validation rules change, multiple files may need updates

## Evidence
```cpp
// WifiHandlers.cpp:38-65 - Validation logic mixed with handlers
namespace cdc::ui {

bool WifiHandlers::isValidIpOctet(int val) {
    return val >= 0 && val <= 255;
}

bool WifiHandlers::isValidIpAddress(const char* ip) {
    if (!ip || !ip[0]) return false;
    int a, b, c, d;
    if (sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) return false;
    return isValidIpOctet(a) && isValidIpOctet(b) && isValidIpOctet(c) && isValidIpOctet(d);
}

uint32_t WifiHandlers::parseIpAddress(const char* ip) const {
    int a, b, c, d;
    if (sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) return 0;
    if (!isValidIpOctet(a) || !isValidIpOctet(b) || !isValidIpOctet(c) || !isValidIpOctet(d)) return 0;
    return (static_cast<uint32_t>(a) << 24) | ...;
}

// These methods are used in WifiHandlers::saveConfig() etc.
}
```

The validation logic is:
1. Tightly coupled to `WifiHandlers` class (methods on class)
2. Not exposed as standalone utilities for reuse
3. Only used for WiFi configuration, but could apply to any IP-based setting

## Recommended Fix
1. **Create a validation utility module**:
   ```cpp
   // components/cdc_core/include/cdc_core/Validation.h
   namespace cdc::core {
       class Validation {
       public:
           static bool isValidIpOctet(int val);
           static bool isValidIpAddress(const char* ip);
           static uint32_t parseIpAddress(const char* ip);
           static bool isValidMacAddress(const char* mac);  // Future reuse
       };
   }
   ```

2. **Refactor `WifiHandlers` to use utilities**:
   ```cpp
   // components/cdc_os_ui/src/WifiHandlers.cpp
   #include "cdc_core/Validation.h"
   
   void WifiHandlers::saveConfig() {
       // Use centralized validation
       if (!core::Validation::isValidIpAddress(ip)) {
           return false;
       }
       // ...
   }
   ```

3. **Benefits of separation**:
   - Validation logic can be reused in other modules (Bluetooth, general settings)
   - Easier to test (pure functions, no class dependencies)
   - Centralized validation rules for consistency

## References
- [Utility class pattern](https://en.wikipedia.org/wiki/Utility_class)
- [DRY principle](https://en.wikipedia.org/wiki/Don%27t_repeat_yourself)
- Related: NVS persistence is also scattered across modules (see issue #006)
