---
title: "[LOW] TOTP module logs slot numbers and account state"
severity: LOW
domain: Privacy by Design
lens: PII-in-Logs
labels:
  - "audit:compliance/privacy-by-design"
---

## Summary
The TOTP module logs slot numbers and basic account state information during add/update operations. While the actual secret is not logged, the slot numbers and account names reveal metadata about the TOTP configuration.

**Location:** `components/mod_totp/src/TotpStore.cpp`

Key log statements:
- Line ~211: `LOG_W(TAG, "No free TOTP slots");`
- Line ~260: `LOG_E(TAG, "Invalid Base32 secret");`
- Line ~290: `LOG_E(TAG, "Failed to write slot %u", slot);`

## Impact
- **Slot Allocation Leakage:** Logs reveal which slots are being accessed and when they are full.
- **Account Count Inference:** Repeated slot operations in logs allow inference about total number of accounts.
- **Debug Information:** While not critical, the logs provide metadata about TOTP usage patterns.

## Evidence
**File:** `components/mod_totp/src/TotpStore.cpp`
**Log statements found:**
```cpp
LOG_W(TAG, "No free TOTP slots");
LOG_E(TAG, "Invalid Base32 secret");
LOG_E(TAG, "Failed to write slot %u", slot);
LOG_E(TAG, "Failed to write slot %u", slot);  // in updateAccount
```

**File:** `components/mod_totp/src/TotpStore.cpp`
**Line 290 (addAccount):**
```cpp
if (res != cdc::hal::SeResult::OK) {
    LOG_E(TAG, "Failed to write slot %u", slot);
    return false;
}
```

**Line 348 (updateAccount):**
```cpp
if (res != cdc::hal::SeResult::OK) {
    LOG_E(TAG, "Failed to write slot %u", slot);
    return false;
}
```

## Recommended Fix
1. **Remove slot numbers from logs:**
   ```cpp
   LOG_E(TAG, "Failed to write TOTP account");
   ```

2. **Use generic error messages:**
   ```cpp
   LOG_W(TAG, "No free TOTP slots available");
   LOG_E(TAG, "Failed to store TOTP account");
   ```

3. **Add DEBUG_MODE guard** for detailed logging:
   ```cpp
   #ifdef DEBUG_MODE
   LOG_E(TAG, "Failed to write slot %u", slot);
   #endif
   ```

## References
- GDPR Article 5(1)(c) - Data minimization
- Common logging best practices: Log only what is necessary for debugging
- Security best practices: Avoid logging metadata that reveals configuration
