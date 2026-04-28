---
title: "[LOW] No rate limiting on serial command interface"
severity: LOW
domain: rate-abuse
lens: rate-abuse-serial
labels:
  - "audit:security/rate-abuse"
---

## Summary
The serial command interface in `components/serial_cmd/src/CommandRegistry.cpp` processes commands at maximum throughput without any rate limiting. Commands can be sent via USB CDC at ~115200 baud (~11,520 bytes/second), allowing rapid-fire execution.

**Location:** `components/serial_cmd/src/CommandRegistry.cpp:98-160`

```cpp
bool processCommand(const char* line) override {
    if (!line || !*line) return false;

    // Check line interceptor first (multiline input modes)
    if (lineInterceptor_ && lineInterceptor_(line)) {
        return true;
    }

    // Find command name (first word)
    char cmdBuf[64];
    ...
    
    // Find and execute command
    for (size_t i = 0; i < count_; i++) {
        if (strcasecmp(commands_[i].name, cmdBuf) == 0) {
            if (commands_[i].handler) {
                commands_[i].handler(line);  // Execute immediately!
            }
            if (onCommandExecuted_) {
                onCommandExecuted_();
            }
            return true;
        }
    }
    ...
}
```

## Impact
**Resource exhaustion:**
- Commands like `GPG_GENERATE`, `TOTP_ADD`, `FIDO2_STATUS` can be flooded
- Expensive operations (crypto, storage writes) can be triggered repeatedly
- USB CDC buffer can be overwhelmed, causing serial lockup

**Denial of service:**
- Rapid command execution can block the main loop
- Serial monitor becomes unresponsive
- Battery drain from continuous CPU usage

**Brute-force vectors:**
- PIN commands (`AUTH <pin>`) can be tried at ~100 attempts/second
- While Badge PIN has 3 retries + 60s lockout, the high throughput speeds up enumeration
- If FEATURE_SECURE_SERIAL is disabled, all commands are open to rapid execution

## Evidence
- **File:** `components/serial_cmd/src/CommandRegistry.cpp`
- **Lines 98-160:** `processCommand()` - no rate limiting
- **Lines 110-130:** PIN blocking check, but only when `FEATURE_SECURE_SERIAL` is enabled
- **File:** `components/cdc_core/include/cdc_core/feature_flags.h`
- **Line 17-22:** `FEATURE_SECURE_SERIAL` defaults to 0 (disabled)
- **File:** `main/CMakeLists.txt` - Build configuration shows FEATURE_SECURE_SERIAL not set by default

## Recommended Fix
Add rate limiting to the serial command interface:

**Option 1: Global rate limiter**
- Track commands per second (e.g., max 10 commands/second)
- Add static counter and timestamp in `CommandRegistry`
- Return early or delay if limit exceeded

**Option 2: Per-command rate limiting**
- Mark expensive commands with a `rateLimit` flag in `Command` struct
- Apply stricter limits to crypto/storage commands
- Example limits:
  - `GPG_GENERATE`: 1/minute
  - `TOTP_ADD`: 5/minute
  - `AUTH`: 3/minute (brute-force protection)

**Implementation steps (Option 1):**
1. Add to `CommandRegistry` class:
   ```cpp
   private:
       uint32_t lastCommandMs_ = 0;
       static constexpr uint32_t RATE_LIMIT_MS = 100;  // 10 commands/sec
   ```
2. Modify `processCommand()`:
   ```cpp
   uint32_t now = esp_timer_get_time() / 1000;
   if (now - lastCommandMs_ < RATE_LIMIT_MS) {
       Console::printf("Rate limit exceeded\r\n");
       return true;
   }
   lastCommandMs_ = now;
   ```
3. Add configuration option for rate limit in `feature_flags.h`

**Option 3: Enable FEATURE_SECURE_SERIAL by default**
- Change `feature_flags.h` to default `FEATURE_SECURE_SERIAL=1`
- All commands (except PING) require authentication
- Reduces attack surface significantly

## References
- FIDO2 CTAP2 spec: Command rate limiting best practices
- OWASP: [Rate Limiting Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Rate_Limiting_Cheat_Sheet.html)
