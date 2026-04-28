---
title: "[LOW] Modules directly access NVS instead of using abstraction"
severity: LOW
domain: architecture/dependency-direction
lens: dependency-direction
labels:
  - "audit:architecture/dependency-direction"
---

## Summary

Multiple modules directly access `nvs_flash` for persistent storage instead of using a common abstraction layer. This creates tight coupling to the storage implementation.

**Affected files:**
- `components/grove_led/src/GroveLedModule.cpp:9-10` - `#include <nvs_flash.h>`, `#include <nvs.h>`
- `components/mod_totp/src/TotpModule.cpp` - Uses `TotpStore` which likely uses NVS directly
- `components/mod_gpg/src/GpgStorage.cpp` - Direct NVS access
- `components/mod_password/src/PasswordModule.cpp` - Direct NVS access

**Evidence:**
```cpp
// components/grove_led/src/GroveLedModule.cpp
#include <nvs_flash.h>
#include <nvs.h>

void GroveLedModule::loadSettings() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        // Direct NVS API usage
        nvs_get_u8(handle, NVS_KEY_ENABLED, &val);
        ...
    }
}
```

## Impact

1. **Storage coupling**: Modules are tied to ESP-IDF's NVS implementation
2. **Code duplication**: Each module implements its own NVS handling
3. **Migration difficulty**: Hard to switch to different storage backend

## Evidence

**In `components/grove_led/src/GroveLedModule.cpp:567-628`:**
```cpp
void GroveLedModule::loadSettings() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        uint8_t val = 0;
        if (nvs_get_u8(handle, NVS_KEY_ENABLED, &val) == ESP_OK) {
            enabled_ = (val != 0);
        }
        // ... more direct NVS calls
        nvs_close(handle);
    }
}

void GroveLedModule::saveSettings() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, NVS_KEY_ENABLED, enabled_ ? 1 : 0);
        // ... more direct NVS calls
        nvs_commit(handle);
        nvs_close(handle);
    }
}
```

## Recommended Fix

1. Create a `ISettingsStore` interface in `cdc_core` or `cdc_hal`
2. Implement `NvsSettingsStore` that wraps NVS API
3. Modules inject settings store dependency

**Scope estimate:** 1 hour (for one module refactoring)

## References

- Dependency Inversion: High-level modules (settings) should not depend on low-level modules (NVS)
- Current architecture: `cdc_core` already has `TropicStorage` abstraction
