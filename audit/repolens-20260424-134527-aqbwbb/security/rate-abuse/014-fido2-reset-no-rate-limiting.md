---
title: "[HIGH] FIDO2 CTAP2 reset allows unlimited factory resets without rate limiting"
severity: HIGH
domain: rate-abuse
lens: rate-abuse-fido2
labels:
  - "audit:security/rate-abuse"
---

## Summary
The FIDO2 `authenticatorReset` command (CTAP2 reset) in `components/mod_fido2/src/ctap2.cpp` allows unlimited factory resets without any rate limiting. Each reset deletes all credentials (up to 32 ECC slots + R-Memory slots) and requires re-registration. While user presence is typically required, an attacker with physical access can:
1. Flood reset commands to wear out secure element storage
2. Force re-registration of all credentials (denial of service)
3. Delete credentials repeatedly to disrupt authentication

**Location:** `components/mod_fido2/src/ctap2.cpp:2951-2964`

```cpp
uint8_t ctap2_reset(uint8_t *response, uint16_t *response_len) {
    // Reset requires user presence within 10 seconds of power up
    // For now, just perform the reset
    if (!fido2_factory_reset()) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }

    response[0] = CTAP2_OK;
    *response_len = 1;
    LOG_I("CTAP2", "Factory reset complete");
    return CTAP2_OK;
}
```

**Location:** `components/mod_fido2/src/fido2.cpp:263-275`

```cpp
bool fido2_factory_reset(void) {
    LOG_W("FIDO2", "Factory reset requested");

    // Delete all credentials
    for (uint8_t slot = 0; slot < FIDO2_MAX_CREDENTIALS; slot++) {
        if (fido2_storage_slot_used(slot)) {
            fido2_storage_delete_credential(slot);  // Erases ECC + R-Memory
        }
    }

    LOG_I("FIDO2", "Factory reset complete");
    return true;
}
```

## Impact
**Storage wear-out:**
- Each reset iterates through 32 ECC slots (5-36 for FIDO2)
- Each credential deletion erases 1 ECC slot + 1 R-Memory slot
- TROPIC01 ECC slots have limited write/erase cycles (~100,000)
- TROPIC01 R-Memory has limited write/erase cycles (~100,000)
- An attacker can flood resets at CTAP2 throughput (~60 commands/second for HID)
- With 10 resets/second, ECC endurance could be exhausted in ~2.8 hours
- With 60 resets/second, ECC endurance could be exhausted in ~28 minutes

**Denial of service:**
- All credentials are deleted on reset
- User must re-register all credentials (up to 32)
- Each registration requires button press (user presence)
- Attacker can force user to re-register 32 credentials repeatedly
- Resets can be triggered faster than user can re-register

**Resource exhaustion:**
- Each credential deletion involves:
  - ECC slot erase (SPI write, ~10ms)
  - R-Memory erase (SPI write, ~10ms)
  - Metadata update (NVS write, ~5ms)
- 32 credentials = ~800ms per reset
- 100 resets = ~80 seconds CPU time
- Battery drain from continuous SPI operations

