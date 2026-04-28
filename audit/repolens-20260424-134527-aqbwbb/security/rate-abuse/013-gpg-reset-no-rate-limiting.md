---
title: "[MEDIUM] GPG_RESET serial command allows unlimited key resets without rate limiting"
severity: MEDIUM
domain: rate-abuse
lens: rate-abuse-gpg
labels:
  - "audit:security/rate-abuse"
---

## Summary
The `GPG_RESET` serial command in `components/mod_gpg/src/GpgModule.cpp` resets all GPG keys (SIG, DEC, AUT) and metadata without any rate limiting. While this is a destructive operation that requires confirmation in the UI, the serial command can be called repeatedly to:
1. Wear out the TROPIC01 secure element's ECC slot erase cycles
2. Wear out NVS storage (metadata erase + commit)
3. Force re-generation of keys (CPU-intensive)

**Location:** `components/mod_gpg/src/GpgModule.cpp:200-204`

```cpp
static void cmd_gpg_reset(const char* args) {
    (void)args;
    bool ok = gpg_reset();
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

**Location:** `components/mod_gpg/src/gpg.cpp:429-447`

```cpp
bool gpg_reset(void) {
    if (!gpg_storage_ready()) return false;
    se_delete_key(gpg_storage_sig_slot());    // ECC slot erase
    se_delete_key(gpg_storage_dec_slot());    // ECC slot erase
    se_delete_key(gpg_storage_aut_slot());    // ECC slot erase
    uint8_t zero_fp[GPG_FINGERPRINT_LEN] = {};
    openpgp_set_key_fingerprint(KEY_SIG, zero_fp, 0);
    openpgp_set_key_fingerprint(KEY_DEC, zero_fp, 0);
    openpgp_set_key_fingerprint(KEY_AUT, zero_fp, 0);
    memset(&s_metadata, 0, sizeof(s_metadata));
    s_initialized = false;
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_key(handle, NVS_KEY_META);  // NVS erase
        nvs_commit(handle);                   // NVS commit
        nvs_close(handle);
    }
    return true;
}
```

## Impact
**Secure element wear-out:**
- TROPIC01 ECC slots have limited erase cycles (typically ~100,000 writes)
- Each `GPG_RESET` erases 3 ECC slots (SIG, DEC, AUT)
- An attacker can flood `GPG_RESET` at serial throughput (~10-100 commands/second)
- 10,000 resets = 30,000 ECC erases = ~30% of ECC slot endurance
- With 100 resets/second, ECC endurance could be exhausted in ~17 minutes

**NVS storage wear-out:**
- NVS has limited write endurance (~100,000 writes per sector)
- Each `GPG_RESET` erases and commits the metadata key
- Repeated resets can wear out the NVS sector containing GPG metadata
- Once NVS is degraded, metadata may become corrupted or unreadable

**Resource exhaustion:**
- Each reset clears 3 ECC slots + NVS metadata
- Requires re-generation to restore functionality (see finding for GPG_GENERATE)
- Attacker can force the user to re-generate keys repeatedly
- Battery drain from continuous CPU usage (ECC erase operations)

**Comparison with other reset operations:**
- FIDO2 reset: `FIDO2_RESET` command, no rate limiting (similar issue)
- Badge PIN reset: Requires physical button + PIN, slower
- GPG reset: Serial command, no rate limiting, fast execution

## Evidence
- **File:** `components/mod_gpg/src/GpgModule.cpp`
- **Lines 200-204:** `cmd_gpg_reset()` - no rate limiting
- **Lines 125-125:** Command registration with no authentication flag
- **File:** `components/mod_gpg/src/gpg.cpp`
- **Lines 429-447:** `gpg_reset()` - erases 3 ECC slots + NVS metadata
- **Line 431-433:** `se_delete_key()` called 3 times
- **Lines 441-444:** NVS erase + commit
- **File:** `components/cdc_core/include/cdc_core/feature_flags.h`
- **Lines 19-21:** `FEATURE_SECURE_SERIAL` defaults to 0 (disabled)

**No rate limiting found in:**
- `cmd_gpg_reset()` function
- `gpg_reset()` function
- `se_delete_key()` wrapper
- Any middleware or wrapper around GPG commands

## Recommended Fix
Implement rate limiting for GPG key reset:

**Option 1: Per-hour rate limit**
```cpp
// Add to GpgModule.cpp (after line 30)
static constexpr uint32_t GPG_RESET_RATE_LIMIT_MS = 3600000;  // 1 per hour
static constexpr uint8_t GPG_RESET_MAX_PER_DAY = 10;
static uint32_t s_last_reset_ms = 0;
static uint8_t s_reset_count_day = 0;
static uint32_t s_reset_day_start = 0;

// In cmd_gpg_reset() (line 200)
static void cmd_gpg_reset(const char* args) {
    uint32_t now = esp_timer_get_time() / 1000;  // ms
    
    // Rate limit: 1 reset per hour
    if (now - s_last_reset_ms < GPG_RESET_RATE_LIMIT_MS) {
        cdc::serial::Console::printf("Rate limit: wait 1 hour\r\n");
        return;
    }
    
    // Per-day limit: max 10 resets/day
    if (now - s_reset_day_start >= 86400000) {  // 24 hours
        s_reset_day_start = now;
        s_reset_count_day = 0;
    }
    if (s_reset_count_day >= GPG_RESET_MAX_PER_DAY) {
        cdc::serial::Console::printf("Rate limit: max 10 resets/day\r\n");
        return;
    }
    s_reset_count_day++;
    s_last_reset_ms = now;
    
    // ... rest of function
}
```

**Option 2: Cooldown after reset**
```cpp
// Add to GpgModule.cpp
static constexpr uint32_t RESET_COOLDOWN_MS = 60000;  // 1 minute cooldown
static uint32_t s_last_reset_ms = 0;

// In cmd_gpg_reset()
static void cmd_gpg_reset(const char* args) {
    uint32_t now = esp_timer_get_time() / 1000;
    
    // Check cooldown
    if (now - s_last_reset_ms < RESET_COOLDOWN_MS) {
        cdc::serial::Console::printf("Cooldown: wait %lds\r\n", 
            (RESET_COOLDOWN_MS - (now - s_last_reset_ms)) / 1000);
        return;
    }
    
    bool ok = gpg_reset();
    s_last_reset_ms = esp_timer_get_time() / 1000;
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

**Option 3: Enable FEATURE_SECURE_SERIAL by default**
```cpp
// In feature_flags.h
#ifndef FEATURE_SECURE_SERIAL
#define FEATURE_SECURE_SERIAL 1  // Changed from 0 to 1
#endif
```
This requires PIN authentication for all serial commands including GPG_RESET.

**Recommended implementation (Option 1 + 3):**
1. Change `FEATURE_SECURE_SERIAL` default to 1 in `feature_flags.h`
2. Add rate limit constants and state to `GpgModule.cpp`
3. Modify `cmd_gpg_reset()` to check rate limits before calling `gpg_reset()`
4. Add logging for rate limit events
5. Persist reset count to NVS for cross-restart tracking
6. Test with rapid-fire GPG_RESET commands to verify rate limiting works

## References
- TROPIC01 datasheet: [ECC slot endurance](https://www.tropic01.com/)
- ESP32 NVS documentation: [Write endurance](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)
- OWASP: [Rate Limiting Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Rate_Limiting_Cheat_Sheet.html)
- CWE-400: [Uncontrolled Resource Consumption](https://cwe.mitre.org/data/definitions/400.html)

</content>