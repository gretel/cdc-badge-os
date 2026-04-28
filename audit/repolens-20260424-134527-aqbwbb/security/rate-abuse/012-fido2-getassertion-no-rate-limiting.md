---
title: "[MEDIUM] FIDO2 getAssertion allows unlimited authentication requests"
severity: MEDIUM
domain: rate-abuse
lens: rate-abuse-fido2
labels:
  - "audit:security/rate-abuse"
---

## Summary
The FIDO2 `getAssertion` command in `components/mod_fido2/src/ctap2.cpp` allows unlimited authentication requests without any rate limiting. While user presence (button press) is required for `option_up=1`, an attacker can:
1. Flood with `getAssertion` requests (100s/second) to consume CPU/battery
2. Enumerate credentials via error responses
3. Trigger signature generation repeatedly (CPU-intensive)

**Location:** `components/mod_fido2/src/ctap2.cpp:1766-1894`

```cpp
uint8_t ctap2_get_assertion(const uint8_t *params, uint16_t params_len,
                             uint8_t *response, uint16_t *response_len) {
    // Step 1: Parse CBOR parameters
    GetAssertionParams p;
    uint8_t status = ga_parse_params(params, params_len, &p);
    // ...

    // Step 5: Request user presence (if required)
    if (p.option_up) {
        if (!wait_for_user_presence(p.rp_id, FIDO2_ACTION_AUTHENTICATE, NULL)) {
            response[0] = CTAP2_ERR_OPERATION_DENIED;
            return CTAP2_ERR_OPERATION_DENIED;
        }
    }

    // Step 9: Generate signature (CPU-intensive!)
    status = ga_sign_assertion(slot, auth_data, auth_data_len, p.client_data_hash,
                               signature, &sig_len);
    // ...
}
```

**Location:** `components/mod_fido2/src/ctap2.cpp:1896-1960`

The `getNextAssertion` command (for multiple credentials) also has no rate limiting and can be called repeatedly after `getAssertion`.

## Impact
**Resource exhaustion:**
- Each `getAssertion` triggers ECC signature generation (`ga_sign_assertion()`)
- ECC signing is CPU-intensive (~50-200ms per signature on ESP32-S3)
- An attacker can flood `getAssertion` to:
  - Deplete battery (100 signatures = ~5-20 seconds CPU time)
  - Block legitimate authentication requests
  - Cause thermal stress on the badge

**Credential enumeration:**
- Error responses reveal information:
  - `CTAP2_ERR_NO_CREDENTIALS` - RP ID has no credentials
  - `CTAP2_ERR_OPERATION_DENIED` - User presence failed (or no UP required)
  - `CTAP2_OK` - Valid credential found + signature generated
- Attacker can iterate through RP IDs to discover which ones have credentials
- Can enumerate client data hashes to detect duplicate logins

**Denial of service:**
- `getAssertion` state is stored in global `g_ctap2.assertion_*` variables
- Multiple rapid requests can overwrite state mid-operation
- `getNextAssertion` can be called without prior `getAssertion` (returns error)
- Attacker can flood with invalid requests to waste CPU cycles

**No rate limiting found:**
- `ctap2_get_assertion()` function
- `ctap2_get_next_assertion()` function
- `ga_sign_assertion()` signature generation
- Any middleware or wrapper around FIDO2 commands

## Evidence
- **File:** `components/mod_fido2/src/ctap2.cpp`
- **Lines 1766-1894:** `ctap2_get_assertion()` - no rate limiting
- **Lines 1896-1960:** `ctap2_get_next_assertion()` - no rate limiting
- **Lines 1860-1869:** `ga_sign_assertion()` - ECC signature generation (expensive)
- **Lines 1833-1839:** Credential lookup + sign count increment
- **Lines 1841-1858:** Authenticator data generation + signature
- **File:** `components/mod_fido2/src/fido2_storage.cpp`
- **Lines:** `fido2_storage_increment_sign_count()` - writes to R-Memory (SPI write)

**No rate limiting found in:**
- `ctap2_get_assertion()` function
- `ctap2_get_next_assertion()` function
- `ga_sign_assertion()` function
- `wait_for_user_presence()` function
- Any middleware or wrapper around FIDO2 commands

## Recommended Fix
Implement rate limiting for FIDO2 assertion commands:

