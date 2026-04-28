---
title: "[MEDIUM] FIDO2 makeCredential allows unlimited credential creation (up to 32)"
severity: MEDIUM
domain: rate-abuse
lens: rate-abuse-fido2
labels:
  - "audit:security/rate-abuse"
---

## Summary
The FIDO2 `makeCredential` command in `components/mod_fido2/src/ctap2.cpp` allows creating up to 32 credentials (one per ECC slot) without any rate limiting on how quickly they can be created. While user presence is required (button press), an attacker with physical access can rapidly create credentials to fill the slot space.

**Location:** `components/mod_fido2/src/ctap2.cpp:1179-1251`

```cpp
uint8_t ctap2_make_credential(const uint8_t *params, uint16_t params_len,
                               uint8_t *response, uint16_t *response_len) {
    // ... parse params ...
    
    // Step 7: Request user presence
    if (!wait_for_user_presence(p.rp_id, FIDO2_ACTION_REGISTER, p.user_name)) {
        response[0] = CTAP2_ERR_OPERATION_DENIED;
        return CTAP2_ERR_OPERATION_DENIED;
    }
    
    LOG_I("CTAP2", "User presence OK, creating credential...");
    
    // Step 8: Create credential and build response
    return create_credential_and_respond(&p, curve, response, response_len);
}
```

**Location:** `components/mod_fido2/include/mod_fido2/fido2.h:18`

```c
#define FIDO2_MAX_CREDENTIALS   32      // Max ECC slots
```

The command requires user presence (button press), but there is no:
- Rate limiting on how many credentials can be created per minute/hour
- Cooldown between credential creations
- Maximum credentials per RP ID
- Tracking of credential creation rate

## Impact
**Denial of service (slot exhaustion):**
- FIDO2 uses 32 ECC slots (slots 5-31, with some reserved)
- An attacker can fill all slots with fake credentials
- Legitimate users can't register new credentials until slots are cleared
- Requires `FIDO2_RESET` command or factory reset to clear

**Resource exhaustion:**
- Each credential creation involves:
  - ECC key generation (mbedtls_ecp_gen_key, ~100-500ms CPU time)
  - TROPIC01 ECC slot write (SPI write, ~10ms)
  - R-Memory metadata write (~10ms)
- Attacker can deplete battery by forcing 32 credential creations
- 32 creations = ~3-16 seconds CPU time + ~0.6 seconds SPI time

**Credential management overhead:**
- Each credential needs to be tracked in R-Memory
- RP ID hash storage (32 bytes per credential)
- User name storage (up to 64 bytes per credential)
- Filling slots with junk data wastes R-Memory space

