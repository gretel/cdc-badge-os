---
title: "[HIGH] GPG OpenPGP CCID interface allows unlimited PIN brute-force via VERIFY APDU"
severity: HIGH
domain: rate-abuse
lens: rate-abuse-gpg
labels:
  - "audit:security/rate-abuse"
---

## Summary
The GPG OpenPGP smart card interface in `components/mod_gpg/src/openpgp/openpgp.cpp` processes APDU `VERIFY` commands (INS=0x20) without any rate limiting. The CCID interface in `components/mod_gpg/src/openpgp/ccid.cpp` calls `openpgp_process_apdu()` which dispatches to `cmd_verify()` for PIN verification, allowing unlimited rapid-fire PIN attempts.

**Location:** `components/mod_gpg/src/openpgp/openpgp.cpp:1046-1114` (VERIFY command handler)

```cpp
static int cmd_verify(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    uint8_t pw_ref = apdu->p2;
    
    // Convert PIN data to null-terminated string
    char pin_str[OPENPGP_PIN_MAX_LEN + 1];
    memcpy(pin_str, apdu->data, apdu->lc);
    pin_str[apdu->lc] = '\0';

    // Verify PIN via TROPIC01 storage
    bool verified = false;
    if (pw_ref == 0x81 || pwdu->p2 == 0x82) {
        // PW1 (User PIN) verification
        verified = pin_storage_openpgp_verify_pw1(pin_str);
        retries = pin_storage_openpgp_pw1_retries();
    } else if (pw_ref == 0x83) {
        // PW3 (Admin PIN) verification
        verified = pin_storage_openpgp_verify_pw3(pin_str);
        retries = pin_storage_openpgp_pw3_retries();
    }
    
    if (verified) {
        return apdu_sw(resp, SW_OK);
    }
    return apdu_sw(resp, 0x63C0 | retries);  // Returns retries left
}
```

**Location:** `components/mod_gpg/src/openpgp/ccid.cpp:259-262`

```cpp
CCID_LOG(TAG, "  Calling openpgp_process_apdu...");
int resp_len = openpgp_process_apdu(apdu_data, apdu_len,
                                    resp_data, resp_data_max);
```

The CCID interface processes VERIFY APDUs at USB CCID throughput (~16ms intervals for 64-byte packets) with no rate limiting, no request throttling, and no per-connection tracking.

## Impact
**Brute-force attacks on GPG PINs:**
- PW1 (User PIN): 6-16 digits, 3 retries, persisted to storage
- PW3 (Admin PIN): 8-16 digits, 3 retries, persisted to storage
- Without rate limiting, an attacker can try all 3 retries in ~50ms (3 APDUs at 16ms each)
- After lockout, attacker can power-cycle the device to reset retries (similar to FIDO2 issue)

**Enumeration of PIN state:**
- `cmd_verify()` returns `0x63C0 | retries` on failure, revealing exact retry count
- APDU `GET DATA` commands can query PIN state without consuming retries
- Attacker can detect when PIN is correct (returns `SW_OK` = 0x9000)

**Resource exhaustion:**
- Each VERIFY APDU triggers `pin_storage_openpgp_verify_pw1/pw3()` which:
  - Computes KDF hash (SHA256 with iterations, ~100-500ms CPU time)
  - Persists retry count to TROPIC01 R-Memory (SPI write, ~10ms)
- An attacker can flood VERIFY commands to:
  - Deplete battery (1000 requests = ~100-500 seconds CPU time)
  - Wear out TROPIC01 R-Memory (limited to ~100,000 writes)
  - Block legitimate CCID operations

