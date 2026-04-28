---
title: "[LOW] Secure element session not closed in AttestationKeyService"
severity: LOW
domain: security
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

The `AttestationKeyService::ensureKey()` method starts a secure element session but never ends it. The session remains active until the next call or until the module is reset, potentially holding the secure element busy longer than necessary.

**Location:** `components/cdc_core/src/AttestationKeyService.cpp` lines 107-169

**Issue:** Session is started at line 108 but `sessionEnd()` is never called in the function.

## Impact

**Operational implications:**

1. **Resource holding**: The secure element session holds:
   - SPI/I2C bus resources
   - Secure element internal state
   - Potential power consumption

2. **Session timeout**: TROPIC01 sessions have a timeout - if the session is held too long without activity, it may expire anyway, but the badge doesn't know this and thinks the session is still active.

3. **Concurrency**: If other modules try to use the secure element while this session is "active" but idle, they may get incorrect `isSessionActive()` results.

4. **Memory leak (minor)**: The session state in `Tropic01Element` (variable `sessionActive_`) remains set until explicitly cleared.

**Context:** Looking at the code, `ensureKey()` is called periodically by `onTick()` (every 3 seconds max). If the session is never ended, it accumulates "idle session time" across multiple ticks.

**Note:** This is **LOW** severity because:
- The session will eventually timeout on the TROPIC01 side
- The session state is checked before use (`isSessionActive()`)
- No sensitive data is leaked by keeping the session open
- Other modules appear to handle their own sessions

## Evidence

**File: `components/cdc_core/src/AttestationKeyService.cpp`**

Session start (lines 107-111):
```cpp
if (!secureElement_->isSessionActive()) {
    if (!secureElement_->sessionStart()) {
        LOG_W(TAG, "Secure element session not active");
        return false;
    }
}
```

**No session end found in function:** The function ends at line 169 with `return true;` but no `sessionEnd()` call.

**Comparison:** Other modules properly end their sessions:

`serial_cmd/src/SerialCmd.cpp` lines 948-953:
```cpp
if (se->isSessionActive()) {
    LOG_I(TAG, "Ending session...");
    se->sessionEnd();
}

if (se->sessionStart()) {
    LOG_I(TAG, "Session started");
}
```

`TropicStorage.cpp` line 222-223 starts a session but also doesn't end it (similar issue).

## Recommended Fix

Add `sessionEnd()` calls at the end of the function:

**Option 1: Add cleanup at end of function**

```cpp
bool AttestationKeyService::ensureKey() {
    if (!secureElement_) {
        LOG_W(TAG, "Secure element not set");
        return false;
    }
    if (!secureElement_->isSessionActive()) {
        if (!secureElement_->sessionStart()) {
            LOG_W(TAG, "Secure element session not active");
            return false;
        }
    }

    // ... rest of function ...

    if (!saveStoredHash(hash, sizeof(hash))) {
        LOG_W(TAG, "Failed to store attestation key hash");
    }

    // Add session end before return
    secureElement_->sessionEnd();
    return true;
}
```

**Option 2: Use goto cleanup pattern**

```cpp
bool AttestationKeyService::ensureKey() {
    bool sessionStartedHere = false;
    
    if (!secureElement_) {
        LOG_W(TAG, "Secure element not set");
        return false;
    }
    if (!secureElement_->isSessionActive()) {
        if (!secureElement_->sessionStart()) {
            LOG_W(TAG, "Secure element session not active");
            return false;
        }
        sessionStartedHere = true;
    }

    // ... rest of function ...

cleanup:
    if (sessionStartedHere) {
        secureElement_->sessionEnd();
    }
    return true;
}
```

**Option 3: Scope-based cleanup**

```cpp
bool AttestationKeyService::ensureKey() {
    bool sessionStartedHere = false;
    
    // Start session if needed
    if (!secureElement_->isSessionActive()) {
        if (!secureElement_->sessionStart()) {
            LOG_W(TAG, "Secure element session not active");
            return false;
        }
        sessionStartedHere = true;
    }

    {  // Scope for secure element operations
        // ... all secure element operations ...
    }

    // End session if we started it
    if (sessionStartedHere) {
        secureElement_->sessionEnd();
    }
    return true;
}
```

**Recommended:** Option 1 is simplest and handles the common case. Option 2 is more robust for functions with multiple return paths.

**Additional fix:** Apply the same pattern to `TropicStorage.cpp` lines 222-223.

## References

- TROPIC01 datasheet - Session management
- [libtropic SDK](https://github.com/libtropic/libtropic)
- CWE-404: Improper Resource Shutdown or Release
