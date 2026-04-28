---
title: "[LOW] Inconsistent state validation in module start() methods"
severity: LOW
domain: architecture
lens: pattern-consistency
labels:
  - "audit:code-quality/pattern-consistency"
---

## Summary

Most modules follow a consistent pattern for state validation in their `start()` methods, but some modules deviate by using **different validation logic** or **additional checks**:

1. **Standard pattern**: Check state is `INITIALIZED` or `STOPPED`, then set to `STARTED`
2. **Extended pattern**: Standard check plus additional hardware/resource validation

**Files affected:**
- `components/mod_totp/src/TotpModule.cpp` - Standard pattern
- `components/mod_gpg/src/GpgModule.cpp` - Standard pattern with USB registration
- `components/mod_hid/src/HidModule.cpp` - Standard pattern
- `components/mod_ble_serial/src/BleSerialModule.cpp` - Standard pattern with conditional start
- `components/grove_led/src/GroveLedModule.cpp` - Extended pattern (checks LED strip)
- `components/mod_fido2/src/Fido2Module.cpp` - Standard pattern with USB registration

### Standard pattern (most modules)

```cpp
// In mod_totp/src/TotpModule.cpp
bool TotpModule::start() {
    if (state_ != core::ServiceState::INITIALIZED &&
        state_ != core::ServiceState::STOPPED) {
        return false;
    }
    state_ = core::ServiceState::STARTED;
    return true;
}
```

### Extended pattern (Grove LED)

```cpp
// In grove_led/src/GroveLedModule.cpp
bool GroveLedModule::start() {
    if (!strip_) {  // Additional hardware check
        LOG_E(TAG, "Cannot start: LED strip not initialized");
        return false;
    }

    LOG_I(TAG, "Starting Grove LED module (count=%d)", ledCount_);
    state_ = core::ServiceState::STARTED;
    return true;
}
```

### Conditional pattern (BLE Serial)

```cpp
// In mod_ble_serial/src/BleSerialModule.cpp
bool BleSerialModule::start() {
    if (state_ != core::ServiceState::INITIALIZED &&
        state_ != core::ServiceState::STOPPED) {
        return false;
    }

    // If auto-enable is set and BLE is enabled, start the service
    if (enabled_) {
        auto* ble = hal::getBluetoothControllerInstance();
        if (ble && ble->isEnabled()) {
            auto& uart = BleUartService::instance();
            if (uart.init()) {
                registerConsoleHooks();
                registerPairingCallback();
                LOG_I(TAG, "BLE Serial service started");
            }
        }
    }

    state_ = core::ServiceState::STARTED;
    return true;
}
```

## Impact

1. **Confusion for developers**: Different modules validate differently, making it hard to know what to expect
2. **Inconsistent error handling**: Some log errors (`GroveLedModule`), others silently fail
3. **Unclear contract**: Is `start()` just a state transition, or does it also initialize resources?

## Evidence

**Standard pattern implementations:**
- `components/mod_totp/src/TotpModule.cpp:951` - Simple state check, no additional validation
- `components/mod_hid/src/HidModule.cpp:308` - Simple state check, no additional validation
- `components/mod_gpg/src/GpgModule.cpp:563` - State check + USB registration (but no early return)

**Extended pattern implementations:**
- `components/grove_led/src/GroveLedModule.cpp:148` - State check + hardware validation + error logging
- `components/mod_ble_serial/src/BleSerialModule.cpp:215` - State check + conditional service start

## Recommended Fix

### Option 1: Standardize on minimal state check

Remove additional validation from `start()` and move it to `init()`:

```cpp
// Before (GroveLedModule):
bool GroveLedModule::start() {
    if (!strip_) {
        LOG_E(TAG, "Cannot start: LED strip not initialized");
        return false;
    }
    state_ = core::ServiceState::STARTED;
    return true;
}

// After:
bool GroveLedModule::start() {
    if (state_ != core::ServiceState::INITIALIZED &&
        state_ != core::ServiceState::STOPPED) {
        return false;
    }
    state_ = core::ServiceState::STARTED;
    return true;
}

// Move hardware check to init():
bool GroveLedModule::init() {
    // ... LED strip creation ...
    if (err != ESP_OK) {
        LOG_E(TAG, "Failed to create LED strip");
        return false;  // Fail init, not start
    }
    // ...
}
```

### Option 2: Document the two patterns

Explicitly document when to use additional validation:

```cpp
/**
 * \brief Start pattern options:
 * 
 * 1. Simple state transition (most modules):
 *    - Just validate state and transition
 *    - Resources should be ready from init()
 *    - Example: TotpModule, HidModule
 * 
 * 2. Resource validation (modules with optional hardware):
 *    - Check hardware/resources are ready
 *    - Log errors for debugging
 *    - Example: GroveLedModule
 */
```

### Option 3: Add consistent error logging

Make all modules with additional checks log errors consistently:

```cpp
bool Module::start() {
    if (state_ != core::ServiceState::INITIALIZED &&
        state_ != core::ServiceState::STOPPED) {
        LOG_W(TAG, "Invalid state for start: %d", state_);  // Consistent logging
        return false;
    }
    // ...
}
```

## References

- `components/cdc_core/include/cdc_core/IService.h` - Service state machine definition
- `components/mod_totp/src/TotpModule.cpp` - Reference implementation of simple pattern
- `components/grove_led/src/GroveLedModule.cpp` - Example of extended pattern

</content>