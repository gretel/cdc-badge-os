---
title: "[LOW] Password module logs slot numbers during CRUD operations"
severity: LOW
domain: Privacy by Design
lens: PII-in-Logs
labels:
  - "audit:compliance/privacy-by-design"
---

## Summary
The password module logs slot numbers during add and update operations. While the actual password data is not logged, the slot numbers reveal metadata about password entry storage and usage patterns.

**Location:** `components/mod_password/src/PasswordStore.cpp`

Key log statements:
- Line ~211: `LOG_W(TAG, "No free password slots");`
- Line ~243: `LOG_E(TAG, "Failed to write slot %u", slot);`
- Line ~291: `LOG_E(TAG, "Failed to write slot %u", slot);`

## Impact
- **Slot Allocation Leakage:** Logs reveal which slots are being accessed and when they are full.
- **Entry Count Inference:** Repeated slot operations in logs allow inference about total number of password entries.
- **Debug Information:** While not critical, the logs provide metadata about password usage patterns.

## Evidence
**File:** `components/mod_password/src/PasswordStore.cpp`
**Log statements found:**
```cpp
LOG_W(TAG, "No free password slots");
LOG_E(TAG, "Failed to write slot %u", slot);  // in addEntry
LOG_E(TAG, "Failed to write slot %u", slot);  // in updateEntry
```

**Line 243 (addEntry):**
```cpp
if (res != cdc::hal::SeResult::OK) {
    LOG_E(TAG, "Failed to write slot %u", slot);
    return false;
}
```

**Line 291 (updateEntry):**
```cpp
if (res != cdc::hal::SeResult::OK) {
    LOG_E(TAG, "Failed to write slot %u", slot);
    return false;
}
```

## Recommended Fix
1. **Remove slot numbers from logs:**
   ```cpp
   LOG_E(TAG, "Failed to write password entry");
   ```

2. **Use generic error messages:**
   ```cpp
   LOG_W(TAG, "No free password slots available");
   LOG_E(TAG, "Failed to store password entry");
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
