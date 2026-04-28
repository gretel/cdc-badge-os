---
title: "[MEDIUM] BLE Controller Interface Lacks Capability Negotiation"
severity: MEDIUM
domain: API Design
lens: api-versioning
labels:
  - "audit:api-design/api-versioning"
---

## Summary
The `IBluetoothController` interface (`components/cdc_hal/include/cdc_hal/IBluetoothController.h`) has no capability negotiation mechanism. Modules cannot query supported features before attempting to use them.

**Evidence:**
- `components/cdc_hal/include/cdc_hal/IBluetoothController.h` - No `getCapabilities()` or `supportsFeature()` method
- `components/mod_vcard/include/mod_vcard/ble_vcard.h` - Assumes BLE stack supports all features

## Impact
- Modules may fail at runtime if optional features are unavailable
- No compile-time or runtime feature detection
- Hard to support multiple BLE stack implementations with different capabilities

## Evidence
Current interface methods (excerpt):
```cpp
class IBluetoothController : public core::IService {
public:
    virtual bool enable() = 0;
    virtual void disable() = 0;
    virtual bool startScan(uint32_t durationMs = 5000) { (void)durationMs; return false; }
    virtual void stopScan() {}
    // ... many methods with default empty implementations
};
```

Methods return false/void but no way to query support beforehand.

## Recommended Fix
Add capability negotiation:

1. Define capability flags:
```cpp
namespace BleCapability {
    constexpr uint32_t SCANNER      = 1 << 0;
    constexpr uint32_t ADVERTISER   = 1 << 1;
    constexpr uint32_t PERIPHERAL   = 1 << 2;
    constexpr uint32_t CENTRAL      = 1 << 3;
    constexpr uint32_t SECURE_CONN  = 1 << 4;
    constexpr uint32_t LONG_ADV     = 1 << 5;  // >31 bytes
}
```

2. Add capability query:
```cpp
/**
 * Get supported capabilities
 * @return Bitmask of BleCapability flags
 */
virtual uint32_t getCapabilities() const = 0;

/**
 * Check if feature is supported
 * @param capability Capability flag
 * @return true if supported
 */
virtual bool supportsFeature(uint32_t capability) const;
```

3. Update modules to check capabilities before use:
```cpp
if (bleController->supportsFeature(BleCapability::SECURE_CONN)) {
    // Use secure connections
} else {
    // Fallback to legacy pairing
}
```

## References
- BLE 5.0 Features: https://www.bluetooth.com/bluetooth-resources/what-s-new-in-bluetooth-5-0/
- Feature Negotiation Pattern: https://www.oreilly.com/library/view/continuous-delivery-reliable/9780321670255/ch04.html
