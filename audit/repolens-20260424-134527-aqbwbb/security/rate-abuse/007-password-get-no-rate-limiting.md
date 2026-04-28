---
title: "[HIGH] PASSWORD_GET serial command prints passwords without rate limiting"
severity: HIGH
domain: rate-abuse
lens: rate-abuse-password
labels:
  - "audit:security/rate-abuse"
---

## Summary
The `PASSWORD_GET` serial command in `components/mod_password/src/PasswordModule.cpp` prints the full password entry (including the password itself) to the serial console without any rate limiting, authentication, or attempt tracking. When `FEATURE_SECURE_SERIAL=0` (default), this command is available to anyone with serial access.

**Location:** `components/mod_password/src/PasswordModule.cpp:218-246`

```cpp
static void cmd_password_get(const char* args) {
    char indexBuf[8] = {};
    const char* p = nextToken(args, indexBuf, sizeof(indexBuf));
    if (!p || !indexBuf[0]) {
        cdc::serial::Console::printf("Usage: PASSWORD_GET <index>\r\n");
        return;
    }
    uint16_t index = static_cast<uint16_t>(atoi(indexBuf));
    uint16_t slot = 0;
    if (!findSlotByIndex(index, &slot)) {
        cdc::serial::Console::printf("ERROR: invalid index\r\n");
        return;
    }
    PasswordEntry entry = {};
    if (!PasswordStore::instance().readEntry(slot, &entry)) {
        cdc::serial::Console::printf("ERROR: read failed\r\n");
        return;
    }
    cdc::serial::Console::printf("Title: %s\r\n", entry.title);
    cdc::serial::Console::printf("Username: %s\r\n", entry.username);
    cdc::serial::Console::printf("Password: %s\r\n", entry.password);  // Prints password!
    cdc::serial::Console::printf("URL: %s\r\n", entry.url);
    cdc::serial::Console::printf("TOTP Slot: %u\r\n", entry.totpSlot);
    cdc::serial::Console::printf("Notes: %s\r\n", entry.notes);
}
```

**Location:** `components/mod_password/src/PasswordModule.cpp:331-334`

```cpp
reg.registerCommand({
    "PASSWORD_GET", "Get password entry", cmd_password_get, CMD_MODULE, true
});
```

The command accepts any index and prints all fields including the password field directly to serial. There is no:
- Rate limiting on how often passwords can be retrieved
- Authentication check (works without PIN when `FEATURE_SECURE_SERIAL=0`)
- Attempt tracking or lockout
- Delay between requests

## Impact
**Brute-force enumeration of password indices:**
- Password entries are stored in R-Memory slots 150-511 (362 slots)
- An attacker can iterate through indices 0-361 to find all stored passwords
- With no rate limiting, all 362 passwords can be dumped in ~3 seconds at 100 commands/second
- Serial throughput at 115200 baud supports ~10 commands/second easily

**Password exposure:**
- Each call prints the full password to serial
- Serial output can be captured by:
  - USB CDC (anyone with physical access can read serial)
  - BLE serial passthrough (if enabled)
  - Log files on connected computer
- No masking or truncation of password field

**No lockout mechanism:**
- Unlike Badge PIN (3 retries + 60s lockout), password retrieval has no limits
- Unlike FIDO2 PIN (8 retries with persistence), no attempt tracking
- Attacker can try unlimited indices without any delay

**Comparison with other modules:**
- FIDO2 PIN: 8 retries, persisted to storage, 60s lockout
- Badge PIN: 3 retries, persisted to storage, 60s lockout
- GPG PIN: Uses Badge PIN (same 3 retries)
- Password GET: Unlimited attempts, no lockout, no persistence

## Evidence
- **File:** `components/mod_password/src/PasswordModule.cpp`
- **Lines 218-246:** `cmd_password_get()` - prints password without rate limiting
- **Line 238:** `cdc::serial::Console::printf("Password: %s\r\n", entry.password);`
- **Lines 331-334:** Command registration with no authentication flag
- **File:** `components/cdc_core/include/cdc_core/feature_flags.h`
- **Lines 19-21:** `FEATURE_SECURE_SERIAL` defaults to 0 (disabled)
- **File:** `main/CMakeLists.txt` - Build configuration shows FEATURE_SECURE_SERIAL not set by default

**Slot allocation from documentation:**
```
| Module | ECC Slots | R-Memory Slots |
|--------|-----------|----------------|
| Password | - | 150-511 |
```
This gives 362 potential password entries to enumerate.

**No rate limiting found in:**
- `cmd_password_get()` function
- `CommandRegistry::processCommand()` 
- Any middleware or wrapper
- PasswordStore class

## Recommended Fix
Implement rate limiting and authentication for password retrieval:

**Option 1: Enable FEATURE_SECURE_SERIAL by default**
```cpp
// In feature_flags.h
#ifndef FEATURE_SECURE_SERIAL
#define FEATURE_SECURE_SERIAL 1  // Changed from 0 to 1
#endif
```
This requires PIN authentication for all serial commands.

**Option 2: Add specific rate limiting for PASSWORD_GET**
```cpp
// Add to PasswordModule.cpp (around line 30)
static constexpr uint32_t PASSWORD_GET_RATE_LIMIT_MS = 2000;  // 2 seconds
static constexpr uint32_t PASSWORD_GET_MAX_PER_MINUTE = 10;
static uint32_t s_last_password_get_ms = 0;
static uint16_t s_password_get_count_min = 0;
static uint32_t s_password_get_window_start = 0;

// In cmd_password_get() (line 218)
static void cmd_password_get(const char* args) {
    uint32_t now = esp_timer_get_time() / 1000;  // ms
    
    // Rate limit: 2 seconds between requests
    if (now - s_last_password_get_ms < PASSWORD_GET_RATE_LIMIT_MS) {
        cdc::serial::Console::printf("Rate limit: wait 2 seconds\r\n");
        return;
    }
    s_last_password_get_ms = now;
    
    // Per-minute limit: max 10 passwords/minute
    if (now - s_password_get_window_start >= 60000) {
        s_password_get_window_start = now;
        s_password_get_count_min = 0;
    }
    if (s_password_get_count_min >= PASSWORD_GET_MAX_PER_MINUTE) {
        cdc::serial::Console::printf("Rate limit: max 10 passwords/minute\r\n");
        return;
    }
    s_password_get_count_min++;
    
    // ... rest of function
}
```

**Option 3: Mask password output by default**
```cpp
// Change line 238 to mask password
cdc::serial::Console::printf("Password: ");
if (entry.password[0]) {
    // Print masked version (e.g., "********" or first 2 chars)
    cdc::serial::Console::printf("********\r\n");
} else {
    cdc::serial::Console::printf("(empty)\r\n");
}
// Add a separate command to reveal password with confirmation
```

**Recommended implementation (Option 1 + 2):**
1. Change `FEATURE_SECURE_SERIAL` default to 1 in `feature_flags.h`
2. Add rate limiting to `cmd_password_get()` as shown above
3. Add a "reveal password" command that requires fresh PIN entry
4. Log all password retrieval attempts to NVS for audit trail

## References
- OWASP: [Rate Limiting Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Rate_Limiting_Cheat_Sheet.html)
- OWASP: [Authentication Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html)
- NIST SP 800-63B: Digital Identity Guidelines (Section 5.1.1: Memorized Secrets)
- CWE-307: Improper Restriction of Excessive Authentication Attempts