**Comparison with other operations:**
- FIDO2 PIN verification: 8 retries, not persisted (see finding #001)
- GPG key generation: Serial command, requires authentication
- Password add: Serial command, requires authentication
- FIDO2 makeCredential: **No rate limiting, user presence only**

## Evidence
- **File:** `components/mod_fido2/src/ctap2.cpp`
- **Lines 1179-1251:** `ctap2_make_credential()` - no rate limiting
- **Line 1241:** `wait_for_user_presence()` - only protection is button press
- **Line 1250:** `create_credential_and_respond()` - creates new credential
- **File:** `components/mod_fido2/include/mod_fido2/fido2.h`
- **Line 18:** `FIDO2_MAX_CREDENTIALS = 32`
- **File:** `components/mod_fido2/src/fido2_storage.cpp`
- **Lines:** ECC slot and R-Memory writes for each credential

**No rate limiting found in:**
- `ctap2_make_credential()` function
- `create_credential_and_respond()` function
- Any middleware or wrapper
- Module-level tracking of credential creation rate

## Recommended Fix
Implement rate limiting for FIDO2 credential creation:

**Option 1: Per-minute rate limit**
```cpp
// Add to ctap2.cpp (after line 100)
static constexpr uint32_t MAKE_CRED_RATE_LIMIT_MS = 5000;  // 1 per 5 seconds
static constexpr uint8_t MAKE_CRED_MAX_PER_MINUTE = 10;
static uint32_t s_last_make_cred_ms = 0;
static uint8_t s_make_cred_count_min = 0;
static uint32_t s_make_cred_window_start = 0;

// In ctap2_make_credential() (line 1179)
uint8_t ctap2_make_credential(const uint8_t *params, uint16_t params_len,
                               uint8_t *response, uint16_t *response_len) {
    uint32_t now = esp_timer_get_time() / 1000;  // ms
    
    // Rate limit: 5 seconds between credential creations
    if (now - s_last_make_cred_ms < MAKE_CRED_RATE_LIMIT_MS) {
        response[0] = CTAP2_ERR_BUSY;
        *response_len = 1;
        return CTAP2_ERR_BUSY;
    }
    s_last_make_cred_ms = now;
    
    // Per-minute limit: max 10 credentials/minute
    if (now - s_make_cred_window_start >= 60000) {
        s_make_cred_window_start = now;
        s_make_cred_count_min = 0;
    }
    if (s_make_cred_count_min >= MAKE_CRED_MAX_PER_MINUTE) {
        response[0] = CTAP2_ERR_BUSY;
        *response_len = 1;
        return CTAP2_ERR_BUSY;
    }
    s_make_cred_count_min++;
    
    // ... rest of function
}
```

**Option 2: Global credential limit with cooldown**
```cpp
// Add to ctap2.cpp
static constexpr uint8_t MAX_CREDS_PER_HOUR = 20;
static uint8_t s_creds_per_hour = 0;
static uint32_t s_hour_start = 0;

// In ctap2_make_credential()
uint32_t now = esp_timer_get_time() / 1000;

// Reset counter every hour
if (now - s_hour_start >= 3600000) {
    s_hour_start = now;
    s_creds_per_hour = 0;
}

// Check hourly limit
if (s_creds_per_hour >= MAX_CREDS_PER_HOUR) {
    response[0] = CTAP2_ERR_BUSY;
    *response_len = 1;
    return CTAP2_ERR_BUSY;
}
s_creds_per_hour++;
```

**Option 3: Per-RP ID limit**
```cpp
// Add to ctap2.cpp
static constexpr uint8_t MAX_CREDS_PER_RP = 5;

// In ctap2_make_credential()
uint8_t rp_count = fido2_storage_count_credentials_for_rp(p.rp_id_hash);
if (rp_count >= MAX_CREDS_PER_RP) {
    response[0] = CTAP2_ERR_LIMIT_REACHED;
    *response_len = 1;
    return CTAP2_ERR_LIMIT_REACHED;
}
```

**Recommended implementation (Option 1 + 3):**
1. Add rate limit constants and state to `ctap2.cpp` (lines 100-110)
2. Modify `ctap2_make_credential()` to check rate limits (lines 1179-1251)
3. Add per-minute counter with reset
4. Add per-RP ID limit (max 5 credentials per RP)
5. Return `CTAP2_ERR_BUSY` for rate-limited requests
6. Add logging for rate limit events

## References
- FIDO CTAP2 spec: [authenticatorMakeCredential](https://fidoalliance.org/specs/fido-v2.1-rd-20201209/fido-client-to-authenticator-protocol-v2.1-rd-20201209.html#authenticatorMakeCredential)
- FIDO CTAP2 spec: [Error codes](https://fidoalliance.org/specs/fido-v2.1-rd-20201209/fido-client-to-authenticator-protocol-v2.1-rd-20201209.html#error-codes) - CTAP2_ERR_BUSY
- OWASP: [Rate Limiting Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Rate_Limiting_Cheat_Sheet.html)
- CWE-400: [Uncontrolled Resource Consumption](https://cwe.mitre.org/data/definitions/400.html)

</content>