---
title: "[MEDIUM] FIDO2 storage module logs Relying Party (RP) IDs during credential iteration and creation"
severity: MEDIUM
domain: Privacy by Design
lens: PII-in-Logs
labels:
  - "audit:compliance/privacy-by-design"
---

## Summary
The FIDO2 storage module logs Relying Party (RP) IDs during credential initialization and creation operations. These log statements reveal which services have FIDO2 credentials stored on the device.

**Locations:** `components/mod_fido2/src/fido2_storage.cpp`

Key log statements:
- Line 457: `LOG_D("FIDO2", "Found credential %d: %s (curve=%d)", i, stored.rp_id, stored.curve);`
- Line 805: `LOG_I("FIDO2", "Creating %s credential in slot %d for %s", curve_name, slot, rp_id);`

## Impact
- **Service Discovery:** During storage initialization, all RP IDs are logged, revealing a complete list of services where the user has FIDO2 credentials.
- **Credential Creation Tracking:** When new credentials are created, the RP ID is logged at INFO level, showing which new services are being added.
- **Debug Output:** Even DEBUG-level logs may be captured during troubleshooting or when DEBUG_MODE is enabled.
- **Correlation with Other Logs:** Combined with logs from ctap2.cpp (issue 008), provides a complete authentication history.

## Evidence
**File:** `components/mod_fido2/src/fido2_storage.cpp`

**Line 457 (DEBUG level):**
```cpp
LOG_D("FIDO2", "Found credential %d: %s (curve=%d)", i, stored.rp_id, stored.curve);
```

This log is called during storage initialization when iterating over all credentials:
```cpp
for (uint8_t i = 0; i < FIDO2_MAX_CREDENTIALS; i++) {
    if (fido2_storage_slot_used(i)) {
        Fido2Credential stored = {};
        fido2_storage_get_credential(i, &stored);
        LOG_D("FIDO2", "Found credential %d: %s (curve=%d)", i, stored.rp_id, stored.curve);
    }
}
```

**Line 805 (INFO level):**
```cpp
LOG_I("FIDO2", "Creating %s credential in slot %d for %s", curve_name, slot, rp_id);
```

This log is called during `fido2_storage_create_credential()` when a new credential is being created.

## Recommended Fix
1. **Remove RP ID from storage logs:**
   ```cpp
   LOG_D("FIDO2", "Found credential %d (curve=%d)", i, stored.curve);
   ```

2. **Use hash of RP ID for debugging:**
   ```cpp
   uint32_t rp_hash = fnv1a_hash(rp_id);
   LOG_I("FIDO2", "Creating %s credential in slot %d (rp_hash=0x%08X)", curve_name, slot, rp_hash);
   ```

3. **Add DEBUG_MODE guard for detailed RP logging:**
   ```cpp
   #ifdef DEBUG_MODE
   LOG_I("FIDO2", "Creating %s credential in slot %d for %s", curve_name, slot, rp_id);
   #endif
   ```

4. **Reduce log level** for routine credential operations from INFO to DEBUG.

## References
- GDPR Article 5(1)(c) - Data minimization
- W3C WebAuthn Specification - Credential metadata handling
- FIDO Alliance - Privacy considerations for authentication
- Common logging best practices: Avoid logging authentication context

</content>