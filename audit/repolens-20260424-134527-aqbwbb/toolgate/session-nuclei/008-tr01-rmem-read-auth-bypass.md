---
title: "[HIGH] TR01_RMEM_READ allows reading PIN data without authentication"
severity: HIGH
domain: cdc-badge-os
lens: library/cdc-badge-os
labels:
  - "audit:toolgate/session-nuclei"
---

## Summary
The `TR01_RMEM_READ` serial command at `components/serial_cmd/src/SerialCmd.cpp:1475` allows reading any TROPIC01 R-Memory slot without authentication. When `FEATURE_SECURE_SERIAL` is disabled (default), an attacker can read slot 0 which contains the PIN hashes and retry counts, enabling offline PIN cracking.

**Location**: `components/serial_cmd/src/SerialCmd.cpp:992-1014` (handler), `components/serial_cmd/src/SerialCmd.cpp:1475` (registration)

## Impact
- **PIN Hash Exposure**: R-Memory slot 0 contains:
  - Badge/FIDO2 PIN hash (16 bytes)
  - OpenPGP PW1 hash (32 bytes) with salt
  - OpenPGP PW3 hash (32 bytes) with salt
- **Offline Brute Force**: With the hash and salt, an attacker can crack PINs offline without lockout penalties
- **All R-Memory Access**: Also exposes TOTP secrets and password vault data in other slots
- **Serial Access Required**: Physical or USB CDC connection at 115200 baud

## Evidence
```cpp
// Command registration at line 1475
reg.registerCommand({"TR01_RMEM_READ", "Read R-Memory slot", cmdTr01RmemRead, "tr01", false});
// The last 'false' means requiresAuth = false

// Handler at lines 992-1014
static void cmdTr01RmemRead(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::RMEM_SLOT_COUNT, "R-Memory slot");
    if (!result.valid) {
        Console::printf("Usage: TR01_RMEM_READ <slot>\r\n");
        return;
    }

    auto* se = getSecureElementWithCheck();
    if (!se) return;

    uint16_t slot = static_cast<uint16_t>(result.value);
    uint8_t data[256];
    uint16_t actualLen = 0;

    hal::SeResult seResult = se->rmemRead(slot, data, sizeof(data), &actualLen);
    if (seResult != hal::SeResult::OK) {
        Console::printf("ERROR: Read failed (slot may be empty)\r\n");
        return;
    }

    Console::printf("R-Memory Slot %d (%d bytes):\r\n", slot, actualLen);
    printHexDump(data, actualLen, actualLen);  // Prints raw data including PIN hashes
}
```

R-Memory slot 0 layout (from `PinManager.h`):
```cpp
// Storage Format (106 bytes):
// [Magic 0xDD]           (1)  - Format identifier
// [Badge/FIDO2 Hash]     (16) - LEFT(SHA256(PIN), 16)
// [Badge Retries]        (1)  - Remaining attempts
// [KDF Algorithm]        (1)  - 0x03 = KDF_ITERSALTED_S2K
// [Hash Algorithm]       (1)  - 0x08 = SHA256
// [Iteration Count]      (4)  - Default 100000
// [PW1 Salt]             (8)  - Random salt for User PIN
// [PW3 Salt]             (8)  - Random salt for Admin PIN
// [PW1 Hash]             (32) - KDF hash of User PIN
// [PW3 Hash]             (32) - KDF hash of Admin PIN
// [PW1 Retries]          (1)  - Remaining attempts
// [PW3 Retries]          (1)  - Remaining attempts
```

## Recommended Fix
Add authentication requirement to the `TR01_RMEM_READ` command:

```cpp
// Change registration from:
reg.registerCommand({"TR01_RMEM_READ", "Read R-Memory slot", cmdTr01RmemRead, "tr01", false});

// To:
reg.registerCommand({"TR01_RMEM_READ", "Read R-Memory slot", cmdTr01RmemRead, "tr01", true});
```

Alternatively, add an explicit check in the handler:
```cpp
static void cmdTr01RmemRead(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::RMEM_SLOT_COUNT, "R-Memory slot");
    if (!result.valid) {
        Console::printf("Usage: TR01_RMEM_READ <slot>\r\n");
        return;
    }

#if FEATURE_SECURE_SERIAL
    if (!SerialCmd::isAuthenticated()) {
        Console::printf("ERROR: Authentication required. Use AUTH <pin> first.\r\n");
        return;
    }
#endif

    auto* se = getSecureElementWithCheck();
    if (!se) return;
    // ... rest of function
}
```

## References
- `components/serial_cmd/src/SerialCmd.cpp:1475` - Command registration
- `components/serial_cmd/src/SerialCmd.cpp:992-1014` - Command handler
- `components/cdc_core/include/cdc_core/PinManager.h:8-28` - R-Memory slot 0 layout
- CWE-200: Exposure of sensitive information to an unauthorized actor
