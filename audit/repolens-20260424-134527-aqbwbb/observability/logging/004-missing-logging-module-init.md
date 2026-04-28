---
title: "[MEDIUM] Missing logging in FIDO2 module lifecycle events"
severity: MEDIUM
domain: logging
lens: logging-coverage
labels:
  - "audit:observability/logging"
---

## Summary
The FIDO2 module (`components/mod_fido2/src/Fido2Module.cpp`) has several lifecycle events and error paths that lack logging, making it difficult to debug module initialization and operation.

**Missing log entries:**

1. **FIDO2 init failure** (lines 185-188): Logs error to registry but not via LOG_E:
```cpp
if (!fido2_is_initialized()) {
    if (!fido2_init()) {
        core::ModuleRegistry::instance().reportModuleError(getName(), "FIDO2 init failed");
        core::UsbManager::instance().unregisterInterface(core::UsbHidInterface::Fido, getName());
        state_ = core::ServiceState::ERROR;
        return false;  // No LOG_E with details
    }
}
```

2. **Interface registration failure** (lines 174-175): Only logs warning:
```cpp
if (!core::UsbManager::instance().registerInterface(core::UsbHidInterface::Fido, getName(), spec)) {
    LOG_W(TAG, "Failed to register FIDO HID interface");
    return false;  // No additional context
}
```

3. **Stop function** (lines 196-199): No log entry for stop event:
```cpp
void Fido2Module::stop() {
    core::UsbManager::instance().unregisterInterface(core::UsbHidInterface::Fido, getName());
    state_ = core::ServiceState::STOPPED;  // No LOG_I for stop event
}
```

4. **RX queue full** (line 89): Warning exists but lacks context:
```cpp
if (xQueueSend(s_rx_queue, &pkt, 0) != pdTRUE) {
    LOG_W(TAG, "RX queue full, dropping packet");  // No count or frequency
}
```

## Impact
- **Debug difficulty**: When FIDO2 module fails to initialize, it's hard to determine where
- **Operational visibility**: No log trail for FIDO2 module state transitions
- **Packet loss tracking**: RX queue drops aren't logged with enough context for debugging

## Evidence
Compare with TOTP module which has better logging:
```cpp
// TotpModule::init() - good logging
LOG_I(TAG, "Initializing TOTP module");
registerStrings();
registerCommands();
...
```

FIDO2 module has:
```cpp
// Fido2Module::init() - less logging
LOG_I(TAG, "Initializing FIDO2 module");
// ... but missing detailed error logging
```

## Recommended Fix
Add logging to FIDO2 module lifecycle:

1. **In `start()` function (around line 174)**:
```cpp
if (!core::UsbManager::instance().registerInterface(core::UsbHidInterface::Fido, getName(), spec)) {
    LOG_E(TAG, "Failed to register FIDO HID interface");
    return false;
}
```

2. **In `start()` function (around line 185)**:
```cpp
if (!fido2_is_initialized()) {
    if (!fido2_init()) {
        LOG_E(TAG, "FIDO2 core initialization failed");
        core::ModuleRegistry::instance().reportModuleError(getName(), "FIDO2 init failed");
        ...
    }
}
LOG_I(TAG, "FIDO2 core stack initialized");
```

3. **In `stop()` function**:
```cpp
void Fido2Module::stop() {
    LOG_I(TAG, "Stopping FIDO2 module");
    core::UsbManager::instance().unregisterInterface(core::UsbHidInterface::Fido, getName());
    state_ = core::ServiceState::STOPPED;
}
```

4. **Enhance RX queue warning (line 89)**:
```cpp
if (xQueueSend(s_rx_queue, &pkt, 0) != pdTRUE) {
    static uint32_t dropCount = 0;
    dropCount++;
    LOG_W(TAG, "RX queue full, dropping packet (total drops: %lu)", dropCount);
}
```

## References
- `components/mod_fido2/src/Fido2Module.cpp` - FIDO2 module implementation
- `components/mod_totp/src/TotpModule.cpp` - Reference for better logging patterns
