---
title: "[LOW] TR01_SLOTS command reveals ECC slot usage without authentication"
severity: LOW
domain: cdc-badge-os
lens: library/cdc-badge-os
labels:
  - "audit:toolgate/session-nuclei"
---

## Summary
The `TR01_SLOTS` serial command reveals which ECC key slots are populated without requiring authentication. When `FEATURE_SECURE_SERIAL` is disabled (default), an attacker with serial access can enumerate used ECC slots, revealing information about stored cryptographic keys (FIDO2 credentials, GPG keys, CA keys).

**Location**: `components/serial_cmd/src/SerialCmd.cpp:1474` (registration), `components/serial_cmd/src/SerialCmd.cpp:964-989` (handler)

## Impact
- **Information Disclosure**: Reveals which ECC slots (1-31) are populated
- **Key Enumeration**: Attacker can determine:
  - GPG key slots (1-3): how many GPG keys stored
  - CA key slot (4): if CA key exists
  - FIDO2 credential slots (5-31): approximate number of credentials
- **Reconnaissance**: Helps attacker map the device's cryptographic state
- **Serial Access Required**: Physical or USB CDC connection at 115200 baud

## Evidence
```cpp
// Command registration at line 1474
reg.registerCommand({"TR01_SLOTS", "Show TR01 slot usage", cmdTr01Slots, "tr01", false});
// The last 'false' means requiresAuth = false

// Handler at lines 964-989
static void cmdTr01Slots(const char* args) {
    (void)args;
    auto* se = getSecureElementWithCheck();
    if (!se) return;

    Console::printf("ECC Key Slots (0-31):\r\n");
    int eccCount = 0;
    for (uint8_t i = 0; i < hal::ISecureElement::ECC_SLOT_COUNT; i++) {
        if (se->eccSlotUsed(i)) {
            Console::printf("  [%02d] Used\r\n", i);  // <-- Exposes slot usage!
            eccCount++;
        }
    }
    if (eccCount == 0) {
        Console::printf("  (none)\r\n");
    }

    Console::printf("\r\nR-Memory Slots summary:\r\n");
    Console::printf("  Slot 0:        System PIN/lockout\r\n");
    Console::printf("  Slots 1-31:    ECC paired (module-owned)\r\n");
    Console::printf("  Slots 32-131:  TOTP accounts\r\n");
    Console::printf("  Slots 132-511: Password vault\r\n");
}
```

Example output revealing key structure:
```
TR01_SLOTS
ECC Key Slots (0-31):
  [01] Used
  [02] Used
  [04] Used
  [05] Used
  [07] Used
  [09] Used
  (6 more used slots)

R-Memory Slots summary:
  Slot 0:        System PIN/lockout
  Slots 1-31:    ECC paired (module-owned)
  Slots 32-131:  TOTP accounts
  Slots 132-511: Password vault
```

## Recommended Fix
Add authentication requirement to the `TR01_SLOTS` command:

```cpp
// Change registration from:
reg.registerCommand({"TR01_SLOTS", "Show TR01 slot usage", cmdTr01Slots, "tr01", false});

// To:
reg.registerCommand({"TR01_SLOTS", "Show TR01 slot usage", cmdTr01Slots, "tr01", true});
```

## References
- `components/serial_cmd/src/SerialCmd.cpp:1474` - Command registration
- `components/serial_cmd/src/SerialCmd.cpp:964-989` - Command handler
- `components/cdc_hal/src/Tropic01Element.cpp:320` - eccSlotUsed implementation
- CWE-200: Exposure of sensitive information to an unauthorized actor
- CWE-201: Introduction of sensitive information into an information stream
