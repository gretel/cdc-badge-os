---
title: "[MEDIUM] No rate limiting on FIDO2 getAssertion operations"
severity: MEDIUM
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The FIDO2 `getAssertion` operation has no rate limiting or attempt counter. An attacker can repeatedly call `getAssertion` with different credentials or parameters without any delay or lockout mechanism.

**File**: `components/mod_fido2/src/ctap2.cpp:1700-1880`
```cpp
static uint8_t ctap2_get_assertion(const uint8_t *params, uint16_t params_len,
                                    uint8_t *response, uint16_t *response_len) {
    // Parse parameters
    GetAssertionParams p;
    uint8_t status = parse_get_assertion_params(&p, params, params_len);
    if (status != CTAP2_OK) return status;

    // Verify pinUvAuthParam if provided
    bool uv_verified = false;
    status = ga_verify_pin_auth(&p, &uv_verified);
    if (status != CTAP2_OK) return status;

    // Find credentials
    AssertionCredentials creds;
    ga_find_credentials(&p, &creds);

    // ... generate assertion ...
}
```

The function processes `getAssertion` requests without any attempt counter or delay mechanism.

## Impact
- **Brute-force Attack**: An attacker can try all possible RP ID hashes to enumerate credentials
- **Credential Guessing**: Attackers can test known credential IDs against different RP IDs
- **Denial of Service**: Rapid repeated calls could exhaust system resources
- **PIN Auth Brute-force**: If `pin_uv_auth_param` is provided, an attacker can try many different HMACs

## Evidence
**File**: `components/mod_fido2/src/ctap2.cpp:1700-1880`
No rate limiting in `getAssertion`:
```cpp
static uint8_t ctap2_get_assertion(const uint8_t *params, uint16_t params_len,
                                    uint8_t *response, uint16_t *response_len) {
    // ... process request ...
    // No attempt counter, no delay, no lockout
}
```

**File**: `components/mod_fido2/src/ctap2.cpp:1532-1582`
The `ga_verify_pin_auth` function validates the HMAC but doesn't track failed attempts:
```cpp
if (memcmp(p->pin_uv_auth_param, expected_hmac, compare_len) != 0) {
    LOG_W("CTAP2", "pinUvAuthParam verification failed");
    return CTAP2_ERR_PIN_AUTH_INVALID;  // No retry tracking
}
```

## Recommended Fix
Add a simple rate limiting mechanism:

```cpp
static struct {
    uint8_t assertion_count;
    uint64_t last_assertion_time_ms;
    uint8_t pin_auth_failures;
} g_ctap2 = {};

#define ASSERTION_RATE_LIMIT_MS  (5 * 1000)  // 5 seconds between assertions
#define MAX_PIN_AUTH_FAILURES    5

static uint8_t check_rate_limit(void) {
    uint64_t now = esp_timer_get_time() / 1000;
    
    if (g_ctap2.last_assertion_time_ms > 0) {
        uint32_t elapsed = now - g_ctap2.last_assertion_time_ms;
        if (elapsed < ASSERTION_RATE_LIMIT_MS) {
            return CTAP2_ERR_BUSY;  // Too many requests
        }
    }
    
    g_ctap2.last_assertion_time_ms = now;
    g_ctap2.assertion_count++;
    return CTAP2_OK;
}

static uint8_t ga_verify_pin_auth(const GetAssertionParams *p, bool *uv_verified) {
    // ... existing code ...
    
    if (memcmp(p->pin_uv_auth_param, expected_hmac, compare_len) != 0) {
        g_ctap2.pin_auth_failures++;
        if (g_ctap2.pin_auth_failures >= MAX_PIN_AUTH_FAILURES) {
            return CTAP2_ERR_PIN_AUTH_INVALID;  // Could add PIN_BLOCKED
        }
        return CTAP2_ERR_PIN_AUTH_INVALID;
    }
    
    // Reset on success
    g_ctap2.pin_auth_failures = 0;
    return CTAP2_OK;
}

static uint8_t ctap2_get_assertion(const uint8_t *params, uint16_t params_len,
                                    uint8_t *response, uint16_t *response_len) {
    // Check rate limit first
    uint8_t status = check_rate_limit();
    if (status != CTAP2_OK) return status;
    
    // ... rest of existing code ...
}
```

## References
- FIDO2 CTAP 2.1 Specification - GetAssertion
- OWASP Authentication Cheat Sheet - Rate Limiting
- NIST SP 800-63B - Authentication and Session Management

</content>