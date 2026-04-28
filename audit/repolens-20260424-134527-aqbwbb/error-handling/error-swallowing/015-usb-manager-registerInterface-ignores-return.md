---
title: "[MEDIUM] USB interface registration failures often ignored in module start()"
severity: MEDIUM
domain: cdc_core
lens: error-handling
labels:
  - "USB"
  - "registerInterface"
  - "start()"
  - "module"
---

## Summary
Multiple module `start()` methods check the return value of `UsbManager::registerInterface()` but only log a warning and continue, masking USB interface registration failures.

## Impact
- Modules report started status even when USB interface is not registered
- Users may expect USB functionality that isn't available
- Debugging becomes difficult as the symptom (no USB) doesn't match the cause (registration failure)

## Evidence
Pattern found in multiple modules:

```cpp
// components/mod_gpg/src/GpgModule.cpp (lines 590-593)
if (!core::UsbManager::instance().registerInterface(core::UsbHidInterface::Ccid, getName(), spec)) {
    LOG_W(TAG, "Failed to register CCID interface");
    // Warning logged but start() still returns true!
}
state_ = core::ServiceState::STARTED;
return true;
```

Similar patterns in:
- `mod_hid/src/HidModule.cpp`
- `mod_ble_serial/src/BleSerialModule.cpp`
- Other modules with USB interfaces

## Recommended Fix
Return `false` from `start()` when critical USB interfaces fail to register:

```cpp
if (!core::UsbManager::instance().registerInterface(core::UsbHidInterface::Ccid, getName(), spec)) {
    LOG_E(TAG, "Failed to register CCID interface");
    state_ = core::ServiceState::ERROR;
    return false;  // Propagate failure
}
state_ = core::ServiceState::STARTED;
return true;
```

For non-critical interfaces, consider:
- A separate "registerInterfaceOptional()" method that returns bool
- Documenting which interfaces are optional vs required
- Module-level flag to track partial startup state

## References
- `UsbManager` interface: `components/cdc_core/include/cdc_core/UsbManager.h`
- Module `start()` contract: `components/cdc_core/include/cdc_core/IModule.h`
- Service state enum: `components/cdc_core/include/cdc_core/ServiceRegistry.h`
