---
title: "[MEDIUM] TOTP_GET serial command generates codes without rate limiting"
severity: MEDIUM
domain: rate-abuse
lens: rate-abuse-totp
labels:
  - "audit:security/rate-abuse"
---

## Summary
The `TOTP_GET` serial command in `components/mod_totp/src/TotpModule.cpp` generates TOTP codes on-demand without any rate limiting. This allows an attacker to rapidly generate TOTP codes for brute-force testing against online services or to enumerate valid accounts.

**Location:** `components/mod_totp/src/TotpModule.cpp:288-315`

```cpp
static void cmd_totp_get(const char* args) {
    if (!args || !*args) {
        cdc::serial::Console::printf("Usage: TOTP_GET <index>\r\n");
        return;
    }
    uint16_t index = static_cast<uint16_t>(atoi(args));
    uint16_t slot = 0;
    if (!findSlotByIndex(index, &slot)) {
        cdc::serial::Console::printf("ERROR: index not found\r\n");
        return;
    }
    char code[9] = {};
    int8_t remaining = TotpStore::instance().generateCode(slot, code);  // Generates code!
    if (remaining < 0) {
        cdc::serial::Console::printf("ERROR: time not valid\r\n");
        return;
    }
    // Prints code to serial
    cdc::serial::Console::printf("%s (%ds)\r\n", code, remaining);
}
```

**Location:** `components/mod_totp/src/TotpModule.cpp:326`

```cpp
reg.registerCommand({"TOTP_GET", "Generate TOTP code by index", cmd_totp_get, CMD_MODULE, true});
```

The command accepts any index and generates a TOTP code without:
- Rate limiting on how often codes can be generated
- Authentication check (works without PIN when `FEATURE_SECURE_SERIAL=0`)
- Attempt tracking or lockout
- Delay between requests

## Impact
**TOTP brute-force attacks:**
- TOTP codes are 6-8 digits (1,000,000 to 100,000,000 combinations)
- An attacker can generate codes rapidly and test against online services
- With no rate limiting, thousands of codes can be generated per minute
- This enables offline brute-forcing of TOTP secrets if the attacker can capture codes

**Account enumeration:**
- Attacker can iterate through indices to discover valid TOTP accounts
- Invalid indices return "ERROR: index not found" (enumeration oracle)
- Valid indices return actual codes (confirms account exists)

**Resource exhaustion:**
- `generateCode()` performs HMAC-SHA computation (CPU-intensive)
- Can be triggered repeatedly to deplete battery or block other operations
- No protection against rapid-fire requests

**Comparison with other modules:**
- Badge PIN: 3 retries, persisted, 60s lockout
- FIDO2 PIN: 8 retries, not persisted (see finding #001)
- TOTP_GET: Unlimited attempts, no lockout, no persistence

## Evidence
- **File:** `components/mod_totp/src/TotpModule.cpp`
- **Lines 288-315:** `cmd_totp_get()` - generates code without rate limiting
- **Line 300:** `TotpStore::instance().generateCode(slot, code)` - crypto operation
- **Lines 320-327:** Command registration with no authentication flag
- **File:** `components/mod_totp/src/TotpStore.cpp`
- **Lines 480-506:** `generateCode()` - performs HMAC-SHA computation
- **File:** `components/cdc_core/include/cdc_core/feature_flags.h`
- **Lines 19-21:** `FEATURE_SECURE_SERIAL` defaults to 0 (disabled)

**No rate limiting found in:**
- `cmd_totp_get()` function
- `TotpStore::generateCode()` method
- `CommandRegistry::processCommand()`
- Any middleware or wrapper

## Recommended Fix
Implement rate limiting and authentication for TOTP code generation:

**Option 1: Enable FEATURE_SECURE_SERIAL by default**
```cpp
// In feature_flags.h
#ifndef FEATURE_SECURE_SERIAL
#define FEATURE_SECURE_SERIAL 1  // Changed from 0 to 1
#endif
```
This requires PIN authentication for all serial commands.

**Option 2: Add specific rate limiting for TOTP_GET**
```cpp
// Add to TotpModule.cpp (around line 98)
static constexpr uint32_t TOTP_GET_RATE_LIMIT_MS = 2000;  // 2 seconds
static constexpr uint8_t TOTP_GET_MAX_PER_MINUTE = 10;
static uint32_t s_last_totp_get_ms = 0;
static uint8_t s_totp_get_count_min = 0;
static uint32_t s_totp_get_window_start = 0;

// In cmd_totp_get() (line 288)
static void cmd_totp_get(const char* args) {
    uint32_t now = esp_timer_get_time() / 1000;  // ms
    
    // Rate limit: 2 seconds between requests
    if (now - s_last_totp_get_ms < TOTP_GET_RATE_LIMIT_MS) {
        cdc::serial::Console::printf("Rate limit: wait 2 seconds\r\n");
        return;
    }
    s_last_totp_get_ms = now;
    
    // Per-minute limit: max 10 codes/minute
    if (now - s_totp_get_window_start >= 60000) {
        s_totp_get_window_start = now;
        s_totp_get_count_min = 0;
    }
    if (s_totp_get_count_min >= TOTP_GET_MAX_PER_MINUTE) {
        cdc::serial::Console::printf("Rate limit: max 10 codes/minute\r\n");
        return;
    }
    s_totp_get_count_min++;
    
    // ... rest of function
}
```

**Option 3: Mask code output by default**
```cpp
// Change line 311 to mask code
cdc::serial::Console::printf("Code: ");
if (code[0]) {
    cdc::serial::Console::printf("******\r\n");  // Masked
} else {
    cdc::serial::Console::printf("(empty)\r\n");
}
// Add a separate command to reveal code with confirmation
```

**Recommended implementation (Option 1 + 2):**
1. Change `FEATURE_SECURE_SERIAL` default to 1 in `feature_flags.h`
2. Add rate limiting to `cmd_totp_get()` as shown above
3. Add a "reveal code" command that requires fresh PIN entry
4. Log all TOTP code retrieval attempts to NVS for audit trail

## References
- OWASP: [Rate Limiting Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Rate_Limiting_Cheat_Sheet.html)
- RFC 6238: [TOTP: Time-Based One-Time Password Algorithm](https://datatracker.ietf.org/doc/html/rfc6238)
- NIST SP 800-63B: Digital Identity Guidelines (Section 5.1.2: One-Time Passwords)
- CWE-307: Improper Restriction of Excessive Authentication Attempts
