---
title: "[HIGH] FIDO2 User Presence Auto-Approval When Callback Not Set"
severity: HIGH
domain: cdc-badge-os
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

The FIDO2 module auto-approves user presence requests when the callback function is not set, bypassing the physical button press requirement for FIDO2 authentication. This occurs in `components/mod_fido2/src/fido2.cpp:183-185`.

**Location:** `components/mod_fido2/src/fido2.cpp:175-186`

```cpp
fido2_user_presence_result_t fido2_request_user_presence(
    const char *rp_id,
    fido2_action_t action,
    const char *user_name
) {
    if (g_fido2.user_presence_cb) {
        return g_fido2.user_presence_cb(rp_id, action, user_name);
    }
    // No callback set - auto-approve (unsafe, but allows testing)
    LOG_W("FIDO2", "No user presence callback - auto-approving");
    return FIDO2_UP_APPROVED;
}
```

## Impact

**Security Impact:**
- **FIDO2/WebAuthn credentials can be created/used without physical confirmation**
- An attacker with serial/USB access can register credentials without the user pressing the physical button
- Breaks the fundamental security guarantee of FIDO2 (user presence verification)
- Allows silent credential registration during a physical attack

**Why it matters:**
- FIDO2 security model requires explicit user interaction (User Presence = UP)
- Without UP, a stolen device can silently register new credentials
- The comment even admits it's "unsafe, but allows testing"

## Evidence

1. **File:** `components/mod_fido2/src/fido2.cpp:183-185`
   - Shows auto-approval fallback with logging
2. **File:** `components/mod_fido2/src/fido2.cpp:30`
   - Shows `user_presence_cb` is a simple function pointer in global struct
3. **File:** `components/mod_fido2/src/Fido2Module.cpp:192`
   - Shows callback is set in `start()` method, but FIDO2 can be initialized before start

**Race condition scenario:**
```cpp
// In Fido2Module::start() (line 184-192):
if (!fido2_is_initialized()) {
    if (!fido2_init()) { ... }  // FIDO2 initialized here
}
fido2_set_user_presence_callback(fido2_ui_user_presence_callback);  // Callback set AFTER init
```

If `fido2_init()` is called before `fido2_set_user_presence_callback()`, there's a window where user presence is auto-approved.

## Recommended Fix

**Option 1 (Preferred): Return DENIED instead of APPROVED**

```cpp
fido2_user_presence_result_t fido2_request_user_presence(
    const char *rp_id,
    fido2_action_t action,
    const char *user_name
) {
    if (g_fido2.user_presence_cb) {
        return g_fido2.user_presence_cb(rp_id, action, user_name);
    }
    // No callback set - deny to force user presence
    LOG_W("FIDO2", "No user presence callback - denying");
    return FIDO2_UP_DENIED;  // Changed from APPROVED to DENIED
}
```

**Option 2: Add initialization check**

```cpp
// In fido2_request_user_presence():
if (!g_fido2.user_presence_cb) {
    // Check if module is fully initialized
    if (fido2_is_initialized()) {
        LOG_E("FIDO2", "User presence callback not set after init");
    }
    return FIDO2_UP_DENIED;
}
return g_fido2.user_presence_cb(rp_id, action, user_name);
```

**Option 3: Enforce callback registration in init()**

Make `fido2_init()` fail if the callback isn't set:
```cpp
bool fido2_init(void) {
    // ... existing init code ...
    
    // Require callback to be set before init completes
    if (!g_fido2.user_presence_cb) {
        LOG_E("FIDO2", "User presence callback required for init");
        return false;
    }
    return true;
}
```

## References

- [FIDO2 CTAP2 Specification - User Presence](https://fidoalliance.org/specs/fido-v2.0-rd-20180130/fido-client-to-protocol-v2.0-rd-20180130.html#user-presence)
- [WebAuthn Level 1 - User Presence](https://www.w3.org/TR/webauthn-1/#user-presence)
- NIST FIDO2 Guidelines: User presence is a required security property
