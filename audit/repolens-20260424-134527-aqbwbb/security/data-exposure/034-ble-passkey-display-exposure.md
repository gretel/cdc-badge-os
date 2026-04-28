---
title: "[MEDIUM] BLE passkey displayed on E-Paper display without user confirmation"
severity: MEDIUM
domain: ble-serial
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The BLE Serial module displays the 6-digit passkey on the E-Paper display during numeric comparison pairing. While this is necessary for user verification, the passkey is also logged to the serial console via `snprintf`, potentially exposing it to anyone with serial access.

**Location:** `components/mod_ble_serial/src/BleSerialModule.cpp:124-132`

## Impact
- **Passkey exposure**: The 6-digit passkey used for BLE pairing is formatted and displayed
- **Serial console leakage**: The `snprintf` call formats the passkey which then appears on the display and potentially in logs
- **Physical access attack**: Anyone with physical access can read the passkey from the display
- **Timing attack**: The passkey is shown for a duration that could be logged

## Evidence
File: `components/mod_ble_serial/src/BleSerialModule.cpp:124-132`
```cpp
ble->setNumericComparisonCallback([](uint16_t connHandle, uint32_t passkey) {
    char msg[32];
    snprintf(msg, sizeof(msg), "BLE Pairing?\n%06lu", (unsigned long)passkey);
    cdc::ui::showPrompt("BLE Pairing", msg, 
        {{"YES", cdc::ui::PromptResult::APPROVE}, {"NO", cdc::ui::PromptResult::DENY}},
        [connHandle]() { ble->confirmPasskey(connHandle, true); },
        [connHandle]() { ble->confirmPasskey(connHandle, false); }
    );
});
```

The passkey is a 6-digit number (000000-999999) used for BLE numeric comparison pairing. It's displayed on the E-Paper screen for the user to verify.

## Recommended Fix
1. Add `DEBUG_MODE` check before logging passkey details
2. Consider masking the passkey in logs (show only last 2 digits)
3. Document that passkey is visible on display for physical verification only

Example fix:
```cpp
ble->setNumericComparisonCallback([](uint16_t connHandle, uint32_t passkey) {
    char msg[32];
    snprintf(msg, sizeof(msg), "BLE Pairing?\n%06lu", (unsigned long)passkey);
    #ifdef DEBUG_MODE
    LOG_D("BLE", "Passkey: %06lu", (unsigned long)passkey);
    #endif
    cdc::ui::showPrompt("BLE Pairing", msg, ...);
});
```

## References
- BLE Numeric Comparison: https://www.bluetooth.com/specifications/specs/security-specification-core-v5-3/
- Related to issue #15 (Bluetooth passkey logging)