**Option 1: Per-RP ID rate limiting**
```cpp
// Add to ctap2.cpp (after line 100)
static constexpr uint32_t ASSERTION_RATE_LIMIT_MS = 2000;  // 1 per 2 seconds
static constexpr uint8_t MAX_ASSERTIONS_PER_MINUTE = 20;
static uint32_t s_last_assertion_ms = 0;
static uint8_t s_assertion_count_min = 0;
static uint32_t s_assertion_window_start = 0;

// In ctap2_get_assertion() (line 1766)
uint8_t ctap2_get_assertion(const uint8_t *params, uint16_t params_len,
                             uint8_t *response, uint16_t *response_len) {
    uint32_t now = esp_timer_get_time() / 1000;  // ms
    
    // Rate limit: 2 seconds between assertions
    if (now - s_last_assertion_ms < ASSERTION_RATE_LIMIT_MS) {
        response[0] = CTAP2_ERR_BUSY;
        *response_len = 1;
        return CTAP2_ERR_BUSY;
    }
    s_last_assertion_ms = now;
    
    // Per-minute limit: max 20 assertions/minute
    if (now - s_assertion_window_start >= 60000) {
        s_assertion_window_start = now;
        s_assertion_count_min = 0;
    }
    if (s_assertion_count_min >= MAX_ASSERTIONS_PER_MINUTE) {
        response[0] = CTAP2_ERR_BUSY;
        *response_len = 1;
        return CTAP2_ERR_BUSY;
    }
    s_assertion_count_min++;
    
    // ... rest of function
}
```

**Option 2: Global FIDO2 rate limiting**
```cpp
// Add to ctap2.cpp
static constexpr uint32_t FIDO2_RATE_LIMIT_MS = 100;  // 10 commands/second
static uint32_t s_last_fido2_cmd_ms = 0;

// Wrap all FIDO2 commands
static bool checkFido2RateLimit() {
    uint32_t now = esp_timer_get_time() / 1000;
    if (now - s_last_fido2_cmd_ms < FIDO2_RATE_LIMIT_MS) {
        return false;
    }
    s_last_fido2_cmd_ms = now;
    return true;
}

// In ctap2_get_assertion()
if (!checkFido2RateLimit()) {
    response[0] = CTAP2_ERR_BUSY;
    *response_len = 1;
    return CTAP2_ERR_BUSY;
}
```

**Option 3: Signature-specific rate limiting**
```cpp
// Track signature count per minute
static constexpr uint8_t MAX_SIGNATURES_PER_MINUTE = 30;
static uint8_t s_sig_count_min = 0;
static uint32_t s_sig_window_start = 0;

// In ga_sign_assertion()
uint32_t now = esp_timer_get_time() / 1000;
if (now - s_sig_window_start >= 60000) {
    s_sig_window_start = now;
    s_sig_count_min = 0;
}
if (s_sig_count_min >= MAX_SIGNATURES_PER_MINUTE) {
    return CTAP2_ERR_BUSY;  // Or return cached signature
}
s_sig_count_min++;
```

**Recommended implementation (Option 1):**
1. Add rate limit constants and state to `ctap2.cpp` (lines 100-110)
2. Modify `ctap2_get_assertion()` to check rate limits (lines 1766-1894)
3. Add per-minute counter with reset
4. Modify `ctap2_get_next_assertion()` to use same rate limit
5. Return `CTAP2_ERR_BUSY` for rate-limited requests
6. Add logging for rate limit events
7. Test with rapid-fire assertion requests to verify rate limiting works

## References
- FIDO CTAP2 spec: [authenticatorGetAssertion](https://fidoalliance.org/specs/fido-v2.1-rd-20201209/fido-client-to-authenticator-protocol-v2.1-rd-20201209.html#authenticatorGetAssertion)
- FIDO CTAP2 spec: [authenticatorGetNextAssertion](https://fidoalliance.org/specs/fido-v2.1-rd-20201209/fido-client-to-authenticator-protocol-v2.1-rd-20201209.html#authenticatorGetNextAssertion)
- FIDO CTAP2 spec: [Error codes](https://fidoalliance.org/specs/fido-v2.1-rd-20201209/fido-client-to-authenticator-protocol-v2.1-rd-20201209.html#error-codes) - CTAP2_ERR_BUSY
- OWASP: [Rate Limiting Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Rate_Limiting_Cheat_Sheet.html)
- CWE-400: [Uncontrolled Resource Consumption](https://cwe.mitre.org/data/definitions/400.html)
