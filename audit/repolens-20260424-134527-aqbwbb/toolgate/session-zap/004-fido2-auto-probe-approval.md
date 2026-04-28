---
title: "[LOW] FIDO2 Auto-Approves Browser Probe Requests"
severity: LOW
domain: cdc-badge-os
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

The FIDO2 module auto-approves browser probe requests with specific dummy RP IDs (`.dummy` and `make.me.blink`) without showing the user a prompt. While this is intentional for device discovery, it could be exploited if an attacker can inject these probe requests.

**Location:** `components/mod_fido2/src/Fido2Ui.cpp:488-495`

```cpp
// Auto-approve browser probe requests (dummy rp_ids used for device discovery)
// These are NOT real device selection - just "is a device there?"
if (action == FIDO2_ACTION_SELECT && rp_id) {
    if (strcmp(rp_id, ".dummy") == 0 || strcmp(rp_id, "make.me.blink") == 0) {
        LOG_I(TAG, "Auto-approving browser probe '%s'", rp_id);
        return FIDO2_UP_APPROVED;
    }
}
```

## Impact

**Security Impact:**
- **Minimal for current implementation** - only affects SELECT action (device discovery)
- Does NOT affect REGISTER or AUTHENTICATE actions
- Attacker would need to know the specific dummy RP IDs
- Primarily a convenience feature for browser compatibility

**Potential concerns:**
1. If an attacker can control the RP ID sent to the device, they could trigger auto-approval
2. The `.dummy` and `make.me.blink` RP IDs are hardcoded and discoverable
3. If extended to other actions (REGISTER/AUTH), could become a real vulnerability

**Current scope:**
- Only affects `FIDO2_ACTION_SELECT` (device discovery)
- Browser probes are typically silent and don't require user interaction anyway
- Most browsers use these for "is a FIDO device connected?" checks

## Evidence

1. **File:** `components/mod_fido2/src/Fido2Ui.cpp:488-495`
   - Shows auto-approval logic for specific RP IDs

2. **File:** `components/mod_fido2/src/Fido2Ui.cpp:460-486`
   - Shows the full `fido2_ui_user_presence_callback` function
   - SELECT action is handled separately from REGISTER/AUTH

3. **Context:**
   - `FIDO2_ACTION_SELECT` is used for device discovery
   - `FIDO2_ACTION_REGISTER` and `FIDO2_ACTION_AUTHENTICATE` still require user interaction
   - The callback properly handles these cases with full UI prompts

## Recommended Fix

**Option 1: Add a whitelist of known probe RP IDs**

```cpp
// Auto-approve browser probe requests (dummy rp_ids used for device discovery)
if (action == FIDO2_ACTION_SELECT && rp_id) {
    static const char* PROBE_RP_IDS[] = {".dummy", "make.me.blink", nullptr};
    for (int i = 0; PROBE_RP_IDS[i] != nullptr; i++) {
        if (strcmp(rp_id, PROBE_RP_IDS[i]) == 0) {
            LOG_I(TAG, "Auto-approving browser probe '%s'", rp_id);
            return FIDO2_UP_APPROVED;
        }
    }
}
```

**Option 2: Add a length check (prevent buffer overflow attempts)**

```cpp
// Auto-approve browser probe requests
if (action == FIDO2_ACTION_SELECT && rp_id) {
    // Only accept short probe RP IDs (max 15 chars)
    size_t len = strlen(rp_id);
    if (len <= 15 && (strcmp(rp_id, ".dummy") == 0 || strcmp(rp_id, "make.me.blink") == 0)) {
        LOG_I(TAG, "Auto-approving browser probe '%s'", rp_id);
        return FIDO2_UP_APPROVED;
    }
}
```

**Option 3: Add logging for unknown RP IDs**

```cpp
// Auto-approve browser probe requests
if (action == FIDO2_ACTION_SELECT && rp_id) {
    if (strcmp(rp_id, ".dummy") == 0 || strcmp(rp_id, "make.me.blink") == 0) {
        LOG_I(TAG, "Auto-approving browser probe '%s'", rp_id);
        return FIDO2_UP_APPROVED;
    } else {
        LOG_D(TAG, "Unknown probe RP ID: '%s'", rp_id);
    }
}
```

**Option 4: Document the behavior**

Add a comment explaining why this is safe:
```cpp
// Auto-approve browser probe requests (dummy rp_ids used for device discovery)
// These are NOT real device selection - just "is a device there?"
// Safe because:
// 1. Only affects SELECT action (device discovery)
// 2. REGISTER/AUTH still require full user interaction
// 3. Probe RP IDs are well-known and unlikely to collide with real RPs
if (action == FIDO2_ACTION_SELECT && rp_id) {
    if (strcmp(rp_id, ".dummy") == 0 || strcmp(rp_id, "make.me.blink") == 0) {
        LOG_I(TAG, "Auto-approving browser probe '%s'", rp_id);
        return FIDO2_UP_APPROVED;
    }
}
```

## References

- [FIDO2 CTAP2 Specification - SELECT](https://fidoalliance.org/specs/fido-v2.0-rd-20180130/fido-client-to-protocol-v2.0-rd-20180130.html)
- WebAuthn Device Discovery flow
- Common practice: Device discovery typically doesn't require user interaction
