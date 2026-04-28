---
title: "[LOW] Session Data Persistence Without Automatic Expiry"
severity: LOW
domain: compliance
lens: data-retention
labels:
  - "audit:compliance/data-retention"
---

## Summary
Session data (PIN authentication state, GPG session keys) persists until explicitly cleared or lockout timer expires. No automatic session timeout based on inactivity.

**Location**: 
- PIN session: `components/cdc_core/src/PinManager.cpp` (lines 616-657)
- GPG session: `components/mod_gpg/src/GpgStorage.cpp` (lines 455-487)

## Impact
1. **Extended Access**: Session remains active indefinitely if user doesn't manually lock
2. **No Inactivity Timeout**: Badge could be left unlocked and unattended
3. **Security Risk**: Physical access to unlocked badge = full access to all data

## Evidence
**PIN lockout** (`PinManager.cpp:616-657`):
```cpp
void PinManager::startLockoutTimer() {
    lockoutStartMs_ = esp_timer_get_time() / 1000;
    lockoutActive_ = true;
}

// Lockout expires after 60 seconds (configurable)
bool PinManager::isLockoutActive() {
    if (!lockoutActive_) return false;
    uint32_t nowMs = esp_timer_get_time() / 1000;
    uint32_t elapsed = nowMs - lockoutStartMs_;
    if (elapsed >= LOCKOUT_DURATION_MS) {
        lockoutActive_ = false;  // Clear expired lockout
        return false;
    }
    return true;
}
```
**Note**: Lockout only triggers after failed PIN attempts, not after successful unlock + inactivity.

**GPG session** (`GpgStorage.cpp:455-487`):
```cpp
void gpg_storage_set_session_pin(const char* pin) {
    if (!pin) {
        gpg_storage_clear_session();  // Manual clear only
        return;
    }
    // Derive session key from PIN
    s_storage.sessionActive = true;
}

void gpg_storage_clear_session(void) {
    mbedtls_platform_zeroize(s_storage.sessionKey, sizeof(s_storage.sessionKey));
    s_storage.sessionActive = false;
}
```
Session persists until `gpg_storage_clear_session()` is called manually.

**No inactivity timer** - searching for idle timeout:
- No `idleTimeout` configuration
- No periodic check for inactivity
- No auto-lock after X minutes of no keypress

## Recommended Fix
Implement automatic session timeout:

**Option 1: Configurable idle timeout**
Add to settings:
```cpp
static constexpr uint32_t DEFAULT_IDLE_TIMEOUT_MS = 5 * 60 * 1000;  // 5 minutes
```

**Option 2: Periodic inactivity check**
In `ModuleRegistry::dispatchTick()`:
```cpp
void ModuleRegistry::dispatchTick(uint32_t nowMs) {
    // Check for idle timeout
    checkIdleTimeout(nowMs);
    
    for (uint8_t i = 0; i < count_; i++) {
        modules_[i]->onTick(nowMs);
    }
}

static void checkIdleTimeout(uint32_t nowMs) {
    static uint32_t lastActivityMs = 0;
    if (lastActivityMs == 0) lastActivityMs = nowMs;
    
    if (nowMs - lastActivityMs > DEFAULT_IDLE_TIMEOUT_MS) {
        // Auto-lock: clear sessions, return to lock screen
        clearAllSessions();
        showLockScreen();
    }
}
```

**Option 3: Activity tracking**
Track user activity (key presses, button clicks) to reset idle timer.

## References
- NIST SP 800-63B - Session inactivity timeout recommendations
- Common security best practices for hardware tokens
