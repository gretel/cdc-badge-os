---
title: "[MEDIUM] TR01_ECC_DEL command lacks comprehensive slot validation"
severity: MEDIUM
domain: api-design/request-validation
lens: serial-command-validation
labels:
  - "audit:api-design/request-validation"
---

## Summary
The `TR01_ECC_DEL` serial command in `components/serial_cmd/src/SerialCmd.cpp:1020` validates that the slot number is within range but doesn't check if the slot is actually used before deletion. This can lead to confusing "Delete failed" messages when deleting empty slots.

**File**: `components/serial_cmd/src/SerialCmd.cpp`  
**Line**: 1020  
**Function**: `cmdTr01EccDel()`

## Impact
- **Confusing feedback**: Users get generic "Delete failed" when deleting empty slots
- **No pre-check**: Doesn't verify slot has data before attempting deletion
- **Missing context**: Should inform users which slots are actually used

## Evidence
From `components/serial_cmd/src/SerialCmd.cpp:1020-1038`:

```cpp
static void cmdTr01EccDel(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::ECC_SLOT_COUNT, "ECC slot");
    if (!result.valid) {
        Console::printf("Usage: TR01_ECC_DEL <slot>\r\n");
        return;
    }

    auto* se = getSecureElementWithCheck();
    if (!se) return;

    uint8_t slot = static_cast<uint8_t>(result.value);
    hal::SeResult seResult = se->eccDelete(slot);
    if (seResult == hal::SeResult::OK) {
        Console::printf("OK: ECC slot %d deleted\r\n", slot);
    } else {
        Console::printf("ERROR: Delete failed\r\n");  // Generic error!
    }
}
```

From `components/serial_cmd/src/SerialCmd.cpp:152-180` (parseSlotArg):
```cpp
static SlotParseResult parseSlotArg(const char* args, uint16_t maxSlot, const char* slotTypeName) {
    SlotParseResult result = {false, 0};

    if (!args || !*args) {
        Console::printf("Usage: Provide a %s number\r\n", slotTypeName);
        return result;
    }

    char* endptr = nullptr;
    long slotVal = strtol(args, &endptr, 10);

    if (endptr == args || *endptr != '\0' || slotVal < 0) {
        Console::printf("ERROR: Invalid %s number\r\n", slotTypeName);
        return result;
    }

    if (slotVal >= maxSlot) {
        Console::printf("ERROR: Invalid %s (0-%d)\r\n", slotTypeName, maxSlot - 1);
        return result;
    }

    result.valid = true;
    result.value = slotVal;
    return result;
}
```

The validation checks:
- Argument is provided
- Argument is a valid integer
- Value is non-negative
- Value is within range (0-31 for ECC slots)

But doesn't check:
- If the slot is actually used
- What type of data is in the slot
- Whether the slot can be safely deleted

## Recommended Fix
Add slot usage check before deletion:

```cpp
static void cmdTr01EccDel(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::ECC_SLOT_COUNT, "ECC slot");
    if (!result.valid) {
        Console::printf("Usage: TR01_ECC_DEL <slot>\r\n");
        return;
    }

    auto* se = getSecureElementWithCheck();
    if (!se) return;

    uint8_t slot = static_cast<uint8_t>(result.value);
    
    // Check if slot is actually used
    if (!se->eccSlotUsed(slot)) {
        Console::printf("INFO: Slot %d is already empty\r\n", slot);
        return;
    }
    
    hal::SeResult seResult = se->eccDelete(slot);
    if (seResult == hal::SeResult::OK) {
        Console::printf("OK: ECC slot %d deleted\r\n", slot);
    } else {
        Console::printf("ERROR: Delete failed (code: %d)\r\n", static_cast<int>(seResult));
    }
}
```

Similarly for `TR01_RMEM_DEL` (line 1043):
```cpp
static void cmdTr01RmemDel(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::RMEM_SLOT_COUNT, "R-Memory slot");
    if (!result.valid) {
        Console::printf("Usage: TR01_RMEM_DEL <slot>\r\n");
        return;
    }

    auto* se = getSecureElementWithCheck();
    if (!se) return;

    uint16_t slot = static_cast<uint16_t>(result.value);
    
    // Check if slot is actually used
    if (!se->rmemSlotUsed(slot)) {
        Console::printf("INFO: Slot %d is already empty\r\n", slot);
        return;
    }
    
    hal::SeResult seResult = se->rmemErase(slot);
    if (seResult == hal::SeResult::OK) {
        Console::printf("OK: R-Memory slot %d erased\r\n", slot);
    } else {
        Console::printf("ERROR: Erase failed (code: %d)\r\n", static_cast<int>(seResult));
    }
}
```

## References
- Secure element interface: `components/cdc_hal/include/cdc_hal/ISecureElement.h` (eccSlotUsed, rmemSlotUsed)
- Similar pattern in `components/serial_cmd/src/SerialCmd.cpp:973` (cmdTr01Slots showing slot usage)
