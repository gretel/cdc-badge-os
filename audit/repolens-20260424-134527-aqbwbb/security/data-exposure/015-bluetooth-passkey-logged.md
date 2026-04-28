---
title: "[MEDIUM] Bluetooth passkey logged to serial output"
severity: MEDIUM
domain: cdc_hal
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The `BluetoothController::onPasskeyAction` function in `components/cdc_hal/src/BluetoothController.cpp:1252,1262` logs the 6-digit passkey value used for Bluetooth pairing to the serial console. This happens for both "numeric comparison" and "display passkey" pairing methods.

## Impact
- **Passkey Exposure**: The 6-digit passkey (000000-999999) is printed to serial output, visible to anyone with USB-CDC access
- **Bluetooth Pairing Weakening**: An attacker with serial access can capture the passkey and complete the Bluetooth pairing process
- **Debug Logging Persistence**: Serial output is often logged to files, terminals, or captured by IDEs, persisting the passkey
- **Numeric Comparison Leakage**: Even for numeric comparison (where user verifies), the passkey is logged, potentially revealing it to an attacker monitoring serial

## Evidence
File: `components/cdc_hal/src/BluetoothController.cpp:1250-1266`

```cpp
void BluetoothController::onPasskeyAction(uint16_t connHandle,
                                          const ble_gap_passkey_params* params) {
    if (!params) return;

    switch (params->action) {
        case BLE_SM_IOACT_NUMCMP:
            LOG_I(TAG, "Numeric comparison: %06lu", (unsigned long)params->numcmp);  // PASSKEY LOGGED
            if (numericCompCb_) {
                numericCompCb_(connHandle, params->numcmp);
            }
            break;

        case BLE_SM_IOACT_DISP:
            LOG_I(TAG, "Display passkey: %06lu", (unsigned long)params->numcmp);  // PASSKEY LOGGED
            if (passkeyCb_) {
                passkeyCb_(params->numcmp);
            }
            break;
        // ...
    }
}
```

The passkey is logged at INFO level, which is always enabled (even in production builds).

## Recommended Fix
1. **Remove the passkey value from log messages** - log only the action type
2. **Use a generic message** for passkey operations
3. **Add DEBUG-only logging** if detailed logging is needed for development

Example fix:
```cpp
case BLE_SM_IOACT_NUMCMP:
    LOG_I(TAG, "Numeric comparison pairing started");
    if (numericCompCb_) {
        numericCompCb_(connHandle, params->numcmp);
    }
    break;

case BLE_SM_IOACT_DISP:
    LOG_I(TAG, "Passkey display pairing started");
    if (passkeyCb_) {
        passkeyCb_(params->numcmp);
    }
    break;
```

If debugging is needed, log only at DEBUG level with masked value:
```cpp
#if DEBUG_MODE
    LOG_D(TAG, "Passkey: %03d... (full value for debugging)", params->numcmp / 1000);
#endif
```

## References
- Bluetooth Core Specification: Secure Simple Pairing
- OWASP: [Session Management - Passkeys](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html)
- NIST SP 800-63B: Digital Identity Guidelines - Authentication

</content>