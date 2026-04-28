---
title: "[LOW] GPG DEC key storage logs R-Memory slot numbers and status"
severity: LOW
domain: Privacy by Design
lens: PII-in-Logs
labels:
  - "audit:compliance/privacy-by-design"
---

## Summary
The GPG storage module logs information about DEC (Decryption) key storage operations, including R-Memory slot numbers. While the actual key material is encrypted, the logging reveals which slots contain keys and the status of operations.

**Location:** `components/mod_gpg/src/GpgStorage.cpp`

Key log statements:
- Line ~100: `LOG_I(TAG, "Saved encrypted DEC private key to R-Memory slot %d", rmem_slot);`
- Line ~150: `LOG_W(TAG, "No DEC private key in R-Memory slot %d", rmem_slot);`
- Line ~180: `LOG_I(TAG, "Deleted DEC private key from R-Memory slot %d", rmem_slot);`

## Impact
- **Slot Allocation Leakage:** Logs reveal which R-Memory slots are used for GPG keys, potentially allowing inference about key configuration.
- **Metadata Exposure:** While the key itself is encrypted, the presence/absence of keys in specific slots is logged.
- **Debug Information:** "Session key derived from PIN" and "Session cleared" logs reveal timing and state information about key operations.

## Evidence
**File:** `components/mod_gpg/src/GpgStorage.cpp`
**Log statements found:**
```cpp
LOG_I(TAG, "Saved encrypted DEC private key to R-Memory slot %d", rmem_slot);
LOG_W(TAG, "No DEC private key in R-Memory slot %d", rmem_slot);
LOG_I(TAG, "Deleted DEC private key from R-Memory slot %d", rmem_slot);
LOG_D(TAG, "Session key derived from PIN");
LOG_D(TAG, "Session cleared");
```

**File:** `components/mod_gpg/src/GpgStorage.cpp`
**Error logging:**
```cpp
LOG_E(TAG, "Failed to derive encryption key");
LOG_E(TAG, "GCM encrypt failed: %d", ret);
LOG_E(TAG, "Failed to write encrypted DEC key to R-Memory slot %d", rmem_slot);
LOG_E(TAG, "Failed to erase DEC key from R-Memory slot %d", rmem_slot);
```

## Recommended Fix
1. **Remove slot number from logs** or use generic references:
   ```cpp
   LOG_I(TAG, "Saved encrypted DEC private key");
   LOG_W(TAG, "No DEC private key found");
   LOG_I(TAG, "Deleted DEC private key");
   ```

2. **Remove debug-level session logs:**
   ```cpp
   // Remove or reduce verbosity:
   LOG_D(TAG, "Session key derived from PIN");
   LOG_D(TAG, "Session cleared");
   ```

3. **Add DEBUG_MODE guard** for detailed logging:
   ```cpp
   #ifdef DEBUG_MODE
   LOG_D(TAG, "Saved encrypted DEC private key to R-Memory slot %d", rmem_slot);
   #endif
   ```

## References
- GDPR Article 5(1)(c) - Data minimization
- Common logging best practices: Log only what is necessary for debugging
- Security best practices: Avoid logging metadata that reveals key configuration
