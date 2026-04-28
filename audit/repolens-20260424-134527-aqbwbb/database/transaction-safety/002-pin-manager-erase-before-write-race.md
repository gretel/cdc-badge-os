---
title: "[HIGH] Erase-before-write race condition in PinManager saveToStorage()"
severity: HIGH
domain: database/transaction-safety
lens: transaction-safety
labels:
  - "audit:database/transaction-safety"
---

## Summary

In `components/cdc_core/src/PinManager.cpp`, the `saveToStorage()` method performs an erase operation followed by a write operation on the same R-Memory slot without atomicity. If power is lost between these two operations, the PIN data slot becomes empty (all 0xFF) with no recovery mechanism.

**Affected location:** `PinManager::saveToStorage()` (lines 152-215), specifically:
- Line 203: `se->rmemErase(RMEM_SLOT_PIN);`
- Line 205-211: `se->rmemWrite(RMEM_SLOT_PIN, data, STORAGE_SIZE);`

## Impact

**Critical data loss scenario:**
1. PinManager saves PIN state (badge hash, PW1, PW3 salts/hashes, retry counts)
2. Erase completes (slot becomes 0xFF)
3. Power loss before/during write
4. On reboot: `loadFromStorage()` sees magic byte 0xFF instead of expected magic marker
5. Result: Falls back to defaults, but retry counts and lockout state are lost

This is particularly dangerous because:
- The PIN *hash* is still correct (stored in memory), but retry state is reset
- An attacker could trigger multiple PIN changes by forcing power loss after erases
- Lockout timers (`lockoutActive_`, `lockoutStartMs_`) are lost, potentially bypassing brute-force protection

## Evidence

**PinManager::saveToStorage()** (lines 198-212):
```cpp
// Erase existing data first
se->rmemErase(RMEM_SLOT_PIN);

hal::SeResult result = se->rmemWrite(RMEM_SLOT_PIN, data, STORAGE_SIZE);
if (result != hal::SeResult::OK) {
    LOG_E(TAG, "R-Memory write failed");
    return false;  // Slot is now EMPTY, no rollback possible!
}

LOG_I(TAG, "PINs saved to R-Memory slot %d", RMEM_SLOT_PIN);
return true;
```

**loadFromStorage()** (lines 96-110):
```cpp
hal::SeResult result = se->rmemRead(RMEM_SLOT_PIN, data, STORAGE_SIZE, &actualLen);
if (result != hal::SeResult::OK || actualLen != STORAGE_SIZE || data[0] != MAGIC_V3) {
    LOG_D(TAG, "No valid PIN data (len=%d, magic=0x%02X)", actualLen, data[0]);
    return false;  // Triggers default load, losing retry counts
}
```

The `loadDefaults()` function (lines 72-87) resets retry counters:
```cpp
badgeRetries_ = MAX_RETRIES;  // Reset!
pw1Retries_ = MAX_RETRIES;    // Reset!
pw3Retries_ = MAX_RETRIES;    // Reset!
```

## Recommended Fix

**Option 1: Write-only update (no pre-erase)**
R-Memory supports overwriting if the new data is the same size. Remove the explicit erase:
```cpp
bool PinManager::saveToStorage() {
    hal::ISecureElement* se = hal::getSecureElementInstance();
    if (!se || !se->isSessionActive()) {
        LOG_E(TAG, "SE session not active");
        return false;
    }

    uint8_t data[STORAGE_SIZE];
    size_t pos = 0;
    // ... fill data ...

    // Remove se->rmemErase() - let rmemWrite handle it internally if needed
    hal::SeResult result = se->rmemWrite(RMEM_SLOT_PIN, data, STORAGE_SIZE);
    if (result != hal::SeResult::OK) {
        LOG_E(TAG, "R-Memory write failed");
        return false;
    }

    LOG_I(TAG, "PINs saved to R-Memory slot %d", RMEM_SLOT_PIN);
    return true;
}
```

**Option 2: Checksum validation**
Add a checksum field to the storage format and validate on load:
```cpp
struct PinStorageV4 {
    uint8_t magic;
    // ... existing fields ...
    uint8_t checksum;  // XOR or CRC of all previous bytes
};
```

Then in `loadFromStorage()`:
```cpp
if (data[0] != MAGIC_V3 || !validateChecksum(data, actualLen)) {
    LOG_W(TAG, "PIN data corrupted, restoring defaults");
    return false;
}
```

## References

- TROPIC01 R-Memory datasheet: erase-before-write semantics
- NVS flash wear leveling and power-fail safety
- Latch-up protection in secure elements