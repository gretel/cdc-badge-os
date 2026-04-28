---
title: "[LOW] Missing logging for FIDO2 module stop and error paths"
severity: LOW
domain: logging
lens: logging-coverage
labels:
  - "audit:observability/logging"
---

## Summary
The FIDO2 module (`components/mod_fido2/src/Fido2Module.cpp`) has missing logging for lifecycle events and error paths, making it difficult to trace module state transitions during debugging.

**Missing log entries:**

1. **`stop()` function** (lines 201-204): No log entry for stop event
```cpp
void Fido2Module::stop() {
    core::UsbManager::instance().unregisterInterface(core::UsbHidInterface::Fido, getName());
    state_ = core::ServiceState::STOPPED;  // No LOG_I for stop event
}
```

2. **`init()` error paths** (lines 135-147): Errors logged via ModuleRegistry but not via LOG_E
```cpp
if (rmemCount < eccCount) {
    core::ModuleRegistry::instance().reportModuleError(
        getName(), "FIDO2 R-MEM range smaller than ECC range");
    state_ = core::ServiceState::ERROR;
    return false;  // No LOG_E with details
}
```

3. **`start()` interface registration failure** (lines 176-179): Only logs warning, not error
```cpp
if (!core::UsbManager::instance().registerInterface(core::UsbHidInterface::Fido, getName(), spec)) {
    LOG_W(TAG, "Failed to register FIDO HID interface");  // Should be LOG_E
    return false;
}
```

4. **`start()` FIDO2 init failure** (lines 184-191): Error logged via ModuleRegistry but not LOG_E
```cpp
if (!fido2_init()) {
    core::ModuleRegistry::instance().reportModuleError(getName(), "FIDO2 init failed");
    core::UsbManager::instance().unregisterInterface(core::UsbHidInterface::Fido, getName());
    state_ = core::ServiceState::ERROR;
    return false;  // No LOG_E with details
}
```

## Impact
- **Debug difficulty**: When FIDO2 module fails to start/stop, it's hard to determine where
- **Operational visibility**: No log trail for FIDO2 module state transitions
- **Inconsistent error logging**: Some errors use LOG_E, others only call reportModuleError()

## Evidence
Compare with TOTP module which has better logging:
```cpp
// TotpModule::stop() - good logging
void TotpModule::stop() {
    LOG_I(TAG, "Stopping TOTP module");
    // ... cleanup
}
```

FIDO2 module has:
```cpp
// Fido2Module::stop() - no logging
void Fido2Module::stop() {
    core::UsbManager::instance().unregisterInterface(...);
    state_ = core::ServiceState::STOPPED;
}
```

## Recommended Fix
Add logging to FIDO2 module lifecycle:

1. **In `stop()` function** (line 201):
```cpp
void Fido2Module::stop() {
    LOG_I(TAG, "Stopping FIDO2 module");
    core::UsbManager::instance().unregisterInterface(core::UsbHidInterface::Fido, getName());
    state_ = core::ServiceState::STOPPED;
}
```

2. **In `init()` error paths** (lines 135-147):
```cpp
if (rmemCount < eccCount) {
    LOG_E(TAG, "FIDO2 R-MEM range (%u) smaller than ECC range (%u)", rmemCount, eccCount);
    core::ModuleRegistry::instance().reportModuleError(
        getName(), "FIDO2 R-MEM range smaller than ECC range");
    ...
}
```

3. **In `start()` interface registration** (line 176):
```cpp
if (!core::UsbManager::instance().registerInterface(core::UsbHidInterface::Fido, getName(), spec)) {
    LOG_E(TAG, "Failed to register FIDO HID interface");
    return false;
}
```

4. **In `start()` FIDO2 init** (lines 184-191):
```cpp
if (!fido2_init()) {
    LOG_E(TAG, "FIDO2 core initialization failed");
    core::ModuleRegistry::instance().reportModuleError(getName(), "FIDO2 init failed");
    ...
}
LOG_I(TAG, "FIDO2 core stack initialized");
```

## References
- `components/mod_fido2/src/Fido2Module.cpp` - FIDO2 module implementation
- `components/mod_totp/src/TotpModule.cpp` - Reference for better logging patterns
