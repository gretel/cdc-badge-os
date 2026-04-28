---
title: "[HIGH] GPG module start() reports success even when openpgp_init() fails"
severity: HIGH
domain: mod_gpg
lens: error-handling
labels:
  - "GPG"
  - "OpenPGP"
  - "start()"
  - "openpgp_init"
---

## Summary
In `components/mod_gpg/src/GpgModule.cpp` (lines 587-595), the `start()` method logs an error when `openpgp_init()` fails but continues to set state to STARTED and returns `true`, masking the initialization failure from callers.

## Impact
The GPG module reports successful startup even when the core OpenPGP library failed to initialize. This leads to:
- Silent failure where the module appears functional but key operations fail
- Callers assuming the module is ready when it is not
- Difficult debugging as the root cause is hidden
- Potential crashes when methods assume initialized state

## Evidence
```cpp
// Lines 584-595 in GpgModule.cpp
bool GpgModule::start() {
    // ...
    spec.epInSize = 64;
    spec.epOutSize = 64;
    if (!openpgp_init()) {
        core::ModuleRegistry::instance().reportModuleError(getName(), "OpenPGP init failed");
        // Error reported but no return false!
    }
    if (!core::UsbManager::instance().registerInterface(core::UsbHidInterface::Ccid, getName(), spec)) {
        LOG_W(TAG, "Failed to register CCID interface");
        // Warning logged but no return false!
    }

    state_ = core::ServiceState::STARTED;  // State set regardless of errors!
    return true;  // Returns success even if openpgp_init() failed!
}
```

Compare to `init()` method (lines 550-569) which correctly returns `false` on error:
```cpp
if (!gpg_storage_ready()) {
    core::ModuleRegistry::instance().reportModuleError(getName(), "GPG slot range invalid");
    state_ = core::ServiceState::ERROR;
    return false;  // Correctly propagates failure
}
```

## Recommended Fix
Return `false` when `openpgp_init()` fails and set state to ERROR:

```cpp
if (!openpgp_init()) {
    core::ModuleRegistry::instance().reportModuleError(getName(), "OpenPGP init failed");
    state_ = core::ServiceState::ERROR;
    return false;
}
if (!core::UsbManager::instance().registerInterface(core::UsbHidInterface::Ccid, getName(), spec)) {
    LOG_W(TAG, "Failed to register CCID interface");
    // Note: This may be non-fatal, but consider if it should also fail start
}
```

## References
- Module interface definition: `components/cdc_core/include/cdc_core/IModule.h`
- Service state enum: `components/cdc_core/include/cdc_core/ServiceRegistry.h`
- Similar pattern correctly implemented in `GpgModule::init()` (lines 550-569)
