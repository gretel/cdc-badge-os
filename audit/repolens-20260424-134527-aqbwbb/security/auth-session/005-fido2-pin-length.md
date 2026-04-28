---
title: "[LOW] Missing PIN length validation in FIDO2 ClientPIN"
severity: LOW
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The FIDO2 ClientPIN protocol accepts PINs without validating minimum length. The badge PIN has a minimum of 4 digits defined in `PinManager`, but the FIDO2 layer does not enforce this during `setPin` or `changePin` operations:

**File**: `components/mod_fido2/src/ctap2.cpp:2323-2620`
The `setPin` and `changePin` handlers accept any PIN length:

```cpp
// Around line 2400-2500
// PIN is read from encrypted pinHashEnc, but no length check
uint8_t decrypted_pin_hash[16];
// ... decryption code ...

// Verify PIN hash
if (!pin_storage_verify_fido2_hash(decrypted_pin_hash)) {
    g_client_pin.pin_retries--;
    ...
}
```

The `pin_storage_verify_fido2_hash()` function validates the hash but does not check if the original PIN was short enough to be easily brute-forced.

## Impact
- **Weak PINs**: Users could set a 1-digit PIN (e.g., "1") which is trivially brute-forceable
- **FIDO2 Compliance**: FIDO2 specification recommends minimum 6-8 character PINs
- **Reduced Security**: Shorter PINs have fewer combinations, making brute-force attacks faster

## Evidence
**File**: `components/mod_fido2/src/ctap2.cpp:2323-2620`
No PIN length validation in the `setPin` handler:
```cpp
// PIN is decrypted and verified, but no length check
if (!pin_storage_verify_fido2_hash(decrypted_pin_hash)) {
    g_client_pin.pin_retries--;
    LOG_W("PIN", "Invalid PIN, retries left: %d", g_client_pin.pin_retries);
    ...
}
```

**File**: `components/cdc_core/include/cdc_core/PinManager.h:35-36`
Badge PIN minimum is 4 digits, but this is only enforced in `setBadgePin()`:
```cpp
if (len < BADGE_PIN_MIN || len > BADGE_PIN_MAX) {
    LOG_E(TAG, "Badge PIN must be %d-%d digits", BADGE_PIN_MIN, BADGE_PIN_MAX);
    return false;
}
```

The FIDO2 `setPin` uses the badge PIN hash directly without calling `setBadgePin()`.

## Recommended Fix
Add PIN length validation in the FIDO2 `setPin` handler:

```cpp
// In ctap2.cpp, setPin handler
static uint8_t client_pin_set_pin(const uint8_t *params, uint16_t params_len, ...) {
    // ... existing code ...

    // After decryption, validate PIN length
    // Note: We only have the hash, so we need to check the original PIN
    // The PIN comes from the encrypted pinHashEnc, so we need to extract the PIN first
    
    // Option 1: Add length parameter to the encrypted payload
    // Option 2: Store minimum PIN length in FIDO2 storage and check during setPin
    
    // For now, add a check in the PIN verification path
    if (pin_length < 4) {  // Minimum badge PIN length
        response[0] = CTAP2_ERR_PIN_INVALID;
        *response_len = 1;
        return response[0];
    }
}
```

Alternatively, modify `pin_storage_verify_fido2_hash()` to accept a minimum length parameter and validate during FIDO2 operations.

## References
- FIDO2 CTAP 2.1 Specification - Client PIN
- FIDO2 WebAuthn Specification - PIN Protocol
- NIST SP 800-63B - DIGITAL IDENTITY GUIDELINES
