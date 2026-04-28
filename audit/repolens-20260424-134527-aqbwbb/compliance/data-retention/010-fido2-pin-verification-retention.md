---
title: "[LOW] FIDO2 PIN Verification State Persists Indefinitely"
severity: LOW
domain: compliance
lens: data-retention
labels:
  - "audit:compliance/data-retention"
---

## Summary
The FIDO2 module stores PIN verification state (`g_fido2.pin_verified`) in a global runtime structure that persists indefinitely after PIN verification. Once verified via ClientPIN protocol, the state remains active until explicitly cleared or power cycle, with no automatic expiration.

**Location**: `components/mod_fido2/src/fido2.cpp` (lines 26-33, 192-204)

## Impact
1. **Extended Authentication**: After entering PIN once, subsequent FIDO2 operations don't require re-verification
2. **Memory Retention**: PIN-verified state stays in RAM indefinitely
3. **No Auto-logout**: Background FIDO2 operations can proceed without fresh PIN entry
4. **Physical Access Risk**: If badge is unlocked and left unattended, FIDO2 operations continue without PIN

## Evidence
**Global state structure** (`fido2.cpp:26-33`):
```cpp
/** \brief Global FIDO2 runtime state. */

static struct {
    bool initialized;
    fido2_user_presence_cb_t user_presence_cb;
    TaskHandle_t task_handle;
    bool pin_verified;  // PIN was verified via ClientPIN protocol
} g_fido2 = {};
```

**Setting verified state** (`fido2.cpp:192-196`):
```cpp
void fido2_set_pin_verified(bool verified) {
    g_fido2.pin_verified = verified;
}
```

**Checking verified state** (`fido2.cpp:203-205`):
```cpp
bool fido2_is_pin_verified(void) {
    return g_fido2.pin_verified;
}
```

**Usage in CTAP2** (`ctap2.cpp:256-258`):
```cpp
bool pin_verified = fido2_is_pin_verified();
LOG_I("CTAP2", "Building authData: pin_verified=%d, cred_protect=%u", pin_verified, cred_protect);
if (pin_verified) {
    // Include user verified flag in authenticator data
}
```

**Manual clear** (`Fido2Ui.cpp:409`):
```cpp
fido2_set_pin_verified(false);  // Only cleared manually or on power cycle
```

**No automatic expiration** - searching for timeout:
- No `pinVerifiedTimeout` configuration
- No periodic check for session age
- No automatic clear based on inactivity

## Recommended Fix
Add PIN verification timeout and automatic expiration:

**Option 1: Add verification TTL**
```cpp
static constexpr uint32_t PIN_VERIFIED_TIMEOUT_MS = 5 * 60 * 1000;  // 5 minutes

static struct {
    bool initialized;
    fido2_user_presence_cb_t user_presence_cb;
    TaskHandle_t task_handle;
    bool pin_verified;
    uint32_t verifiedAtMs;  // New: timestamp when PIN was verified
} g_fido2 = {};

void fido2_set_pin_verified(bool verified) {
    g_fido2.pin_verified = verified;
    if (verified) {
        g_fido2.verifiedAtMs = esp_timer_get_time() / 1000;  // Track verification time
    }
}

bool fido2_is_pin_verified(void) {
    if (!g_fido2.pin_verified) return false;

    uint32_t nowMs = esp_timer_get_time() / 1000;
    uint32_t elapsed = nowMs - g_fido2.verifiedAtMs;

    if (elapsed > PIN_VERIFIED_TIMEOUT_MS) {
        g_fido2.pin_verified = false;  // Auto-clear expired verification
        return false;
    }

    return true;
}
```

**Option 2: Add tick-based check**
Call from FIDO2 module tick function:
```cpp
void Fido2Module::onTick(uint32_t nowMs) {
    checkPinVerifiedTimeout(nowMs);
    // ... rest of tick logic ...
}

static void checkPinVerifiedTimeout(uint32_t nowMs) {
    if (!fido2_is_pin_verified()) return;
    if (nowMs - g_fido2.verifiedAtMs > PIN_VERIFIED_TIMEOUT_MS) {
        fido2_set_pin_verified(false);
    }
}
```

**Option 3: Clear on specific events**
Auto-clear PIN verification on:
- FIDO2 credential list change
- PIN change command
- Lock screen activation

## References
- FIDO2 CTAP2 specification - PIN verification handling
- NIST SP 800-63B - Session inactivity timeout recommendations
- Common security best practices for hardware tokens
