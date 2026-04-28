---
title: "[LOW] PASSWORD_ADD command lacks TOTP slot range validation"
severity: LOW
domain: api-design/request-validation
lens: serial-command-validation
labels:
  - "audit:api-design/request-validation"
---

## Summary
The `PASSWORD_ADD` serial command in `components/mod_password/src/PasswordModule.cpp:288` parses the optional TOTP slot parameter but only validates it's within 0-255 range, not whether the slot is actually available or valid for the TOTP module.

**File**: `components/mod_password/src/PasswordModule.cpp`  
**Line**: 288  
**Function**: `cmd_password_add()`

## Impact
- **Invalid references**: Users can specify a TOTP slot that doesn't exist or isn't in the TOTP module's range
- **Silent acceptance**: Invalid slot numbers are accepted but may cause issues when the password is used
- **No cross-validation**: Doesn't verify the TOTP slot belongs to the TOTP module

## Evidence
From `components/mod_password/src/PasswordModule.cpp:285-293`:

```cpp
p = nextToken(p, totpBuf, sizeof(totpBuf));

// ... field copying ...

if (totpBuf[0]) {
    int totp = atoi(totpBuf);
    if (totp >= 0 && totp <= 255) {  // Only basic range check!
        entry.totpSlot = static_cast<uint8_t>(totp);
    }
}
```

From `components/mod_password/include/mod_password/PasswordStore.h:37`:
```cpp
static constexpr uint8_t TOTP_SLOT_NONE = 0xFF;
```

The validation only checks:
- `totp >= 0` (non-negative)
- `totp <= 255` (fits in uint8_t)

But doesn't check:
- Whether the slot is within the TOTP module's configured range
- Whether the slot is actually used by a TOTP account
- Whether the slot is valid (0-255 covers all uint8_t values)

Problematic inputs:
- `PASSWORD_ADD "Site" "user" "pass" "url" "999"` → 999 > 255, silently ignored (totpSlot = 0xFF)
- `PASSWORD_ADD "Site" "user" "pass" "url" "50"` → slot 50 might not be in TOTP range
- `PASSWORD_ADD "Site" "user" "pass" "url" "0"` → slot 0 is system slot (attestation)

## Recommended Fix
Add validation against the actual TOTP slot range:

```cpp
#include "mod_totp/TotpStore.h"  // Access TOTP slot configuration

if (totpBuf[0]) {
    int totp = atoi(totpBuf);
    
    // Basic range check
    if (totp < 0 || totp > 255) {
        cdc::serial::Console::printf("ERROR: TOTP slot must be 0-255\r\n");
        return;
    }
    
    // Get TOTP slot range
    auto& totpStore = cdc::mod_totp::TotpStore::instance();
    uint16_t totpStart = totpStore.rmemStart();
    uint16_t totpEnd = totpStore.rmemEnd();
    
    if (totpStart == 0 || totpEnd == 0) {
        cdc::serial::Console::printf("ERROR: TOTP module not configured\r\n");
        return;
    }
    
    // Check if slot is within TOTP range
    if (totp < totpStart || totp > totpEnd) {
        cdc::serial::Console::printf("ERROR: TOTP slot must be in range %d-%d\r\n", 
                                     totpStart, totpEnd);
        return;
    }
    
    entry.totpSlot = static_cast<uint8_t>(totp);
}
```

Or at minimum, validate against known system constraints:
```cpp
if (totpBuf[0]) {
    int totp = atoi(totpBuf);
    
    // System slot 0 is reserved for attestation
    // TOTP slots are typically 32-131 (based on slot map)
    if (totp == 0) {
        cdc::serial::Console::printf("ERROR: Slot 0 is reserved for system\r\n");
        return;
    }
    if (totp < 0 || totp > 255) {
        cdc::serial::Console::printf("ERROR: TOTP slot must be 0-255\r\n");
        return;
    }
    
    entry.totpSlot = static_cast<uint8_t>(totp);
}
```

## References
- Slot allocation documentation in `main/tropic_slot_map.h`
- TOTP store header: `components/mod_totp/include/mod_totp/TotpStore.h`
