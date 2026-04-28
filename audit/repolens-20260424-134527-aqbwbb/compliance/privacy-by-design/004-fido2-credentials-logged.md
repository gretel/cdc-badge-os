---
title: "[MEDIUM] FIDO2 credential data (RP ID, user name) logged during user presence prompts"
severity: MEDIUM
domain: Privacy by Design
lens: PII-in-Logs
labels:
  - "audit:compliance/privacy-by-design"
---

## Summary
The FIDO2 UI module logs relying party (RP) IDs and user names during user presence prompts. These log statements at lines 470-471 and 488 in `components/mod_fido2/src/Fido2Ui.cpp` output potentially sensitive authentication context.

**Location:** `components/mod_fido2/src/Fido2Ui.cpp`

```cpp
LOG_I(TAG, "User presence: action=%s, rp='%s', strBase=%u, promptActive=%d",
      actionStr, rp_id ? rp_id : "(null)", s_strIdBase, s_promptActive ? 0 : 1);
...
LOG_I(TAG, "Auto-approving browser probe '%s'", rp_id);
```

RP IDs (e.g., `github.com`, `example.com`) identify which services the user is authenticating to, and user names (when available) link credentials to specific accounts.

## Impact
- **Authentication Context Exposure:** Logs reveal which services the user has FIDO2 credentials for, providing insight into the user's online presence and accounts.
- **User Name Leakage:** When user names are available in credential info, they are logged alongside RP IDs.
- **Browser Probe Logging:** The "Auto-approving browser probe" log reveals device discovery activity, which may be useful for fingerprinting.
- **Debug Logs in Production:** Serial logs may be captured during troubleshooting, exposing authentication metadata.

## Evidence
**File:** `components/mod_fido2/src/Fido2Ui.cpp`
**Lines 470-471:**
```cpp
LOG_I(TAG, "User presence: action=%s, rp='%s', strBase=%u, promptActive=%d",
      actionStr, rp_id ? rp_id : "(null)", s_strIdBase, s_promptActive ? 1 : 0);
```

**Line 488-492:**
```cpp
// Auto-approve browser probe requests (dummy rp_ids used for device discovery)
if (action == FIDO2_ACTION_SELECT && rp_id) {
    if (strcmp(rp_id, ".dummy") == 0 || strcmp(rp_id, "make.me.blink") == 0) {
        LOG_I(TAG, "Auto-approving browser probe '%s'", rp_id);
```

**File:** `components/mod_fido2/src/Fido2Ui.cpp`
**Lines 142-147** (credential list building):
```cpp
if (strlen(info.user_name) > 0) {
    snprintf(s_labels[i], sizeof(s_labels[i]),
             "%.45s (%.45s)", info.rp_id, info.user_name);
} else {
    snprintf(s_labels[i], sizeof(s_labels[i]),
             "%.45s", info.rp_id);
}
```

While this is display logic (not logging), it shows that user_name and rp_id are readily available PII.

## Recommended Fix
1. **Remove or reduce RP ID logging** in user presence flow:
   ```cpp
   LOG_I(TAG, "User presence: action=%s, promptActive=%d", actionStr, s_promptActive ? 1 : 0);
   ```

2. **Add DEBUG_MODE guard** if RP ID logging is needed for development:
   ```cpp
   #ifdef DEBUG_MODE
   LOG_I(TAG, "User presence: action=%s, rp='%s'", actionStr, rp_id ? rp_id : "(null)");
   #endif
   ```

3. **Log only hash of RP ID** for debugging without exposing actual values:
   ```cpp
   uint32_t rp_hash = fnv1a_hash(rp_id ? rp_id : "");
   LOG_I(TAG, "User presence: action=%s, rp_hash=0x%08X", actionStr, rp_hash);
   ```

4. **Review all FIDO2 logging** for other PII exposure in related files.

## References
- GDPR Article 5(1)(c) - Data minimization
- W3C WebAuthn Specification - Credential metadata handling
- FIDO Alliance - Privacy considerations for authentication
- Common logging best practices: Avoid logging authentication context
