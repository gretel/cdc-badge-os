---
title: "[LOW] TR01_WIPE command lacks progress reporting for ECC key deletion"
severity: LOW
domain: database
lens: query-safety
labels:
  - audit:database/query-safety
---

## Summary
The `TR01_WIPE` command (line 1133-1188 in `components/serial_cmd/src/SerialCmd.cpp`) provides progress reporting for R-Memory erasure but not for ECC key deletion. With 32 ECC slots, the deletion can take noticeable time without user feedback.

**Evidence:**
- File: `components/serial_cmd/src/SerialCmd.cpp`
- Lines: 1158-1168 (ECC deletion - no progress)
- Lines: 1170-1183 (R-Memory deletion - has progress)
```cpp
// ECC keys - NO progress reporting
for (uint8_t i = 0; i < hal::ISecureElement::ECC_SLOT_COUNT; i++) {
    if (se->eccSlotUsed(i)) {
        if (se->eccDelete(i) == hal::SeResult::OK) {
            eccDeleted++;
        }
    }
}
Console::printf("  Deleted %d ECC keys\r\n", eccDeleted);

// R-Memory - HAS progress reporting
for (uint16_t i = 0; i < hal::ISecureElement::RMEM_SLOT_COUNT; i++) {
    if ((i & (WIPE_PROGRESS_INTERVAL - 1)) == 0) {
        Console::printf("  Progress: %d/%d\r\n", i, hal::ISecureElement::RMEM_SLOT_COUNT);
        Console::flush();
    }
    // ...
}
```

## Impact
- **User Experience**: User may think the device is frozen during ECC deletion
- **Inconsistent Feedback**: Mixed progress reporting within same operation
- **Timeout Risk**: Serial connection might timeout if no output for extended period

## Recommended Fix
Add progress reporting for ECC key deletion to match R-Memory pattern:

```cpp
// Delete all ECC keys
Console::printf("Erasing ECC keys...\r\n");
Console::flush();
for (uint8_t i = 0; i < hal::ISecureElement::ECC_SLOT_COUNT; i++) {
    if ((i & 7) == 0) {  // Report every 8 slots
        Console::printf("  ECC Progress: %d/%d\r\n", i, hal::ISecureElement::ECC_SLOT_COUNT);
        Console::flush();
    }
    if (se->eccSlotUsed(i)) {
        if (se->eccDelete(i) == hal::SeResult::OK) {
            eccDeleted++;
        }
    }
}
```

## References
- Similar pattern in same file: `cmdTr01Wipe` R-Memory loop (lines 1170-1183)
- WIPE_PROGRESS_INTERVAL constant defined at line 39
