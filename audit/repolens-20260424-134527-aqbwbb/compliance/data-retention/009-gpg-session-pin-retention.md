---
title: "[LOW] GPG Session PIN State Persists Without Automatic Expiration"
severity: LOW
domain: compliance
lens: data-retention
labels:
  - "audit:compliance/data-retention"
---

## Summary
The GPG module stores session PIN state in memory (`s_storage.sessionActive`, `s_storage.sessionKey`) that persists indefinitely after PIN verification. The session remains active until explicitly cleared via `gpg_storage_clear_session()` or module reset, with no automatic expiration based on time or inactivity.

**Location**: `components/mod_gpg/src/GpgStorage.cpp` (lines 68-80, 455-487)

## Impact
1. **Extended Key Access**: Once a PIN is entered, the derived session key remains in memory indefinitely
2. **Memory Retention**: Session key (32 bytes) stays in RAM even when not actively using GPG functions
3. **No Auto-Lock**: Background tasks or other modules could potentially access GPG functions without re-authentication
4. **Power-loss Only Clear**: Session state only cleared on power cycle or manual clear

## Evidence
**Session state structure** (`GpgStorage.cpp:68-80`):
```cpp
static struct {
    bool ready = false;
    uint16_t eccStart = 0;
    uint16_t eccEnd = 0;
    uint16_t rmemStart = 0;
    uint16_t rmemEnd = 0;
    uint8_t sigSlot = 0;
    uint8_t decSlot = 0;
    uint8_t autSlot = 0;

    // Session state for verified PIN
    bool sessionActive = false;
    uint8_t sessionKey[32];  // HKDF-derived key from PIN
} s_storage;
```

**Session activation** (`GpgStorage.cpp:455-467`):
```cpp
void gpg_storage_set_session_pin(const char* pin) {
    if (!pin) {
        gpg_storage_clear_session();  // Manual clear only
        return;
    }

    // Derive session key from PIN
    if (derive_key_from_pin(pin, s_storage.sessionKey)) {
        s_storage.sessionActive = true;  // Session active indefinitely
    }
}
```

**Manual session clear** (`GpgStorage.cpp:485-487`):
```cpp
void gpg_storage_clear_session(void) {
    mbedtls_platform_zeroize(s_storage.sessionKey, sizeof(s_storage.sessionKey));
    s_storage.sessionActive = false;
}
```

**No automatic expiration** - searching for TTL or timeout:
- No `sessionTimeout` configuration
- No periodic check for session age
- No `expires_at` timestamp for session

## Recommended Fix
Add session timeout configuration and automatic expiration:

**Option 1: Add session TTL**
```cpp
static constexpr uint32_t DEFAULT_SESSION_TIMEOUT_MS = 10 * 60 * 1000;  // 10 minutes

static struct {
    // ... existing fields ...
    bool sessionActive = false;
    uint8_t sessionKey[32];
    uint32_t sessionStartMs;  // New: track when session started
} s_storage;

void gpg_storage_set_session_pin(const char* pin) {
    if (!pin) {
        gpg_storage_clear_session();
        return;
    }

    if (derive_key_from_pin(pin, s_storage.sessionKey)) {
        s_storage.sessionActive = true;
        s_storage.sessionStartMs = esp_timer_get_time() / 1000;  // Track start time
    }
}

bool gpg_storage_is_session_valid(void) {
    if (!s_storage.sessionActive) return false;

    uint32_t nowMs = esp_timer_get_time() / 1000;
    uint32_t elapsed = nowMs - s_storage.sessionStartMs;

    if (elapsed > DEFAULT_SESSION_TIMEOUT_MS) {
        gpg_storage_clear_session();  // Auto-expire
        return false;
    }

    return true;
}
```

**Option 2: Add serial command for session info**
```bash
GPG_SESSION_STATUS  # Show if session active and how long
GPG_SESSION_CLEAR   # Manually clear session
```

**Option 3: Configurable timeout via NVS**
```cpp
// Load timeout from NVS (default 10 minutes)
nvs_get_u32(nvs, "session_timeout", &sessionTimeoutMs);
```

## References
- NIST SP 800-63B - Session inactivity timeout recommendations (10-15 minutes for moderate security)
- Common security best practices for smart card/session management
