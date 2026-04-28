---
title: "[LOW] FIDO2 Relying-Party IDs Logged at DEBUG Level"
severity: LOW
domain: gdpr-dsgvo
lens: compliance/gdpr-dsgvo
labels:
  - "audit:compliance/gdpr-dsgvo"
---

## Summary
The FIDO2 module logs Relying-Party IDs (e.g., "github.com", "google.com", "ssh:server") at DEBUG level during initialization and credential searches. While logged at DEBUG level, these logs can still be captured via serial connection and reveal which online services the user has FIDO2 credentials for.

**File**: `components/mod_fido2/src/fido2_storage.cpp`

## Impact
**Privacy Leakage**: The list of Relying-Party IDs reveals which services the user uses and has FIDO2 credentials for. This is metadata that can be useful for profiling user habits.

**Debug-Only Protection**: DEBUG level logs are often not filtered in production builds, especially if `DEBUG_MODE` is enabled for development. The logs persist in serial output and can be captured.

**Limited Impact**: Since Relying-Party IDs are not highly sensitive (they're often public anyway), this is a LOW severity issue. However, it's still good practice to minimize PII in logs.

## Evidence

### Log Statement at Initialization
```cpp
// components/mod_fido2/src/fido2_storage.cpp:457
LOG_D("FIDO2", "Found credential %d: %s (curve=%d)", i, stored.rp_id, stored.curve);
```

### Log Statement During Resident Credential Search
```cpp
// components/mod_fido2/src/fido2_storage.cpp:538
LOG_D("FIDO2", "Slot %d: valid=%d resident=%d rp_match=%d rp=%s",
      i, g_storage.creds[i].valid, g_storage.creds[i].resident,
      rp_match, g_storage.creds[i].rp_id);
```

### Log Statement During Credential Creation
```cpp
// components/mod_fido2/src/fido2_storage.cpp:805
LOG_I("FIDO2", "Creating %s credential in slot %d for %s", curve_name, slot, rp_id);
```

### Log Statement During User ID Lookup
```cpp
// components/mod_fido2/src/fido2_storage.cpp:583
LOG_D("FIDO2", "Found existing credential in slot %d (empty user_id)", i);
```

## Recommended Fix

### Option 1: Reduce Verbosity of FIDO2 Logs (~1 hour)
Change the log level from `LOG_D` to a more trace-level output or remove the RP ID from the log message:

```cpp
// Before:
LOG_D("FIDO2", "Found credential %d: %s (curve=%d)", i, stored.rp_id, stored.curve);

// After:
LOG_D("FIDO2", "Found credential %d (curve=%d)", i, stored.curve);
// Or even better, use a trace level if available
```

### Option 2: Add Feature Flag for FIDO2 Debug Logs (~1 hour)
Wrap FIDO2 debug logs in a feature flag:

```cpp
// components/mod_fido2/src/fido2_storage.cpp
#ifdef DEBUG_FIDO2_VERBOSE
LOG_D("FIDO2", "Found credential %d: %s (curve=%d)", i, stored.rp_id, stored.curve);
#else
LOG_D("FIDO2", "Found credential %d (curve=%d)", i, stored.curve);
#endif
```

### Option 3: Truncate RP ID for Logs (~1 hour)
If the full RP ID is needed for debugging, truncate it:

```cpp
// components/mod_fido2/src/fido2_storage.cpp
static void log_credential(uint8_t slot, const char* rp_id, uint8_t curve) {
    char truncated[16];
    strncpy(truncated, rp_id, sizeof(truncated) - 1);
    truncated[sizeof(truncated) - 1] = '\0';
    LOG_D("FIDO2", "Found credential %d: %s... (curve=%d)", slot, truncated, curve);
}
```

### References
- **GDPR Art. 5(1)(b)**: "Purpose limitation" - Data should be collected for specified purposes
- **Best Practice**: Minimize PII in logs, especially metadata that reveals user behavior
- **Security**: Even DEBUG logs can be captured and analyzed for user profiling
