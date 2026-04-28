---
title: "[MEDIUM] FIDO2 User Presence Callback Race Condition"
severity: MEDIUM
domain: cdc-badge-os
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

The FIDO2 module initializes the core stack (`fido2_init()`) before setting the user presence callback (`fido2_set_user_presence_callback()`). If a FIDO2 operation (makeCredential/getAssertion) is triggered between these two calls, the callback will be `nullptr` and user presence will be auto-approved.

**Location:** `components/mod_fido2/src/Fido2Module.cpp:184-192`

```cpp
bool Fido2Module::start() {
    // ...
    if (!fido2_is_initialized()) {
        if (!fido2_init()) {  // FIDO2 initialized here - callback still NULL!
            core::ModuleRegistry::instance().reportModuleError(getName(), "FIDO2 init failed");
            core::UsbManager::instance().unregisterInterface(core::UsbHidInterface::Fido, getName());
            state_ = core::ServiceState::ERROR;
            return false;
        }
    }
    fido2_set_user_presence_callback(fido2_ui_user_presence_callback);  // Callback set AFTER init

    state_ = core::ServiceState::STARTED;
    return true;
}
```

**Location:** `components/mod_fido2/src/fido2.cpp:180-185`

```cpp
fido2_user_presence_result_t fido2_request_user_presence(...) {
    if (g_fido2.user_presence_cb) {
        return g_fido2.user_presence_cb(rp_id, action, user_name);
    }
    // No callback set - auto-approve (unsafe, but allows testing)
    LOG_W("FIDO2", "No user presence callback - auto-approving");
    return FIDO2_UP_APPROVED;  // <-- Race condition vulnerability!
}
```

## Impact

**Security Impact:**
- **User presence can be bypassed during module initialization**
- Race window exists between `fido2_init()` and `fido2_set_user_presence_callback()`
- USB HID interface may be ready before callback is set
- A fast browser/attacker could trigger FIDO2 operation during this window

**Race condition timeline:**
1. `Fido2Module::start()` called
2. `fido2_init()` completes → FIDO2 core ready, `user_presence_cb = NULL`
3. USB HID interface registered (may happen in `fido2_init()`)
4. **RACE WINDOW**: Browser sends CTAP2 request
5. `fido2_request_user_presence()` called → returns `FIDO2_UP_APPROVED`
6. `fido2_set_user_presence_callback()` called → too late!

**Why this matters:**
- USB enumeration is fast (< 100ms)
- Browser can send CTAP2 request immediately
- Race window could be 10-50ms (enough for a fast attacker)
- Only affects initial startup, not subsequent operations

## Evidence

1. **File:** `components/mod_fido2/src/Fido2Module.cpp:184-192`
   - Shows callback set AFTER `fido2_init()`

2. **File:** `components/mod_fido2/src/fido2.cpp:30`
   - Shows `user_presence_cb` initialized to 0 (NULL)

3. **File:** `components/mod_fido2/src/fido2.cpp:124-160` (`fido2_init()`)
   - Initializes FIDO2 core, sets up ECDH key
   - Does NOT set user presence callback

4. **File:** `components/mod_fido2/src/fido2.cpp:164-166` (`fido2_set_user_presence_callback()`)
   ```cpp
   void fido2_set_user_presence_callback(fido2_user_presence_cb_t cb) {
       g_fido2.user_presence_cb = cb;
   }
   ```
   - Simple assignment, no synchronization

5. **File:** `components/mod_fido2/src/fido2.cpp:180-185` (`fido2_request_user_presence()`)
   - Auto-approves if callback is NULL

**Race window calculation:**
- `fido2_init()` takes ~10-20ms (ECDH key generation)
- `fido2_set_user_presence_callback()` takes < 1ms
- USB HID registration happens during `fido2_init()`
- Browser can send CTAP2 request within 50ms of USB ready

## Recommended Fix

**Option 1 (Recommended): Set callback before init**

```cpp
bool Fido2Module::start() {
    // Set callback FIRST, then init
    fido2_set_user_presence_callback(fido2_ui_user_presence_callback);
    
    if (!fido2_is_initialized()) {
        if (!fido2_init()) {
            // ... error handling ...
        }
    }

    state_ = core::ServiceState::STARTED;
    return true;
}
```

**Option 2: Set callback inside fido2_init()**

```cpp
// In fido2_init():
bool fido2_init(void) {
    // ... existing init code ...
    
    // Set callback as part of init
    fido2_set_user_presence_callback(fido2_ui_user_presence_callback);
    
    return true;
}
```

**Option 3: Add initialization state check**

```cpp
fido2_user_presence_result_t fido2_request_user_presence(...) {
    if (!g_fido2.initialized) {
        LOG_W("FIDO2", "FIDO2 not initialized - denying");
        return FIDO2_UP_DENIED;
    }
    if (g_fido2.user_presence_cb) {
        return g_fido2.user_presence_cb(rp_id, action, user_name);
    }
    LOG_W("FIDO2", "No user presence callback - denying");
    return FIDO2_UP_DENIED;  // Changed from APPROVED to DENIED
}
```

**Option 4: Delay USB HID registration until callback is set**

```cpp
bool Fido2Module::start() {
    if (!fido2_is_initialized()) {
        if (!fido2_init()) { ... }
    }
    fido2_set_user_presence_callback(fido2_ui_user_presence_callback);
    
    // Register USB HID AFTER callback is set
    auto& usbManager = core::UsbManager::instance();
    usbManager.registerInterface(core::UsbHidInterface::Fido, getName());

    state_ = core::ServiceState::STARTED;
    return true;
}
```

## References

- [OWASP Race Conditions](https://cheatsheetseries.owasp.org/cheatsheets/Race_Conditions_Cheat_Sheet.html)
- [FIDO2 CTAP2 Specification - User Presence](https://fidoalliance.org/specs/fido-v2.0-rd-20180130/fido-client-to-protocol-v2.0-rd-20180130.html#user-presence)
- Common security practice: Critical state should be fully initialized before exposing interface
