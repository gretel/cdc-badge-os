---
title: "[HIGH] FIDO2 auto-approves user presence when callback not set (unsafe for production)"
severity: HIGH
domain: compliance
lens: consent-flows
labels:
  - fido2
  - auto-approval
  - security
---

## Summary

When the FIDO2 user-presence callback is not set, the `fido2_request_user_presence()` function **auto-approves** the request with a warning log. This allows FIDO2 authentication/registration to proceed without any user confirmation, which defeats the purpose of user-presence verification.

**Location:** `components/mod_fido2/src/fido2.cpp:169-186`

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

The comment even admits this is "unsafe" but allows it for testing purposes. However, there is no compile-time flag or runtime check to prevent this in production builds.

## Impact

1. **Silent auto-approval:** If the user-presence callback is not registered (e.g., due to a bug, missing module registration, or early boot), FIDO2 operations will silently auto-approve without the user knowing.

2. **No user confirmation:** The whole point of user-presence verification is to ensure the user physically approves each authentication. Auto-approving defeats this security feature.

3. **Hard to debug:** The warning log might be missed, especially if serial logging is not monitored.

4. **Production risk:** A developer might test with the callback unset, then forget to set it for production.

5. **FIDO2 spec violation:** FIDO2 requires user verification or user presence. Auto-approval doesn't satisfy either requirement.

## Evidence

**fido2.cpp:169-186** - Auto-approve fallback:
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
    return FIDO2_UP_APPROVED;  // <-- Auto-approves!
}
```

**Fido2Module.cpp:192** - Callback is set in `start()`:
```cpp
bool Fido2Module::start() {
    // ... USB interface registration ...
    
    if (!fido2_is_initialized()) {
        if (!fido2_init()) {
            // ... error handling ...
        }
    }
    fido2_set_user_presence_callback(fido2_ui_user_presence_callback);  // Set here
    
    state_ = core::ServiceState::STARTED;
    return true;
}
```

Potential race conditions:
1. If `fido2_init()` is called before `start()` (e.g., manually), the callback won't be set
2. If `start()` fails partway through, the callback might not be registered
3. If the module is re-initialized, the callback might be cleared

**ctap2.cpp:508-534** - User presence is critical for CTAP operations:
```cpp
static bool wait_for_user_presence(const char *rp_id, fido2_action_t action, const char *user_name) {
    LOG_I("CTAP2", "User presence required for %s at %s",
          action == FIDO2_ACTION_REGISTER ? "registration" : "authentication",
          rp_id ? rp_id : "unknown");

    fido2_user_presence_result_t result = fido2_request_user_presence(rp_id, action, user_name);
    // If result is FIDO2_UP_APPROVED due to auto-approve, user never saw a prompt!
}
```

## Recommended Fix

Make auto-approval a compile-time option with a strict default:

1. **Add a compile-time flag:**
```cpp
// In components/cdc_core/feature_flags.h
#ifndef FEATURE_FIDO2_AUTO_APPROVE
#define FEATURE_FIDO2_AUTO_APPROVE 0  // Default: false (strict mode)
#endif
```

2. **Update the auto-approve logic:**
```cpp
fido2_user_presence_result_t fido2_request_user_presence(
    const char *rp_id,
    fido2_action_t action,
    const char *user_name
) {
    if (g_fido2.user_presence_cb) {
        return g_fido2.user_presence_cb(rp_id, action, user_name);
    }

    #if FEATURE_FIDO2_AUTO_APPROVE
    // Auto-approve enabled (for testing)
    LOG_W("FIDO2", "No user presence callback - auto-approving (FEATURE_FIDO2_AUTO_APPROVE=1)");
    return FIDO2_UP_APPROVED;
    #else
    // Strict mode: return DENIED
    LOG_E("FIDO2", "No user presence callback - DENYING (FEATURE_FIDO2_AUTO_APPROVE=0)");
    return FIDO2_UP_DENIED;
    #endif
}
```

3. **Add a runtime check on module start:**
```cpp
bool Fido2Module::start() {
    // ... existing code ...
    
    fido2_set_user_presence_callback(fido2_ui_user_presence_callback);
    
    // Verify callback is working
    if (!fido2_is_initialized()) {
        core::ModuleRegistry::instance().reportModuleError(getName(), "FIDO2 init failed");
        return false;
    }
    
    // Optional: Test callback with a probe request
    // (Could be done during init to verify UI is ready)
    
    state_ = core::ServiceState::STARTED;
    return true;
}
```

4. **Add documentation:**
In the FIDO2 module documentation, clearly state:
- User-presence callback is required
- Auto-approve is a testing feature, disabled by default
- How to enable auto-approve for testing (`-DFEATURE_FIDO2_AUTO_APPROVE=1`)

## References

- FIDO2 WebAuthn spec: "The authenticator MUST verify user presence"
- FIDO Alliance: "User presence is the minimum requirement for FIDO authentication"
- OWASP: "Never auto-approve security-critical operations"

</content>