**Comparison with other reset operations:**
- GPG_RESET: Serial command, no rate limiting (see finding #013)
- Badge PIN reset: Requires physical button + PIN, slower
- FIDO2 reset: CTAP2 command, no rate limiting, fast execution
- FIDO2 reset: Requires user presence but no rate limiting between resets

## Evidence
- **File:** `components/mod_fido2/src/ctap2.cpp`
- **Lines 2951-2964:** `ctap2_reset()` - no rate limiting
- **Line 2954:** Calls `fido2_factory_reset()` which deletes all credentials
- **Line 2947:** Command documentation mentions CTAP2 `0x07` (authenticatorReset)
- **File:** `components/mod_fido2/src/fido2.cpp`
- **Lines 263-275:** `fido2_factory_reset()` - deletes all 32 credentials
- **Lines 267-271:** Loop through all FIDO2_MAX_CREDENTIALS slots
- **File:** `components/mod_fido2/include/mod_fido2/fido2.h`
- **Line 18:** `FIDO2_MAX_CREDENTIALS = 32`
- **File:** `components/mod_fido2/src/fido2_storage.cpp`
- **Lines:** `fido2_storage_delete_credential()` erases ECC + R-Memory

**No rate limiting found in:**
- `ctap2_reset()` function
- `fido2_factory_reset()` function
- `fido2_storage_delete_credential()` wrapper
- Any middleware or wrapper around CTAP2 commands
- `ctap2_init()` dispatcher (line 3406)

## Recommended Fix
Implement rate limiting for FIDO2 factory reset:

**Option 1: Per-day rate limit**
```cpp
// Add to ctap2.cpp (after line 100)
static constexpr uint32_t RESET_RATE_LIMIT_MS = 86400000;  // 1 per day
static constexpr uint8_t RESET_MAX_PER_WEEK = 7;
static uint32_t s_last_reset_ms = 0;
static uint8_t s_reset_count_week = 0;
static uint32_t s_reset_week_start = 0;

// In ctap2_reset() (line 2951)
uint8_t ctap2_reset(uint8_t *response, uint16_t *response_len) {
    uint32_t now = esp_timer_get_time() / 1000;  // ms
    
    // Rate limit: 1 reset per day
    if (now - s_last_reset_ms < RESET_RATE_LIMIT_MS) {
        response[0] = CTAP2_ERR_BUSY;
        *response_len = 1;
        return CTAP2_ERR_BUSY;
    }
    
    // Per-week limit: max 7 resets/week
    if (now - s_reset_week_start >= 604800000) {  // 7 days
        s_reset_week_start = now;
        s_reset_count_week = 0;
    }
    if (s_reset_count_week >= RESET_MAX_PER_WEEK) {
        response[0] = CTAP2_ERR_BUSY;
        *response_len = 1;
        return CTAP2_ERR_BUSY;
    }
    s_reset_count_week++;
    s_last_reset_ms = now;
    
    // ... rest of function
}
```

**Option 2: Persistent reset counter (survives power-cycle)**
```cpp
// Add to fido2_storage.cpp
#define FIDO2_RESET_SLOT 0  // Use special R-Memory slot for reset counter

bool fido2_factory_reset(void) {
    // Load reset counter from R-Memory
    uint32_t last_reset = load_reset_timestamp();
    uint8_t count = load_reset_count();
    
    // Check rate limit (1 per day)
    uint32_t now = get_unix_time();
    if (now - last_reset < 86400) {  // 1 day in seconds
        return false;  // Too soon
    }
    
    // Check weekly limit (max 7/week)
    if (count >= 7) {
        return false;
    }
    
    // Delete all credentials
    for (uint8_t slot = 0; slot < FIDO2_MAX_CREDENTIALS; slot++) {
        if (fido2_storage_slot_used(slot)) {
            fido2_storage_delete_credential(slot);
        }
    }
    
    // Update reset counter
    save_reset_timestamp(now);
    save_reset_count(count + 1);
    
    return true;
}
```

**Option 3: Per-RP ID limit**
```cpp
// Track resets per RP ID (for credential management)
static constexpr uint8_t MAX_RESETS_PER_RP = 2;

// In ctap2_reset()
uint8_t rp_count = fido2_storage_count_resets_for_rp(rp_id_hash);
if (rp_count >= MAX_RESETS_PER_RP) {
    response[0] = CTAP2_ERR_LIMIT_REACHED;
    *response_len = 1;
    return CTAP2_ERR_LIMIT_REACHED;
}
```

**Recommended implementation (Option 2):**
1. Add reset counter storage in R-Memory (slot 0 or dedicated slot)
2. Modify `fido2_factory_reset()` to check rate limits before deleting
3. Persist reset timestamp and count to survive power-cycles
4. Return `CTAP2_ERR_BUSY` for rate-limited requests
5. Add logging for rate limit events
6. Test with rapid-fire reset commands to verify rate limiting works

## References
- FIDO CTAP2 spec: [authenticatorReset](https://fidoalliance.org/specs/fido-v2.1-rd-20201209/fido-client-to-authenticator-protocol-v2.1-rd-20201209.html#authenticatorReset)
- FIDO CTAP2 spec: [Error codes](https://fidoalliance.org/specs/fido-v2.1-rd-20201209/fido-client-to-authenticator-protocol-v2.1-rd-20201209.html#error-codes) - CTAP2_ERR_BUSY
- TROPIC01 datasheet: [ECC slot endurance](https://www.tropic01.com/)
- OWASP: [Rate Limiting Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Rate_Limiting_Cheat_Sheet.html)
- CWE-400: [Uncontrolled Resource Consumption](https://cwe.mitre.org/data/definitions/400.html)

</content>