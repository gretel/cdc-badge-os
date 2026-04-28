---
title: "[MEDIUM] TR01_RMEM_READ command exposes secure element memory without authentication"
severity: MEDIUM
domain: Authorization & Access Control
lens: authorization
labels:
  - audit:security/authorization
---

## Summary
The `TR01_RMEM_READ` command allows reading any R-Memory slot from the TROPIC01 secure element without authentication. This can expose sensitive data including PIN hashes, TOTP secrets, and password metadata stored in R-Memory slots.

**Location:** `components/serial_cmd/src/SerialCmd.cpp:1475`

## Impact
- **PIN hash exposure:** R-Memory slot 0 contains PIN hashes and retry counters
- **TOTP secret enumeration:** TOTP account data (names, metadata) stored in R-Memory slots 32-131
- **Password metadata leakage:** Password entry metadata stored in R-Memory slots 150-511
- **Slot structure discovery:** Attacker can enumerate all used slots and understand data layout

## Evidence
Command registration at `components/serial_cmd/src/SerialCmd.cpp:1475`:
```cpp
reg.registerCommand({"TR01_RMEM_READ", "Read R-Memory slot", cmdTr01RmemRead, "tr01", false});
```

The command is registered with `requiresAuth = false`, allowing unauthenticated access.

Command handler at `components/serial_cmd/src/SerialCmd.cpp:986-1012`:
```cpp
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
    printHexDump(data, actualLen, actualLen);  // Prints full slot contents!
}
```

R-Memory slot layout (from documentation in `PinManager.h`):
```
Slot 0:   System PIN/lockout (PIN hashes, retry counters, lockout timestamp)
Slots 1-31: ECC paired (module-owned metadata)
Slots 32-131: TOTP accounts (account names, secrets, configuration)
Slots 132-511: Password vault (entry metadata, titles, usernames)
```

PIN storage format at `components/cdc_core/include/cdc_core/PinManager.h:8-28`:
```cpp
/**
 * PIN Manager - Manages all device PINs in TROPIC01 R-Memory Slot 0
 *
 * Storage Format (106 bytes):
 * [Magic 0xDD]           (1)  - Format identifier
 * [Badge/FIDO2 Hash]     (16) - LEFT(SHA256(PIN), 16)
 * [Badge Retries]        (1)  - Remaining attempts for Badge PIN
 * [KDF Algorithm]        (1)  - 0x03 = KDF_ITERSALTED_S2K
 * [Hash Algorithm]      (1)  - 0x08 = SHA256
 * [Iteration Count]      (4)  - Default 100000
 * [PW1 Salt]             (8)  - Random salt for User PIN
 * [PW3 Salt]             (8)  - Random salt for Admin PIN
 * [PW1 Hash]            (32) - KDF hash of User PIN
 * [PW3 Hash]            (32) - KDF hash of Admin PIN
 * [PW1 Retries]          (1)  - Remaining attempts
 * [PW3 Retries]          (1)  - Remaining attempts
 */
```

An attacker can read slot 0 and obtain:
- Badge PIN hash (16 bytes)
- PW1/PW3 salts and hashes
- Iteration counts for KDF

## Recommended Fix
Require authentication for R-Memory read access. Update command registration at `components/serial_cmd/src/SerialCmd.cpp:1475`:

```cpp
// Change from:
reg.registerCommand({"TR01_RMEM_READ", "Read R-Memory slot", cmdTr01RmemRead, "tr01", false});

// To:
reg.registerCommand({"TR01_RMEM_READ", "Read R-Memory slot", cmdTr01RmemRead, "tr01", true});
```

Additionally, consider adding slot-level access control:
1. **Slot 0 (PIN data):** Require Admin PIN (PW3) verification
2. **Module-owned slots:** Verify module-specific authentication
3. **TOTP slots (32-131):** Require TOTP module authentication
4. **Password slots (132-511):** Require Password module authentication

Example enhanced handler:
```cpp
static void cmdTr01RmemRead(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::RMEM_SLOT_COUNT, "R-Memory slot");
    if (!result.valid) {
        Console::printf("Usage: TR01_RMEM_READ <slot>\r\n");
        return;
    }

    uint16_t slot = static_cast<uint16_t>(result.value);
    
    // Slot-specific access control
    auto& pm = core::PinManager::instance();
    if (slot == 0) {
        // PIN data requires Admin PIN
        if (!pm.verifyPW3(get_admin_pin_from_env())) {
            Console::printf("ERROR: Admin PIN required for slot 0\r\n");
            return;
        }
    } else if (slot >= 32 && slot <= 131) {
        // TOTP slots require TOTP authentication
        // ...
    }
    
    // ... rest of read logic ...
}
```

## References
- NIST SP 800-73-4 (PIV Interface): https://csrc.nist.gov/publications/detail/sp/800-73/4/final
- FIDO2 CTAP2 specification: https://fidoalliance.org/specs/fido2/
- OWASP Data Protection Cheat Sheet: https://cheatsheetseries.owasp.org/cheatsheets/Data_Protection_Cheat_Sheet.html