**Comparison with other interfaces:**
- Badge PIN: 3 retries + 60s lockout, persisted to storage
- FIDO2 PIN: 8 retries, not persisted (see finding #001), no rate limiting
- GPG PW1/PW3: 3 retries, persisted, **no rate limiting on CCID interface**
- Serial commands: Rate limiting depends on `FEATURE_SECURE_SERIAL` (default 0)

## Evidence
- **File:** `components/mod_gpg/src/openpgp/openpgp.cpp`
- **Lines 1046-1114:** `cmd_verify()` - no rate limiting
- **Line 1082:** `pin_storage_openpgp_verify_pw1(pin_str)` - KDF computation + storage write
- **Line 1093:** `pin_storage_openpgp_verify_pw3(pin_str)` - KDF computation + storage write
- **Lines 1108-1113:** Returns retry count on failure (enumeration oracle)
- **File:** `components/mod_gpg/src/openpgp/ccid.cpp`
- **Lines 259-262:** Calls `openpgp_process_apdu()` without rate limiting wrapper
- **File:** `components/mod_gpg/include/pin_storage.h`
- **Lines 11-12:** `pin_storage_openpgp_verify_pw1/pw3()` - no internal rate limiting
- **File:** `components/cdc_core/src/PinManager.cpp:413-428, 511-528`
- **Lines 425, 531:** Retry counter persisted to storage, but no rate limiting

**No rate limiting found in:**
- `cmd_verify()` function
- `openpgp_process_apdu()` dispatcher
- CCID `processRdrCommand()` function
- Any middleware or wrapper around VERIFY commands

## Recommended Fix
Implement rate limiting at the CCID/APDU level for VERIFY commands:

**Option 1: Per-connection rate limiting**
```cpp
// Add to openpgp.cpp (after line 145)
static constexpr uint32_t VERIFY_RATE_LIMIT_MS = 500;  // 1 per 500ms
static uint32_t s_last_verify_ms = 0;
static uint8_t s_verify_count_10s = 0;
static uint32_t s_verify_window_start = 0;

// In cmd_verify() (line 1046)
static int cmd_verify(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    uint32_t now = esp_timer_get_time() / 1000;  // ms
    
    // Rate limit: 500ms between VERIFY commands
    if (now - s_last_verify_ms < VERIFY_RATE_LIMIT_MS) {
        return apdu_sw(resp, SW_SECURITY_STATUS_NOT_SATISFIED);
    }
    s_last_verify_ms = now;
    
    // Per-10-second limit: max 10 VERIFY commands
    if (now - s_verify_window_start >= 10000) {
        s_verify_window_start = now;
        s_verify_count_10s = 0;
    }
    if (s_verify_count_10s >= 10) {
        return apdu_sw(resp, SW_SECURITY_STATUS_NOT_SATISFIED);
    }
    s_verify_count_10s++;
    
    // ... rest of function
}
```

**Option 2: Global CCID rate limiting**
```cpp
// Add to ccid.cpp (after line 100)
static constexpr uint32_t CCID_VERIFY_RATE_LIMIT_MS = 500;
static uint32_t s_last_ccid_verify_ms = 0;

// In processRdrCommand() before calling openpgp_process_apdu() (line 259)
if (apdu.ins == 0x20) {  // INS_VERIFY
    uint32_t now = esp_timer_get_time() / 1000;
    if (now - s_last_ccid_verify_ms < CCID_VERIFY_RATE_LIMIT_MS) {
        // Return CCID error for rate-limited VERIFY
        CCID_LOG_W(TAG, "Rate limit on VERIFY command");
        return CCID_HEADER_SIZE;  // Short response
    }
    s_last_ccid_verify_ms = now;
}
```

**Option 3: Session-based lockout**
```cpp
// Add to openpgp.cpp
static constexpr uint8_t MAX_VERIFY_PER_MINUTE = 10;
static uint8_t s_verify_count_min = 0;
static uint32_t s_verify_min_start = 0;
static uint32_t s_last_successful_verify = 0;

// In cmd_verify()
uint32_t now = esp_timer_get_time() / 1000;

// Reset counter every minute
if (now - s_verify_min_start >= 60000) {
    s_verify_min_start = now;
    s_verify_count_min = 0;
}

// Check per-minute limit
if (s_verify_count_min >= MAX_VERIFY_PER_MINUTE) {
    // Block for 10 seconds after max attempts
    if (now - s_last_successful_verify >= 10000) {
        s_verify_count_min = 0;  // Reset after cooldown
    } else {
        return apdu_sw(resp, SW_SECURITY_STATUS_NOT_SATISFIED);
    }
}
s_verify_count_min++;
```

**Recommended implementation (Option 1 + 3):**
1. Add rate limit constants and state to `openpgp.cpp` (lines 145-155)
2. Modify `cmd_verify()` to check rate limits before verification (lines 1046-1114)
3. Add per-minute counter with reset
4. Return `SW_SECURITY_STATUS_NOT_SATISFIED` (0x9400) for rate-limited requests
5. Add logging for rate limit events
6. Test with rapid-fire CCID VERIFY commands to verify rate limiting works

## References
- ISO 7816-4: [VERIFY command (0x20)](https://www.iso.org/standard/75652.html)
- OpenPGP Smart Card Application: [PIN verification](https://g10code.com/p-card.html)
- NIST SP 800-63B: [Authentication and Lifecycle Management](https://pages.nist.gov/800-63-3/sp800-63b.html)
- CWE-307: [Improper Restriction of Excessive Authentication Attempts](https://cwe.mitre.org/data/definitions/307.html)

</content>