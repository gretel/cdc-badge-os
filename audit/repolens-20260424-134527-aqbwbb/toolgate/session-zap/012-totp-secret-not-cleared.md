---
title: "[MEDIUM] TOTP secrets not cleared from stack after use"
severity: MEDIUM
domain: security
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

The TOTP module stores decoded secrets in stack-allocated buffers but does not clear them after writing to secure element storage. The secrets persist in stack memory until overwritten by subsequent function calls.

**Locations:** `components/mod_totp/src/TotpStore.cpp`
- `addAccount()` - line 258-270
- `updateAccount()` - line 316-328

**Affected variables:**
- `secret[SECRET_LEN]` - Decoded TOTP secret (up to 64 bytes)
- `payload` structure containing the secret

## Impact

**Security implications:**

1. **Stack persistence**: TOTP secrets (typically 20-32 bytes for Base32-encoded keys) remain on the stack after the function returns:
   - `addAccount()` - secret stored at line 258
   - `updateAccount()` - secret stored at line 316

2. **Multiple copies**: The secret exists in multiple locations:
   - Stack buffer `secret[SECRET_LEN]`
   - `payload.secret` (also on stack)
   - Secure element R-Memory (persistent)

3. **Extended attack window**: An attacker with memory access can extract:
   - The raw secret bytes for offline TOTP generation
   - Account names and issuers from the payload
   - Configuration (digits, period, algorithm)

4. **Secret size**: TOTP secrets are typically:
   - 16 bytes (128-bit) to 32 bytes (256-bit)
   - Base32 encoded as 32-64 characters
   - Critical secret - anyone with the secret can generate valid TOTP codes

**Context:** TOTP secrets are the "master key" for time-based one-time passwords. Unlike PINs which can be changed and have retry limits, TOTP secrets are typically long-lived and have no built-in expiration.

## Evidence

**File: `components/mod_totp/src/TotpStore.cpp`**

`addAccount()` function (lines 255-275):
```cpp
uint8_t secret[SECRET_LEN];
int secretLen = base32Decode(secretBase32, secret, SECRET_LEN);
if (secretLen <= 0) {
    LOG_E(TAG, "Invalid Base32 secret");
    return false;
}

TotpPayload payload = {};
if (issuer) {
    strncpy(payload.issuer, issuer, sizeof(payload.issuer) - 1);
}
memcpy(payload.secret, secret, static_cast<size_t>(secretLen));
payload.secretLen = static_cast<uint8_t>(secretLen);
payload.digits = digits ? digits : DEFAULT_DIGITS;
payload.period = period ? period : DEFAULT_PERIOD;
payload.algorithm = algorithm;
payload.flags = 0;

// Write to secure element
auto res = se->rmemWriteWithHeader(slot, moduleId_, name, 0,
                                    reinterpret_cast<const uint8_t*>(&payload),
                                    sizeof(payload));
```

**No clearing code found:** The `secret` buffer and `payload` structure are never cleared with `memset()` before the function returns.

`updateAccount()` function (lines 311-330):
```cpp
uint8_t secret[SECRET_LEN];
int secretLen = base32Decode(secretBase32, secret, SECRET_LEN);
if (secretLen <= 0) {
    LOG_E(TAG, "Invalid Base32 secret");
    return false;
}

TotpPayload payload = {};
// ... populate payload ...
memcpy(payload.secret, secret, static_cast<size_t>(secretLen));
// ... write to secure element ...
```

Same issue - no clearing of `secret` or `payload`.

## Recommended Fix

Clear the secret buffers after they're written to secure element storage:

**Option 1: Add memset before return**

```cpp
bool TotpStore::addAccount(const char* name, const char* issuer, const char* secretBase32,
                           uint8_t digits, uint16_t period, TotpAlgorithm algorithm) {
    // ... existing code ...

    uint8_t secret[SECRET_LEN];
    int secretLen = base32Decode(secretBase32, secret, SECRET_LEN);
    // ... validation ...

    TotpPayload payload = {};
    // ... populate payload ...
    memcpy(payload.secret, secret, static_cast<size_t>(secretLen));
    payload.secretLen = static_cast<uint8_t>(secretLen);

    // Write to secure element
    auto res = se->rmemWriteWithHeader(slot, moduleId_, name, 0,
                                        reinterpret_cast<const uint8_t*>(&payload),
                                        sizeof(payload));

    // Clear secrets before returning
    memset(secret, 0, sizeof(secret));
    memset(&payload, 0, sizeof(payload));

    if (res != cdc::hal::SeResult::OK) {
        LOG_E(TAG, "Failed to write slot %u", slot);
        return false;
    }

    cdc::core::TropicStorage::instance().writeSlot(moduleId_, slot, name, 0);
    return true;
}
```

**Option 2: Use scope block**

```cpp
bool TotpStore::addAccount(...) {
    // ... validation ...

    {  // Start scope for secret buffers
        uint8_t secret[SECRET_LEN];
        int secretLen = base32Decode(secretBase32, secret, SECRET_LEN);
        // ... decode and validate ...

        TotpPayload payload = {};
        // ... populate ...
        memcpy(payload.secret, secret, static_cast<size_t>(secretLen));

        // Write to secure element
        auto res = se->rmemWriteWithHeader(slot, moduleId_, name, 0,
                                            reinterpret_cast<const uint8_t*>(&payload),
                                            sizeof(payload));

        // Clear before leaving scope
        memset(secret, 0, sizeof(secret));
        memset(&payload, 0, sizeof(payload));
    }  // Variables go out of scope

    // ... rest of function ...
}
```

**Option 3: Use helper function**

```cpp
static void clearTotpSecret(uint8_t* secret, uint8_t len, TotpPayload* payload) {
    memset(secret, 0, SECRET_LEN);
    memset(payload, 0, sizeof(TotpPayload));
}

// Use in addAccount() and updateAccount()
clearTotpSecret(secret, secretLen, &payload);
```

**Recommended:** Option 1 is simplest and most explicit. Clear both `secret` and `payload` to ensure no copies remain.

**Additional considerations:**

1. Clear `secretBase32` if it's a local copy (not passed by caller)
2. Consider using `volatile` for the memset to prevent compiler optimization
3. Apply same pattern to `updateAccount()` and any other functions that handle secrets

## References

- CWE-200: Exposure of Sensitive Information to an Unauthorized Actor
- CWE-312: Secure Data Removal
- [RFC 6238 - TOTP](https://datatracker.ietf.org/doc/html/rfc6238)
- [OATH TOTP Algorithm](https://en.wikipedia.org/wiki/Time-based_One-time_Password_algorithm)
